/**
 * =============================================================================
 * D2Lobby2
 * Copyright (C) 2023 Nicholas Hastings
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 2.0 or later, as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As a special exception, you are also granted permission to link the code
 * of this program (as well as its derivative works) to "Dota 2," the
 * "Source Engine, and any Game MODs that run on software by the Valve Corporation.
 * You must obey the GNU General Public License in all respects for all other
 * code used.  Additionally, this exception is granted to all derivative works.
 */

#include <stdio.h>

#include "d2lobby.h"
#include "eventlog.h"
#include "httpmgr.h"
#include "lobbymgr.h"
#include "logger.h"
#include "pluginsystem.h"
#include "util.h"

#include <jansson.h>
#include "pb2json.h"

#include <inttypes.h>

#include <generated_proto/dota_gcmessages_msgid.pb.h>
#include <generated_proto/dota_usermessages.pb.h>

#include <igameeventsystem.h>
#include <tier1/fmtstr.h>


SH_DECL_HOOK0(IVEngineServer2, GetBuildVersion, const, 0, int);
SH_DECL_HOOK1_void(ISource2Server, ServerHibernationUpdate, SH_NOATTRIB, 0, bool);

SH_DECL_HOOK0_void(ISource2Server, GameServerSteamAPIActivated, SH_NOATTRIB, 0);

SH_DECL_HOOK4_void(IGameEventSystem, PostEventAbstract_Local, SH_NOATTRIB, 0, CSplitScreenSlot, GameEventHandle_t__ *, const void *, unsigned long);
SH_DECL_HOOK8_void(IGameEventSystem, PostEventAbstract, SH_NOATTRIB, 0, CSplitScreenSlot, bool, int, const unsigned char *, GameEventHandle_t__ *, const void *, unsigned long, NetChannelBufType_t);
SH_DECL_HOOK5_void(IGameEventSystem, PostEntityEventAbstract, SH_NOATTRIB, 0, const CBaseHandle &, GameEventHandle_t__ *, const void *, unsigned long, NetChannelBufType_t);

SH_DECL_HOOK7_void(ISource2GameClients, OnClientConnected, SH_NOATTRIB, 0, CEntityIndex, int, const char *, uint64, const char *, const char *, bool);
SH_DECL_HOOK6_void(ISource2GameClients, ClientDisconnect, SH_NOATTRIB, 0, CEntityIndex, int, int, const char *, uint64, const char *);
SH_DECL_HOOK3_void(ISource2Server, GameFrame, SH_NOATTRIB, 0, bool, bool, bool);

SH_DECL_HOOK1(IScriptManager, CreateVM, SH_NOATTRIB, 0, IScriptVM *, ScriptLanguage_t);
SH_DECL_HOOK1_void(IScriptManager, DestroyVM, SH_NOATTRIB, 0, IScriptVM *);

D2Lobby g_D2Lobby;

IVEngineServer2 *engine = nullptr;
ISource2Server *gamedll = nullptr;
ISource2GameClients *serverclients = nullptr;
IFileSystem *filesystem = nullptr;
IGameEventSystem *eventsys = nullptr;
IScriptVM *scriptvm = nullptr;
static IScriptManager *scriptmgr = nullptr;
ISteamHTTP* http = nullptr;

CSteamGameServerAPIContext steamctx;

static bool s_bLieAboutVersion = true;// false;

static ConVar d2lobby_enable_live_stats("d2lobby_enable_live_stats", "1");

class BaseAccessor : public IConCommandBaseAccessor
{
public:
	bool RegisterConCommandBase(ConCommandBase *pVar)
	{
		return META_REGCVAR(pVar);
	}
} s_BaseAccessor;

extern ConVar match_post_url;
void OnMatchPostURLChanged(IConVar *var, const char *pOldValue, float flOldValue)
{
	UTIL_LogToFile("match_post_url changed to \"%s\"\n", match_post_url.GetString());
}
ConVar match_post_url("match_post_url", "", FCVAR_RELEASE, "", OnMatchPostURLChanged);

