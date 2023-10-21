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

#ifdef _WIN32
#include <Windows.h>
#undef SendMessage
#endif

#include "gcmgr.h"

#include "d2lobby.h"
#include "lobbymgr.h"
#include "util.h"

#include <tier1/fmtstr.h>
#include <generated_proto/dota_gcmessages_msgid.pb.h>
#include <generated_proto/gcsystemmsgs.pb.h>

SH_DECL_EXTERN0_void(ISource2Server, GameServerSteamAPIActivated, SH_NOATTRIB, 0);
SH_DECL_HOOK3(ISteamGameCoordinator, SendMessage, SH_NOATTRIB, 0, EGCResults, uint32, const void *, uint32);
SH_DECL_HOOK4(ISteamGameCoordinator, RetrieveMessage, SH_NOATTRIB, 0, EGCResults, uint32 *, void *, uint32, uint32 *);
SH_DECL_HOOK1(ISteamGameCoordinator, IsMessageAvailable, SH_NOATTRIB, 0, bool, uint32 *);

// reference to a steam call, to filter results by
typedef int32 HSteamCall;

static bool Hook_Steam_BGetCallback(HSteamPipe, CallbackMsg_t *, HSteamCall);
static void Hook_Steam_FreeLastCallback(HSteamPipe);

typedef bool(*Steam_BGetCallback)(HSteamPipe, CallbackMsg_t *, HSteamCall);
typedef void(*Steam_FreeLastCallback)(HSteamPipe);
Steam_BGetCallback fnSteam_BGetCallback;
Steam_FreeLastCallback fnSteam_FreeLastCallback;

static ISteamGameCoordinator *gamecoordinator = nullptr;

bool GCManager::OnLoad()
{
	int hookId = SH_ADD_HOOK(ISource2Server, GameServerSteamAPIActivated, gamedll, SH_MEMBER(this, &GCManager::Hook_GameServerSteamAPIActivated), false);

#ifdef _WIN32
	HMODULE hSteamClient;
#ifdef _WIN64
	hSteamClient = LoadLibrary("steamclient64.dll");
#else
	hSteamClient = LoadLibrary("steamclient.dll");
#endif // _WIN64
	fnSteam_BGetCallback = (Steam_BGetCallback)GetProcAddress(hSteamClient, "Steam_BGetCallback");
	fnSteam_FreeLastCallback = (Steam_FreeLastCallback)GetProcAddress(hSteamClient, "Steam_FreeLastCallback");
#else  // _WIN32
	void *pSteamClient = dlopen("steamclient.so", RTLD_LAZY);
	fnSteam_BGetCallback = (Steam_BGetCallback)dlsym(pSteamClient, "Steam_BGetCallback");
	fnSteam_FreeLastCallback = (Steam_FreeLastCallback)dlsym(pSteamClient, "Steam_FreeLastCallback");
#endif // !_WIN32

	if (!fnSteam_BGetCallback)
	{
		Msg("Failed to find Steam_BGetCallback!\n");
		return false;
	}

	if (!fnSteam_FreeLastCallback)
	{
		Msg("Failed to find Steam_FreeLastCallback!\n");
		return false;
	}

	DevMsg("Found BGetCallback at 0x%p\n", fnSteam_BGetCallback);
	DevMsg("Found FreeLastCallback at 0x%p\n", fnSteam_FreeLastCallback);

	m_HookGetCallback = subhook_new((void *)fnSteam_BGetCallback, (void *)&Hook_Steam_BGetCallback,
#if defined( __x86_64__ ) || defined( _M_X64 )
		SUBHOOK_OPTION_64BIT_OFFSET
#else
		subhook_options_t(0)
#endif
	);
	if (m_HookGetCallback == NULL)
	{
		Msg("!!!!! Failed to create GetCallback hook.\n");
		return false;
	}

	m_HookFreeCallback = subhook_new((void *)fnSteam_FreeLastCallback, (void *)&Hook_Steam_FreeLastCallback,
#if defined( __x86_64__ ) || defined( _M_X64 )
		SUBHOOK_OPTION_64BIT_OFFSET
#else
		subhook_options_t(0)
#endif
	);
	if (m_HookFreeCallback == NULL)
	{
		Msg("!!!!! Failed to create FreeCallback hook.\n");
		return false;
	}

	if (0 != subhook_install(m_HookGetCallback))
	{
		Msg("!!!!! Failed to install GetCallback hook.\n");
		return false;
	}

	if (0 != subhook_install(m_HookFreeCallback))
	{
		Msg("!!!!! Failed to install FreeCallback hook.\n");
		return false;
	}

	DevMsg("GetCallback (func: 0x%p) (src: 0x%p) (dst: 0x%p) (realdst: 0x%p)\n", fnSteam_BGetCallback, subhook_get_src(m_HookGetCallback), subhook_get_dst(m_HookGetCallback), &Hook_Steam_BGetCallback);
	DevMsg("FreeCallback (func: 0x%p) (src: 0x%p) (dst: 0x%p) (realdst: 0x%p)\n", fnSteam_FreeLastCallback, subhook_get_src(m_HookFreeCallback), subhook_get_dst(m_HookFreeCallback), &Hook_Steam_FreeLastCallback);

	return hookId != 0;
}

