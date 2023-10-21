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

#include <generated_proto/dota_gcmessages_common.pb.h>
#include <generated_proto/dota_gcmessages_server.pb.h>

#include <vector>

class LobbyManager : public IPluginSystem
{
public:
	virtual const char *GetName() const override { return "Lobby Manager"; }
	virtual bool OnLoad() override;
	virtual void OnUnload() override;
public:
	bool AddRadiantPlayer(const CSteamID &steamId, const char *pszName, const char *pszHero);
	bool AddDirePlayer(const CSteamID &steamId, const char *pszName, const char *pszHero);
	bool AddSpectatorPlayer(const CSteamID &steamId, const char *pszName);
	void SetSeriesData(DotaSeriesType type, uint8 radiantWins, uint8 direWins);
	void SetMatchId(uint64 id);
	uint32 GetGameMode() const { return m_GameMode; }
	void SetGameMode(uint32 mode);
	void SetMatchType(uint32 matchType);

	CSteamID MemberSteamIdFromAccountId(AccountID_t id);

	void CheckInjectLobby();

	DOTA_GameState GetGameState() const { return m_Lobby.game_state(); }
	void GetPlayersWithoutHeroPicks(CUtlVector<CSteamID> &out);
	int GetPlayerTeam(const CSteamID &steamId);
	void UpdatePlayerName(const CSteamID &steamId, const char *pszName);

	void EnterPostGame(EMatchOutcome outcome);
	void FinalizeLobby();
	void DeleteLobby();
	void PrintDebug();

	void HandleConnectedPlayers(const CMsgConnectedPlayers &msg);

	// These are probably redundant but I'm tired of this player count boooog
	void OnPlayerConnected(const CSteamID &steamId);
	void OnPlayerDisconnected(const CSteamID &steamId);

	const char *GetPlayerHero(const CSteamID &steamId);


	int GetConnectedPlayerCount() const;
	void GetNotConnectedPlayerNames(char *pszNames, size_t len);
	bool Hook_LobbyAllowsCheats() const;

	inline int MatchPlayerCount() const
	{
		return m_Lobby.members().size();
	}

	inline uint64 MatchId() const
	{
		return m_MatchId;
	}

	inline const google::protobuf::RepeatedPtrField<CDOTALobbyMember> &LobbyMembers()
	{
		return m_Lobby.members();
	}

private:
	void PopulateLobbyData();
	void SendLobbySOUpdate();
private:
	class Player
	{
	public:
		Player(CSteamID steamId, const char *pszName, const char *pszHero = nullptr)
		{
			m_SteamId = steamId;
			Q_strncpy(m_szName, pszName, sizeof(m_szName));
			if (pszHero)
			{
				Q_strncpy(m_szHero, pszHero, sizeof(m_szHero));
			}
			else
			{
				m_szHero[0] = 0;
			}
		}
	public:
		inline CSteamID SteamId() const
		{
			return m_SteamId;
		}
		inline const char *Name() const
		{
			return m_szName;
		}
		inline const char *Hero() const
		{
			if (m_szHero[0])
				return m_szHero;
			else
				return nullptr;
		}
	private:
		CSteamID m_SteamId;
		char m_szName[kMaxPlayerNameLength];
		char m_szHero[64];
	};

	struct SeriesData
	{
		DotaSeriesType Type;
		uint8 RadiantWins;
		uint8 DireWins;
	};

private:
	bool m_bLobbyInjected = false;

	std::vector<Player *> m_RadiantPlayers;
	std::vector<Player *> m_DirePlayers;
	std::vector<Player *> m_SpectatorPlayers;

	SeriesData m_SeriesData;

	uint64 m_MatchId = 0;
	uint32 m_GameMode = DOTA_GAMEMODE_AP;

	CSODOTALobby_LobbyType m_LobbyType = CSODOTALobby_LobbyType_CASUAL_1V1_MATCH;
	/*
	Auto-end after everyone on a team gone for 5m
	CSODOTALobby_LobbyType_CASUAL_MATCH
	CSODOTALobby_LobbyType_COOP_BOT_MATCH
	CSODOTALobby_LobbyType_LEGACY_SOLO_QUEUE_MATCH
	CSODOTALobby_LobbyType_COMPETITIVE_MATCH
	CSODOTALobby_LobbyType_CASUAL_1V1_MATCH

	"GG" works
	CSODOTALobby_LobbyType_TOURNAMENT
	CSODOTALobby_LobbyType_LEGACY_TEAM_MATCH
	CSODOTALobby_LobbyType_CASUAL_1V1_MATCH
	CSODOTALobby_LobbyType_LOCAL_BOT_MATCH

	Pause limiting works
	CSODOTALobby_LobbyType_CASUAL_MATCH
	CSODOTALobby_LobbyType_COOP_BOT_MATCH
	CSODOTALobby_LobbyType_LEGACY_TEAM_MATCH
	CSODOTALobby_LobbyType_LEGACY_SOLO_QUEUE_MATCH
	CSODOTALobby_LobbyType_COMPETITIVE_MATCH
	CSODOTALobby_LobbyType_CASUAL_1V1_MATCH
	CSODOTALobby_LobbyType_WEEKEND_TOURNEY
	*/

private:
	CSODOTALobby m_Lobby;
public:
	CSODOTALobby m_CustomLobby;
private:
	CMsgSOIDOwner m_LobbyOwner;

	const uint64 k_LobbyId = 24210021764591890;
	const int k_LobbySOType = 2004;
	const int k_LobbyOwnerType = 3;

	uint64 s_LobbyVersion = 24210021764591896;
};

extern LobbyManager g_LobbyMgr;
