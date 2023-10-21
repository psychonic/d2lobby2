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
#include "constants.h"

#include <basetypes.h>

#include <generated_proto/dota_gcmessages_common.pb.h>

#include <vector>

class CSteamID;
struct json_t;

enum class EventType : int
{
	GameStateChange,
	PlayerConnect,
	PlayerDisconnect,
	PlayerTeam_OBSOLETE,
	HeroDeath,
	TowerKill,
	CourierKill,
	RoshanKill,
	RuneBottled,
	RuneUsed,
	AegisPickup,
	AegisSteal,
	Buyback,
	AegisDeny,
	FirstBlood,
	TowerDeny,
	ItemPurchase,
	CallGG,
	CancelGG,
};

class EventLogger : IPluginSystem
{
public:
	virtual const char *GetName() const override { return "Event Logger"; }
	bool OnLoad() override;
	void OnUnload() override;
	void OnDOTAGameStateChange(uint32 oldState, uint32 newState) override;
public:
	void Hook_OnCmdSay(const CCommandContext &, const CCommand &);
	void Hook_OnCmdGG(const CCommandContext &, const CCommand &);
	void Hook_OnCmdCancelGG(const CCommandContext &, const CCommand &);
	void Hook_SetCommandClient(CPlayerSlot slot);
public:
	void OnGameEvent(uint16 id, const void *pData, int clientCount = 0, const unsigned char *clients = nullptr);
	void LogGGCall(uint64 steamId64, int team);
	void LogGGCancel(uint64 steamId64, int team);
	void LogPlayerConnect(const char *pszName, const CSteamID &steamId);
	void LogPlayerDisconnect(const char *pszName, const CSteamID &steamId, int reason);
private:
	void LogHeroKill(int victimId, std::vector<int> &killers, uint gold);
	void LogSimplePlayerEvent(EventType type, int playerId);
	void LogTowerKill(int playerId, int team);
	void LogCourierKill(int team);
	void LogRoshanKill(int team, uint gold);
	void LogRuneBottle(int playerId, DotaRune rune);
	void LogRuneUse(int playerId, DotaRune rune);
	void LogItemPurchase(int playerId, int itemId);
private:
	void HandlePossibleGG(const CSteamID &sid);
	json_t *CreateTimedEvent(EventType type);
	void SendAndFreeEvent(json_t *pData);
private:
	std::vector<int> m_Hooks;
	DotaTeam m_GGTeam = kTeamUnassigned;
	CPlayerSlot m_CommandClient = 0;
};

extern EventLogger g_EventLogger;