void GCManager::OnUnload()
{
	SH_REMOVE_HOOK(ISource2Server, GameServerSteamAPIActivated, gamedll, SH_MEMBER(this, &GCManager::Hook_GameServerSteamAPIActivated), false);

	for (auto h : m_SteamHooks)
	{
		SH_REMOVE_HOOK_ID(h);
	}

	m_SteamHooks.clear();

	subhook_remove(m_HookGetCallback);
	subhook_remove(m_HookFreeCallback);
	
	subhook_free(m_HookGetCallback);
	subhook_free(m_HookFreeCallback);
}

void GCManager::Hook_GameServerSteamAPIActivated()
{
	static bool bGCHooked = false;
	if (!bGCHooked)
	{
		HSteamUser hSteamUser = SteamGameServer_GetHSteamUser();
		HSteamPipe hSteamPipe = SteamGameServer_GetHSteamPipe();

		gamecoordinator = (ISteamGameCoordinator *)steamctx.SteamClient()->GetISteamGenericInterface(hSteamUser, hSteamPipe, STEAMGAMECOORDINATOR_INTERFACE_VERSION);
		UTIL_MsgAndLog("Found ISteamGameCoordinator at %p.\n", gamecoordinator);

		m_SteamHooks.push_back(
			SH_ADD_HOOK(ISteamGameCoordinator, RetrieveMessage, gamecoordinator, SH_MEMBER(this, &GCManager::Hook_RetrieveMessage), false)
			);
		m_SteamHooks.push_back(
			SH_ADD_HOOK(ISteamGameCoordinator, RetrieveMessage, gamecoordinator, SH_MEMBER(this, &GCManager::Hook_RetrieveMessagePost), true)
			);
		m_SteamHooks.push_back(
			SH_ADD_HOOK(ISteamGameCoordinator, SendMessage, gamecoordinator, SH_MEMBER(this, &GCManager::Hook_SendMessage), false)
			);
		m_SteamHooks.push_back(
			SH_ADD_HOOK(ISteamGameCoordinator, IsMessageAvailable, gamecoordinator, SH_MEMBER(this, &GCManager::Hook_IsMessageAvailable), false)
			);

		bGCHooked = true;

		CMsgProtoBufHeader welcomeHdr;
		CMsgClientWelcome welcomeMsg;
		welcomeMsg.set_version(1);

		uint32 emsg = k_EMsgGCServerWelcome;
		int hdrSize = welcomeHdr.ByteSize();

		std::string message;
		message.append((const char *)&emsg, sizeof(uint32));
		message.append((const char *)&hdrSize, sizeof(int));
		welcomeHdr.AppendToString(&message);
		welcomeMsg.AppendToString(&message);
		InjectGCMessage(message);
	}

	RETURN_META(MRES_IGNORED);
}

static bool Hook_Steam_BGetCallback(HSteamPipe hPipe, CallbackMsg_t *pCallback, HSteamCall hCall)
{
	if (!g_GCMgr.NeedsSteamGCNotify())
	{
#if defined( __x86_64__ ) || defined ( _M_X64 )
		// For x64, need to remove and re-add detour before calling trampoline.
		// Due to bug in subhook, trampoline code does not account for more that 
		// 32-bit difference between original func and trampoline
		subhook_remove(g_GCMgr.m_HookGetCallback);
		bool ret = fnSteam_BGetCallback(hPipe, pCallback, hCall);
		subhook_install(g_GCMgr.m_HookGetCallback);
		return ret;
#else
		void *pTramp = subhook_get_trampoline(g_GCMgr.m_HookGetCallback);
		return ((Steam_BGetCallback)pTramp)(hPipe, pCallback, hCall);
#endif
	}

	static GCMessageAvailable_t gcmsg;
	// Unused. GCSDK ignores callback content and just checks IsMessageAvailable
	//gcmsg.m_nMessageSize = ;	

	pCallback->m_hSteamUser = SteamAPI_GetHSteamUser();
	pCallback->m_iCallback = GCMessageAvailable_t::k_iCallback;
	pCallback->m_pubParam = (uint8 *)&gcmsg;
	pCallback->m_cubParam = sizeof(GCMessageAvailable_t);

	g_GCMgr.OnSteamGCNotify();

	return true;
}

