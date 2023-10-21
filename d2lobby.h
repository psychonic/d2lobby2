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

#pragma once

#include <ISmmPlugin.h>

#include <steam/steam_gameserver.h>

#include <igameeventsystem.h>
#include <vscript/ivscript.h>
#include <generated_proto/dota_gcmessages_common.pb.h>
#include <generated_proto/dota_gcmessages_server.pb.h>

#include <vector>

struct json_t;
class CMsgDOTAPlayerFailedToConnect;
class CMsgGameMatchSignOut;


#if defined WIN32 && !defined snprintf
#define snprintf _snprintf
#endif

class D2Lobby : public ISmmPlugin
{
public: // ISmmPlugin
	bool Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late);
	bool Unload(char *error, size_t maxlen);
	bool Pause(char *error, size_t maxlen);
	bool Unpause(char *error, size_t maxlen);
	void AllPluginsLoaded();
	const char *GetAuthor();
	const char *GetName();
	const char *GetDescription();
	const char *GetURL();
	const char *GetLicense();
	const char *GetVersion();
	const char *GetDate();
	const char *GetLogTag();
private:
	bool InitGlobals(char *error, size_t maxlen);
	void InitHooks();
	void ShutdownHooks();
	void SendMatchData();
	void BeginShutdown();
public:
	void OnGCPlayerFailedToConnect(CMsgDOTAPlayerFailedToConnect &msg);
	void OnLiveStatsUpdate(CMsgDOTALiveScoreboardUpdate &msg);
	void OnGCMatchSignOut(CMsgGameMatchSignOut &msg);
public:
	int Hook_GetBuildVersion() const;
	void Hook_ServerHibernationUpdate(bool bHibernating);
	void Hook_ServerHibernationUpdate_Post(bool bHibernating);
	void Hook_GameServerSteamAPIActivated();
	void Hook_PostEventAbstract_Local(CSplitScreenSlot, GameEventHandle_t__ *, const void *, unsigned long);
	void Hook_PostEventAbstract(CSplitScreenSlot, bool, int, const unsigned char *, GameEventHandle_t__ *, const void *pData, unsigned long nSize, NetChannelBufType_t);
	void Hook_PostEntityEventAbstract(const CBaseHandle &, GameEventHandle_t__ *, const void *pData, unsigned long nSize, NetChannelBufType_t);
	void Hook_OnClientConnected(CEntityIndex index, int userId, const char *pszName, uint64 xuid, const char *pszNetworkID,
		const char *pszAddress, bool bFakePlayer);
	void Hook_ClientDisconnect(CEntityIndex index, int userId, /* ENetworkDisconnectionReason */ int reason,
		const char *pszName, uint64 xuid, const char *pszNetworkID);
	IScriptVM *Hook_CreateVM(ScriptLanguage_t lang);
	void Hook_DestroyVM(IScriptVM *pVM);
	void Hook_GameFrame(bool, bool, bool);
private:
	json_t *m_MatchData;
	std::vector<int> m_GlobalHooks;
	DOTA_GameState m_GameState = DOTA_GAMERULES_STATE_INIT;

	enum class ShutdownState
	{
		None,
		PreShutdown,
		ShuttingDown,
	};
	float m_flPreShutdownStartTime = 0.0f;
	ShutdownState m_ShutdownState = ShutdownState::None;
};

extern D2Lobby g_D2Lobby;

extern IVEngineServer2 *engine;
extern ISource2Server *gamedll;
extern ISource2GameClients *serverclients;
extern ISteamHTTP *http;
extern IFileSystem *filesystem;
extern IScriptVM *scriptvm;
extern CSteamGameServerAPIContext steamctx;

PLUGIN_GLOBALVARS();