PLUGIN_EXPOSE(D2Lobby, g_D2Lobby);
bool D2Lobby::Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();

	if (!InitGlobals(error, maxlen))
	{
		return false;
	}

	InitHooks();

	for (auto p : PluginSystems())
	{
		if (!p->OnLoad())
		{
			UTIL_MsgAndLog(MSG_TAG "%s subsystem failed to load!\n", p->GetName());
			return false;
		}
	}

	UTIL_LogToFile("D2Lobby plugin loaded\n");

	return true;
}

bool D2Lobby::InitGlobals(char *error, size_t maxlen)
{
	// For compat with GET_V_IFACE macros
	ISmmAPI *ismm = g_SMAPI;

	GET_V_IFACE_ANY(GetServerFactory, gamedll, ISource2Server, INTERFACEVERSION_SERVERGAMEDLL);
	GET_V_IFACE_ANY(GetServerFactory, serverclients, ISource2GameClients, SOURCE2GAMECLIENTS_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, engine, IVEngineServer2, INTERFACEVERSION_VENGINESERVER);
	GET_V_IFACE_ANY(GetEngineFactory, filesystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, eventsys, IGameEventSystem, GAMEEVENTSYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, scriptmgr, IScriptManager, VSCRIPT_INTERFACE_VERSION);

	ICvar *icvar;
	GET_V_IFACE_CURRENT(GetEngineFactory, icvar, ICvar, CVAR_INTERFACE_VERSION);
	g_pCVar = icvar;
	ConVar_Register(0, &s_BaseAccessor);

	return true;
}

void D2Lobby::InitHooks()
{
	int h;
	h = SH_ADD_HOOK(IVEngineServer2, GetBuildVersion, engine, SH_MEMBER(this, &D2Lobby::Hook_GetBuildVersion), false);
	m_GlobalHooks.push_back(h);

	h = SH_ADD_HOOK(ISource2Server, ServerHibernationUpdate, gamedll, SH_MEMBER(this, &D2Lobby::Hook_ServerHibernationUpdate), false);
	m_GlobalHooks.push_back(h);

	h = SH_ADD_HOOK(ISource2Server, ServerHibernationUpdate, gamedll, SH_MEMBER(this, &D2Lobby::Hook_ServerHibernationUpdate_Post), true);
	m_GlobalHooks.push_back(h);

	h = SH_ADD_HOOK(ISource2Server, GameServerSteamAPIActivated, gamedll, SH_MEMBER(this, &D2Lobby::Hook_GameServerSteamAPIActivated), false);
	m_GlobalHooks.push_back(h);

	h = SH_ADD_HOOK(IGameEventSystem, PostEventAbstract_Local, eventsys, SH_MEMBER(this, &D2Lobby::Hook_PostEventAbstract_Local), true);
	m_GlobalHooks.push_back(h);

	h = SH_ADD_HOOK(IGameEventSystem, PostEventAbstract, eventsys, SH_MEMBER(this, &D2Lobby::Hook_PostEventAbstract), true);
	m_GlobalHooks.push_back(h);

	h = SH_ADD_HOOK(IGameEventSystem, PostEntityEventAbstract, eventsys, SH_MEMBER(this, &D2Lobby::Hook_PostEntityEventAbstract), true);
	m_GlobalHooks.push_back(h);

	h = SH_ADD_HOOK(ISource2GameClients, OnClientConnected, serverclients, SH_MEMBER(this, &D2Lobby::Hook_OnClientConnected), true);
	m_GlobalHooks.push_back(h);

	h = SH_ADD_HOOK(ISource2GameClients, ClientDisconnect, serverclients, SH_MEMBER(this, &D2Lobby::Hook_ClientDisconnect), true);
	m_GlobalHooks.push_back(h);

	h = SH_ADD_HOOK(IScriptManager, CreateVM, scriptmgr, SH_MEMBER(this, &D2Lobby::Hook_CreateVM), true);
	m_GlobalHooks.push_back(h);

	h = SH_ADD_HOOK(IScriptManager, DestroyVM, scriptmgr, SH_MEMBER(this, &D2Lobby::Hook_DestroyVM), false);
	m_GlobalHooks.push_back(h);

	h = SH_ADD_HOOK(ISource2Server, GameFrame, gamedll, SH_MEMBER(this, &D2Lobby::Hook_GameFrame), true);
	m_GlobalHooks.push_back(h);
}