static void Hook_Steam_FreeLastCallback(HSteamPipe hPipe)
{
	if (g_GCMgr.NeedsSteamGCNotify())
	{
		g_GCMgr.OnSteamGCFree();
		return;
	}

#if defined( __x86_64__ ) || defined ( _M_X64 )
	// For x64, need to remove and re-add detour before calling trampoline.
	// Due to bug in subhook, trampoline code does not account for more that 
	// 32-bit difference between original func and trampoline
	subhook_remove(g_GCMgr.m_HookFreeCallback);
	fnSteam_FreeLastCallback(hPipe);
	subhook_install(g_GCMgr.m_HookFreeCallback);
#else
	void *pTramp = subhook_get_trampoline(g_GCMgr.m_HookFreeCallback);
	((Steam_FreeLastCallback)pTramp)(hPipe);
#endif
}

bool GCManager::Hook_IsMessageAvailable(uint32 *pcubMsgSize)
{
	UTIL_LogToFile("ISM (%u to inject)\n", m_GCMsgsToInject.size());
	if (m_GCMsgsToInject.size())
	{
		*pcubMsgSize = m_GCMsgsToInject.front().length();
		UTIL_LogToFile("Server checking for available msg and we have one of size %d\n", *pcubMsgSize);
		RETURN_META_VALUE(MRES_SUPERCEDE, true);
	}

	RETURN_META_VALUE(MRES_IGNORED, true);
}

EGCResults GCManager::Hook_RetrieveMessage(uint32 *punMsgType, void *pubDest, uint32 cubDest, uint32 *pcubMsgSize)
{
	if (m_GCMsgsToInject.size())
	{
		auto &msg = m_GCMsgsToInject.front();

		*punMsgType = *(uint32 *)msg.data();
		*pcubMsgSize = msg.length();

		UTIL_LogToFile("Server retrieving for msg and we have one of size %d\n", *pcubMsgSize);
		UTIL_LogToFile("MsgType %u (%u), cubDest %u\n", *punMsgType, (*punMsgType) & ~0x80000000, cubDest);

		if (*pcubMsgSize > cubDest)
		{
			RETURN_META_VALUE(MRES_SUPERCEDE, k_EGCResultBufferTooSmall);
		}

		memcpy(pubDest, msg.data(), msg.length());

		m_GCMsgsToInject.pop();

		RETURN_META_VALUE(MRES_SUPERCEDE, k_EGCResultOK);
	}

	EGCResults ret = SH_CALL(gamecoordinator, &ISteamGameCoordinator::RetrieveMessage)(punMsgType, pubDest, cubDest, pcubMsgSize);

	int realMsg = (*punMsgType) & ~0x80000000;

	switch (realMsg)
	{
	case k_EMsgGCGCToRelayConnect:
	case k_EMsgGCToServerConsoleCommand:
		RETURN_META_VALUE(MRES_SUPERCEDE, k_EGCResultNoMessage);
	case k_EMsgGCRequestBatchPlayerResourcesResponse:
	{
		CMsgDOTARequestBatchPlayerResourcesResponse msg;
		MessageFromBuffer(msg, pubDest, cubDest);

		Msg("Got k_EMsgGCRequestBatchPlayerResourcesResponse.\n");
		for (auto &r : msg.results())
		{
			Msg("----------------\n");
			Msg("%s\n", r.DebugString().c_str());
		}
		Msg("----------------\n");
	}
	case k_EMsgGCGameMatchSignOutPermissionResponse:
	{
		UTIL_LogToFile("Intercepted incoming k_EMsgGCGameMatchSignOutPermissionResponse\n");
		uint32 skip = 4 /*emsg*/ + 4 /*hsizesize*/ + *(int32 *)((intp)pubDest + 4) /*hsize*/;
		void *start = (void *)((intp)pubDest + skip);
		uint32 size = *pcubMsgSize - skip;

		CMsgGameMatchSignOutPermissionResponse msg;
		if (msg.ParseFromArray(start, size))
		{
			msg.set_permission_granted(true);
			msg.clear_retry_delay_seconds();
			uint32 newMsgSize = (uint32)msg.ByteSize();
			if (newMsgSize > (cubDest - skip))
			{
				RETURN_META_VALUE(MRES_SUPERCEDE, k_EGCResultBufferTooSmall);
			}

			msg.SerializeToArray(start, newMsgSize);
			*pcubMsgSize = newMsgSize + skip;
			RETURN_META_VALUE(MRES_SUPERCEDE, k_EGCResultOK);
		}
		else
		{
			UTIL_MsgAndLog("Failed to parse SignOutPermissionResponse\n");
		}
	}
	break;
	}

	RETURN_META_VALUE(MRES_SUPERCEDE, ret);
}

