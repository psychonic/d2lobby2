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

#include "pluginsystem.h"

#include <steam/steam_gameserver.h>
#include <steam/isteamgamecoordinator.h>

#include <generated_proto/dota_gcmessages_common.pb.h>

#include <queue>
#include <vector>
#include <subhook.h>

class GCManager : public IPluginSystem
{
public:
	virtual const char *GetName() const override { return "GC Manager"; }
	virtual bool OnLoad() override;
	virtual void OnUnload() override;
public:
	void Hook_GameServerSteamAPIActivated();

	bool Hook_IsMessageAvailable(uint32 *pcubMsgSize);
	EGCResults Hook_RetrieveMessage(uint32 *punMsgType, void *pubDest, uint32 cubDest, uint32 *pcubMsgSize);
	EGCResults Hook_RetrieveMessagePost(uint32 *punMsgType, void *pubDest, uint32 cubDest, uint32 *pcubMsgSize);
	EGCResults Hook_SendMessage(uint32 unMsgType, const void *pubData, uint32 cubData);
public:
	void InjectGCMessage(const std::string &msg) {
		m_GCMsgsToInject.push(msg);
		m_Notify = SteamGCNotify::NeedsNotify;
	}

	bool NeedsSteamGCNotify() const { return m_Notify == SteamGCNotify::NeedsNotify; }
	bool NeedsSteamGCFree() const { return m_Notify == SteamGCNotify::NeedsFree; }

	void OnSteamGCNotify() { m_Notify = SteamGCNotify::NeedsFree; }
	void OnSteamGCFree() { m_Notify = SteamGCNotify::None; }
private:
	inline bool HeaderFromBuffer(CMsgProtoBufHeader &hdr, const void *pubData, uint32 cubData)
	{
		int headerSize = *(int *)((intptr_t)pubData + 4);
		return hdr.ParsePartialFromArray((const void *)((intptr_t)pubData + 8), headerSize);
	}

	template<typename T>
	inline bool MessageFromBuffer(T &msg, const void *pubData, uint32 cubData)
	{
		int headerSize = *(int *)((intptr_t)pubData + 4);
		return msg.ParseFromArray((const void *)((intptr_t)pubData + 8 + headerSize), cubData - headerSize - 8);
	}
private:
	std::vector<int> m_SteamHooks;
	std::queue<std::string> m_GCMsgsToInject;

	enum class SteamGCNotify
	{
		None,
		NeedsNotify,
		NeedsFree,
	};
	SteamGCNotify m_Notify = SteamGCNotify::None;
public:
	subhook_t m_HookGetCallback;
	subhook_t m_HookFreeCallback;
};

extern GCManager g_GCMgr;