void D2Lobby::ShutdownHooks()
{
	for (auto &h : m_GlobalHooks)
	{
		SH_REMOVE_HOOK_ID(h);
	}

	m_GlobalHooks.clear();
}


bool D2Lobby::Unload(char *error, size_t maxlen)
{
	ShutdownHooks();

	UTIL_LogToFile("D2Lobby plugin unloaded\n");

	for (auto p : PluginSystems())
	{
		p->OnUnload();
	}

	return true;
}

static bool bNextVMIsMain = true;

IScriptVM *D2Lobby::Hook_CreateVM(ScriptLanguage_t lang)
{
	if (bNextVMIsMain)
	{
		bNextVMIsMain = false;
		scriptvm = META_RESULT_ORIG_RET(IScriptVM *);
	}
	return scriptvm;
}

void D2Lobby::Hook_DestroyVM(IScriptVM *pVM)
{
	scriptvm = nullptr;
	bNextVMIsMain = true;
	RETURN_META(MRES_IGNORED);
}

int D2Lobby::Hook_GetBuildVersion() const
{
	UTIL_MsgAndLog("GetBuildVersion (%s)\n", s_bLieAboutVersion ? "lying" : "honesty");
	if (s_bLieAboutVersion)
	{
		//s_bLieAboutVersion = false;
		RETURN_META_VALUE(MRES_SUPERCEDE, 0);
	}
	
	RETURN_META_VALUE(MRES_IGNORED, 0);
}

void D2Lobby::Hook_GameServerSteamAPIActivated()
{
	steamctx.Init();

	http = steamctx.SteamHTTP();

	UTIL_MsgAndLog("Found ISteamHTTP at %p.\n", http);

	match_post_url.SetValue(CommandLine()->ParmValue("+match_post_url", ""));

	json_t *pContainer = json_object();
	json_object_set_new(pContainer, "match_id", json_integer(0));
	json_object_set_new(pContainer, "status", json_string("startup"));
	json_object_set_new(pContainer, "ip", json_string(CommandLine()->ParmValue("-ip", "")));
	json_object_set_new(pContainer, "port", json_integer(CommandLine()->ParmValue("-ip", 0)));

	char *pszOutput = json_dumps(pContainer, JSON_COMPACT);
	json_decref(pContainer);

	UTIL_LogToFile("Sending startup message:\n%s\n", pszOutput);

	if (match_post_url.GetString()[0])
	{
		g_HTTPManager.PostJSONToMatchUrl(pszOutput);
	}

	free(pszOutput);

	RETURN_META(MRES_IGNORED);
}

void D2Lobby::Hook_GameFrame(bool, bool, bool)
{
	if (m_GameState == DOTA_GAMERULES_STATE_WAIT_FOR_PLAYERS_TO_LOAD)
	{
		static float flLastStatusMessageTime = 0.f;
		float curTime = Plat_FloatTime();
		if (curTime - 30.f > flLastStatusMessageTime)
		{
			engine->ServerCommand(CFmtStr("say %d/%d players connected\n", g_LobbyMgr.GetConnectedPlayerCount(), g_LobbyMgr.MatchPlayerCount()));
			
			if (g_LobbyMgr.GetConnectedPlayerCount() != g_LobbyMgr.MatchPlayerCount())
			{
				char awaitedPlayers[1024];
				g_LobbyMgr.GetNotConnectedPlayerNames(awaitedPlayers, sizeof(awaitedPlayers));

				char c;
				for (size_t i = 0; (c = awaitedPlayers[i]); ++i)
				{
					if (c == ';')
						awaitedPlayers[i] = ':';
				}

				engine->ServerCommand(CFmtStrN<1024>("say Waiting for: %s\n", awaitedPlayers));
			}

			flLastStatusMessageTime = curTime;
		}
	}

	if (m_ShutdownState == ShutdownState::PreShutdown)
	{
		if (g_HTTPManager.HasAnyPendingRequests())
			return;

		static ConVarRef tv_delay("tv_delay");

		// Don't stop before delay + 3m, even if all wrapped up
		float flMinShutdownTime = m_flPreShutdownStartTime + (3.f * 60.f) + tv_delay.GetFloat();
		float flMaxShutdownTime = m_flPreShutdownStartTime + (5.f * 60.f) + tv_delay.GetFloat();
		if (Plat_FloatTime() > flMaxShutdownTime)
		{
			UTIL_LogToFile("PostGameThink: Reached shutdown time, sending match data.\n");
			BeginShutdown();
			return;
		}

		if (Plat_FloatTime() > flMinShutdownTime)
		{
			UTIL_LogToFile("PostGameThink: shutting down.\n");
			BeginShutdown();
			return;
		}

		int clientCount = 0;
		const int maxClients = 64;
		for (int i = 1; i <= maxClients; ++i)
		{
			if (UTIL_IsPlayerConnected(i))
				++clientCount;
		}

		if (clientCount == 0 && Plat_FloatTime() > (m_flPreShutdownStartTime + tv_delay.GetFloat()))
		{
			UTIL_LogToFile("PostGameThink: No players are still connected, shutting down.\n");
			BeginShutdown();
		}
	}
	else if (m_ShutdownState == ShutdownState::ShuttingDown)
	{
		if (g_HTTPManager.HasAnyPendingRequests())
			return;

		engine->ServerCommand("quit\n");
	}
}