EGCResults GCManager::Hook_RetrieveMessagePost(uint32 *punMsgType, void *pubDest, uint32 cubDest, uint32 *pcubMsgSize)
{
	int realMsg = (*punMsgType) & ~0x80000000;
	if (realMsg == k_EMsgGCServerWelcome)
	{
		static bool bGotWelcome = false;

		Msg("Received GC Welcome\n");
		if (!bGotWelcome && CommandLine()->HasParm("-dotacfg"))
		{
			engine->ServerCommand(CFmtStr("exec %s\n", CommandLine()->ParmValue("-dotacfg")));
		}
		bGotWelcome = true;
	}

	return k_EGCResultOK;
}

EGCResults GCManager::Hook_SendMessage(uint32 unMsgType, const void *pubData, uint32 cubData)
{
	int realMsg = unMsgType & ~0x80000000;

	switch (realMsg)
	{
	case k_EMsgGCLiveScoreboardUpdate:
	{
		DevMsg("!!!!! GOT SCOREBOARD UPDATE (%.2f)\n", Plat_FloatTime());

		// Set league_id in lobby for this to work (1 is fine)
		CMsgDOTALiveScoreboardUpdate msg;
		MessageFromBuffer(msg, pubData, cubData);
		g_D2Lobby.OnLiveStatsUpdate(msg);

		RETURN_META_VALUE(MRES_SUPERCEDE, k_EGCResultOK);
	}
	case k_EMsgGCPlayerFailedToConnect:
	{
		UTIL_MsgAndLog("Intercepted outgoing k_EMsgGCPlayerFailedToConnect\n");

		CMsgDOTAPlayerFailedToConnect msg;
		MessageFromBuffer(msg, pubData, cubData);
		g_D2Lobby.OnGCPlayerFailedToConnect(msg);

		RETURN_META_VALUE(MRES_SUPERCEDE, k_EGCResultOK);
	}
	case k_EMsgGCConnectedPlayers:
	{
		CMsgConnectedPlayers msg;
		MessageFromBuffer(msg, pubData, cubData);

		UTIL_MsgAndLog("Intercepted outgoing k_EMsgGCConnectedPlayers (%s)\n", CMsgConnectedPlayers_SendReason_Name(msg.send_reason()).c_str());

		g_LobbyMgr.HandleConnectedPlayers(msg);

		RETURN_META_VALUE(MRES_SUPERCEDE, k_EGCResultOK);
	}
	case k_EMsgGCGameMatchSignOut:
	{
		UTIL_LogToFile("Intercepted outgoing k_EMsgGCGameMatchSignOut\n");

		UTIL_LogToFile("Building match end data\n");

		CMsgProtoBufHeader hdrIn;
		HeaderFromBuffer(hdrIn, pubData, cubData);
		uint64 jobId_gs = hdrIn.job_id_source();

		CMsgGameMatchSignOut msgIn;
		MessageFromBuffer(msgIn, pubData, cubData);

		g_D2Lobby.OnGCMatchSignOut(msgIn);
		
		CMsgProtoBufHeader hdrOut;
		hdrOut.set_job_id_target(jobId_gs);

		CMsgGameMatchSignoutResponse msgOut;
		msgOut.set_match_id(g_LobbyMgr.MatchId());

		uint32 emsg = k_EMsgGCGameMatchSignOutResponse | 0x80000000;
		int headerSize = hdrOut.ByteSize();

		std::string message;
		message.append((const char *)&emsg, sizeof(uint32));
		message.append((const char *)&headerSize, sizeof(int));
		hdrOut.AppendToString(&message);
		msgOut.AppendToString(&message);

		Msg("Adding SignOutResposne message to the queue\n");

		InjectGCMessage(message);

		if (msgIn.good_guys_win())
		{
			g_LobbyMgr.EnterPostGame(k_EMatchOutcome_RadVictory);
		}
		else
		{
			g_LobbyMgr.EnterPostGame(k_EMatchOutcome_DireVictory);
		}
		//g_LobbyMgr.DeleteLobby();

		RETURN_META_VALUE(MRES_SUPERCEDE, k_EGCResultOK);
	}
	}

	RETURN_META_VALUE(MRES_IGNORED, k_EGCResultOK);
}

GCManager g_GCMgr;