void D2Lobby::OnGCPlayerFailedToConnect(CMsgDOTAPlayerFailedToConnect &msg)
{
	UTIL_LogToFile("GameFrame: Timed out waiting for anyone to join, sending match data.\n");
	m_MatchData = json_object();
	json_object_set_new(m_MatchData, "match_id", json_integer(g_LobbyMgr.MatchId()));
	json_object_set_new(m_MatchData, "status", json_string("load_failed"));

	auto *pFailedPlayers = json_array();
	auto *pConnectedPlayers = json_array();

	for (auto &m : g_LobbyMgr.LobbyMembers())
	{
		bool bConnected = true;
		for (uint64 id : msg.abandoned_loaders())
		{
			if (id == m.id())
			{
				bConnected = false;
				break;
			}
		}

		if (bConnected)
		{
			for (uint64 id : msg.failed_loaders())
			{
				if (id == m.id())
				{
					bConnected = false;
					break;
				}
			}
		}

		if (bConnected)
		{
			json_array_append_new(pConnectedPlayers, json_integer(m.id()));
		}
		else
		{
			json_array_append_new(pFailedPlayers, json_integer(m.id()));
		}
	}

	json_object_set_new(m_MatchData, "failed_players", pFailedPlayers);
	json_object_set_new(m_MatchData, "connected_players", pFailedPlayers);

	SendMatchData();
	g_LobbyMgr.DeleteLobby();

	BeginShutdown();
}

void D2Lobby::OnLiveStatsUpdate(CMsgDOTALiveScoreboardUpdate &msg)
{
	if (!d2lobby_enable_live_stats.GetBool())
		return;

	json_t *pJson = parse_msg(&msg);
	json_object_set_new(pJson, "status", json_string("update"));
	json_object_set_new(pJson, "match_id", json_integer(g_LobbyMgr.MatchId()));

	char *pszOutput = json_dumps(pJson, JSON_COMPACT);

	UTIL_MsgAndLog("Sending live update:\n%s\n", pszOutput);

	if (match_post_url.GetString()[0])
	{
		g_HTTPManager.PostJSONToMatchUrl(pszOutput);
	}

	json_decref(pJson);
	free(pszOutput);
}

void D2Lobby::OnGCMatchSignOut(CMsgGameMatchSignOut &msg)
{
	json_t *pAdditionalMessages = json_array();

	if (msg.additional_msgs_size())
	{
		for (auto &m : msg.additional_msgs())
		{
			json_t *pMessage = json_object();
			switch (m.id())
			{
			case k_EMsgGCPlayerStatsMatchSignOut:
			{
				json_object_set_new(pMessage, "MsgType", json_string("PlayerStats"));
				CMsgSignOutPlayerStats msg;
				if (msg.ParsePartialFromArray(m.contents().data(), m.contents().size()))
				{
					json_object_set_new(pMessage, "MsgData", parse_msg(&msg));
				}
				break;
			}
			case k_EMsgSignOutCommunicationSummary:
			{
				json_object_set_new(pMessage, "MsgType", json_string("CommunicationSummary"));
				CMsgSignOutCommunicationSummary msg;
				if (msg.ParsePartialFromArray(m.contents().data(), m.contents().size()))
				{
					json_object_set_new(pMessage, "MsgData", parse_msg(&msg));
				}
				break;
			}
			}

			json_array_append_new(pAdditionalMessages, pMessage);
		}
		msg.mutable_additional_msgs()->Clear();
	}

	assert(m_MatchData == nullptr);
	m_MatchData = parse_msg(&msg);
	json_object_set_new(m_MatchData, "additional_msgs", pAdditionalMessages);
	json_object_set_new(m_MatchData, "status", json_string("completed"));
	json_object_set_new(m_MatchData, "match_id", json_integer(g_LobbyMgr.MatchId()));

	SendMatchData();

	m_flPreShutdownStartTime = Plat_FloatTime();
	m_ShutdownState = ShutdownState::PreShutdown;
}

void D2Lobby::SendMatchData()
{
	char *pszOutput = json_dumps(m_MatchData, JSON_COMPACT);

	UTIL_MsgAndLog("Sending match data:\n%s\n", pszOutput);

	if (match_post_url.GetString()[0])
	{
		g_HTTPManager.PostJSONToMatchUrl(pszOutput);
	}
	else
	{
		UTIL_MsgAndLog("Match url not set, saving match result to match_%" PRIu64 ".txt\n", g_LobbyMgr.MatchId());
		FILE *f = fopen(CFmtStr("match_%" PRIu64 ".txt", g_LobbyMgr.MatchId()), "w");
		fprintf(f, "%s", pszOutput);
		fclose(f);
	}

	json_decref(m_MatchData);

	free(pszOutput);
}

void D2Lobby::BeginShutdown()
{
	m_ShutdownState = ShutdownState::ShuttingDown;

	json_t *pContainer = json_object();
	json_object_set_new(pContainer, "match_id", json_integer(g_LobbyMgr.MatchId()));
	json_object_set_new(pContainer, "status", json_string("shutdown"));

	char *pszOutput = json_dumps(pContainer, JSON_COMPACT);
	json_decref(pContainer);

	UTIL_LogToFile("Sending shutdown message:\n%s\n", pszOutput);

	if (match_post_url.GetString()[0])
	{
		g_HTTPManager.PostJSONToMatchUrl(pszOutput);
	}

	free(pszOutput);
}

void D2Lobby::Hook_PostEventAbstract_Local(CSplitScreenSlot nSlot, GameEventHandle_t__ *pEvent, const void *pData, unsigned long nSize)
{
	g_EventLogger.OnGameEvent(pEvent->m_MessageID, pData);
}

void D2Lobby::Hook_PostEventAbstract(CSplitScreenSlot nSlot, bool bSendToServer, int nClientCount, const unsigned char *clients, GameEventHandle_t__ *pEvent, const void *pData, unsigned long nSize, NetChannelBufType_t)
{
	if (pEvent->m_MessageID == DOTA_UM_GamerulesStateChanged)
	{
		DOTA_GameState newState = (DOTA_GameState)((CDOTAUserMsg_GamerulesStateChanged *)pData)->state();
		for (auto p : PluginSystems())
		{
			p->OnDOTAGameStateChange(m_GameState, newState);
		}

		m_GameState = newState;

		if (newState == DOTA_GAMERULES_STATE_HERO_SELECTION && g_LobbyMgr.MatchId())
		{
			UTIL_LogToFile("Issuing command: tv_record \"replays/%" PRIu64 "\"\n", g_LobbyMgr.MatchId());
			engine->ServerCommand(CFmtStr("tv_record \"replays/%" PRIu64 "\"\n", g_LobbyMgr.MatchId()));
		}
	}

	g_EventLogger.OnGameEvent(pEvent->m_MessageID, pData, nClientCount, clients);
}

void D2Lobby::Hook_PostEntityEventAbstract(const CBaseHandle &, GameEventHandle_t__ *pEvent, const void *pData, unsigned long nSize, NetChannelBufType_t)
{
	g_EventLogger.OnGameEvent(pEvent->m_MessageID, pData);
}

void D2Lobby::Hook_OnClientConnected(CEntityIndex index, int userId, const char *pszName, uint64 xuid, const char *pszNetworkID,
	const char *pszAddress, bool bFakePlayer)
{
	CSteamID sid(xuid);
	g_LobbyMgr.OnPlayerConnected(sid);

	if (g_LobbyMgr.GetGameState() == DOTA_GAMERULES_STATE_WAIT_FOR_PLAYERS_TO_LOAD)
	{
		if (!bFakePlayer)
		{
			UTIL_MsgAndLog("Player connected \"%s\" (%" PRIu64 ")\n", pszName, xuid);

			char szSafeName[192];
			Q_snprintf(szSafeName, sizeof(szSafeName), "%s", pszName);

			char c;
			for (size_t i = 0; (c = szSafeName[i]); ++i)
			{
				if (c == ';')
					szSafeName[i] = ':';
			}

			engine->ServerCommand(CFmtStr("say %s has connected [%d/%d]\n", szSafeName, g_LobbyMgr.GetConnectedPlayerCount(), g_LobbyMgr.MatchPlayerCount()));
		}
	}

	g_EventLogger.LogPlayerConnect(pszName, sid);
}

void D2Lobby::Hook_ClientDisconnect(CEntityIndex index, int userId, /* ENetworkDisconnectionReason */ int reason,
	const char *pszName, uint64 xuid, const char *pszNetworkID)
{
	CSteamID sid(xuid);
	g_LobbyMgr.OnPlayerDisconnected(xuid);

	if (g_LobbyMgr.GetGameState() == DOTA_GAMERULES_STATE_WAIT_FOR_PLAYERS_TO_LOAD)
	{
		if (!!Q_stricmp(pszNetworkID, "BOT"))
		{
			UTIL_MsgAndLog("Player disconnected \"%s\" (%" PRIu64 ")\n", pszName, xuid);

			char szSafeName[192];
			Q_snprintf(szSafeName, sizeof(szSafeName), "%s", pszName);

			char c;
			for (size_t i = 0; (c = szSafeName[i]); ++i)
			{
				if (c == ';')
					szSafeName[i] = ':';
			}

			engine->ServerCommand(CFmtStr("say %s has disconnected [%d/%d]\n", szSafeName, g_LobbyMgr.GetConnectedPlayerCount(), g_LobbyMgr.MatchPlayerCount()));
		}
	}

	g_EventLogger.LogPlayerDisconnect(pszName, sid, reason);
}

void D2Lobby::Hook_ServerHibernationUpdate(bool bHibernating)
{
	Msg("ServerHibernationUpdate(%s)\n", bHibernating ? "true" : "false");
	if (bHibernating)
	{
		s_bLieAboutVersion = true;
	}

	RETURN_META(MRES_IGNORED);
}

void D2Lobby::Hook_ServerHibernationUpdate_Post(bool bHibernating)
{
	if (!bHibernating)
	{
		if (CommandLine()->HasParm("-d2lnoneuts"))
		{
			static ConVarRef dota_neutral_initial_spawn_delay("dota_neutral_initial_spawn_delay");
			dota_neutral_initial_spawn_delay.SetValue(1800000.0f);
		}
		
		g_LobbyMgr.FinalizeLobby();
	}
	Msg("ServerHibernationUpdate(%s) POST\n", bHibernating ? "true" : "false");
}

void D2Lobby::AllPluginsLoaded()
{
	/* This is where we'd do stuff that relies on the mod or other plugins 
	 * being initialized (for example, cvars added and events registered).
	 */
}

bool D2Lobby::Pause(char *error, size_t maxlen)
{
	return true;
}

bool D2Lobby::Unpause(char *error, size_t maxlen)
{
	return true;
}

const char *D2Lobby::GetLicense()
{
	return "Private";
}

const char *D2Lobby::GetVersion()
{
	return "0.0.0.1";
}

const char *D2Lobby::GetDate()
{
	return __DATE__;
}

const char *D2Lobby::GetLogTag()
{
	return "D2L";
}

const char *D2Lobby::GetAuthor()
{
	return "Nicholas Hastings <nshastings@gmail.com>";
}

const char *D2Lobby::GetDescription()
{
	return "";
}

const char *D2Lobby::GetName()
{
	return "Dota 2 Lobby Management";
}

const char *D2Lobby::GetURL()
{
	return "https://github.com/psychonic";
}

