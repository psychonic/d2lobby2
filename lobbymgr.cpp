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

#include "lobbymgr.h"

#include "d2lobby.h"
#include "gcmgr.h"
#include "util.h"

#include <inttypes.h>

#include <generated_proto/gcsystemmsgs.pb.h>

SH_DECL_HOOK0(IServerGCLobby, LobbyAllowsCheats, const, 0, bool);

LobbyManager g_LobbyMgr;

CON_COMMAND(set_custom_difficulty, "")
{
	g_LobbyMgr.m_CustomLobby.set_custom_difficulty(strtoul(args[1], nullptr, 10));
}

CON_COMMAND(set_custom_game_auto_created_lobby, "")
{
	g_LobbyMgr.m_CustomLobby.set_custom_game_auto_created_lobby(args[1][0] == '1' ? true : false);
}

CON_COMMAND(set_custom_game_crc, "")
{
	g_LobbyMgr.m_CustomLobby.set_custom_game_crc(strtoull(args[1], nullptr, 10));
}

CON_COMMAND(set_custom_game_id, "")
{
	g_LobbyMgr.m_CustomLobby.set_custom_game_id(strtoull(args[1], nullptr, 10));
}

CON_COMMAND(set_custom_game_mode, "")
{
	g_LobbyMgr.m_CustomLobby.set_custom_game_mode(args[1]);
}

CON_COMMAND(set_custom_game_timestamp, "")
{
	g_LobbyMgr.m_CustomLobby.set_custom_game_timestamp(strtoul(args[1], nullptr, 10));
}

CON_COMMAND(set_custom_game_uses_account_records, "")
{
	g_LobbyMgr.m_CustomLobby.set_custom_game_uses_account_records(args[1][0] == '1' ? true : false);
}

CON_COMMAND(set_custom_map_name, "")
{
	g_LobbyMgr.m_CustomLobby.set_custom_map_name(args[1]);
}

CON_COMMAND(set_custom_max_players, "")
{
	g_LobbyMgr.m_CustomLobby.set_custom_max_players(strtoul(args[1], nullptr, 10));
}

CON_COMMAND(set_custom_min_players, "")
{
	g_LobbyMgr.m_CustomLobby.set_custom_min_players(strtoul(args[1], nullptr, 10));
}

CON_COMMAND(d2lobby_debug, "")
{
	g_LobbyMgr.PrintDebug();
}

extern ConVar match_post_url;

bool LobbyManager::OnLoad()
{
	m_Lobby.Clear();
	m_CustomLobby.Clear();
	int hookid = SH_ADD_HOOK(IServerGCLobby, LobbyAllowsCheats, gamedll->GetServerGCLobby(), SH_MEMBER(this, &LobbyManager::Hook_LobbyAllowsCheats), false);
	return hookid != 0;
}

void LobbyManager::OnUnload()
{
	SH_REMOVE_HOOK(IServerGCLobby, LobbyAllowsCheats, gamedll->GetServerGCLobby(), SH_MEMBER(this, &LobbyManager::Hook_LobbyAllowsCheats), false);
}

void LobbyManager::PrintDebug()
{
	Msg("Radiant players:\n");
	for (auto *plyr : m_RadiantPlayers)
	{
		Msg("- %llu [U:%u:%u]\t(%s)\n", plyr->SteamId().ConvertToUint64(), plyr->SteamId().GetEUniverse(), plyr->SteamId().GetAccountID(), plyr->Name());
	}

	Msg("Dire players:\n");
	for (auto *plyr : m_DirePlayers)
	{
		Msg("- %llu [U:%u:%u]\t(%s)\n", plyr->SteamId().ConvertToUint64(), plyr->SteamId().GetEUniverse(), plyr->SteamId().GetAccountID(), plyr->Name());
	}

	Msg("Spectator players:\n");
	for (auto *plyr : m_SpectatorPlayers)
	{
		Msg("- %llu [U:%u:%u]\t(%s)\n", plyr->SteamId().ConvertToUint64(), plyr->SteamId().GetEUniverse(), plyr->SteamId().GetAccountID(), plyr->Name());
	}

#if 0
	for (int i = 0; i < m_iBroadcastChannelCnt; ++i)
	{
		Msg("Broadcast Channel %d (%s) - %s\n", i, m_BroadcastChannels[i].CountryCode, m_BroadcastChannels[i].Description);
		for (int j = 0; j < m_BroadcastChannels[i].CasterCount; ++j)
		{
			Msg("- %llu [U:1:%u]\t(%s)\n", plyr->SteamId().ConvertToUint64(), plyr->SteamId().GetAccountID(), plyr->Name());
		}
	}
#endif

	Msg("Match ID: \"%" PRIu64 "\"\n", m_MatchId);
	Msg("Match POST Url: \"%s\"\n", match_post_url.GetString());
	Msg("Match Type: %d (%s)\n", m_LobbyType, CSODOTALobby_LobbyType_Name(m_LobbyType).c_str());
	Msg("Game Mode: %d (%s)\n", m_GameMode, DOTA_GameMode_Name((DOTA_GameMode)m_GameMode).c_str());
}

CON_COMMAND(reset_all, "")
{
	//g_D2Lobby.ResetAll();
	//UTIL_LogToFile("Received command %s\n", args.GetCommandString());
}

CON_COMMAND(add_radiant_player, "add_radiant_player <steamid64> <name> [hero entity name]")
{
	if (args.ArgC() < 3 || args.ArgC() > 4)
	{
		Msg("add_radiant_player <steamid64> <name> [hero entity name]\n");
		return;
	}

	uint64 steamid64 = strtoull(args[1], nullptr, 10);
	if (steamid64 == 0)
	{
		Msg("Failed to parse steamid.\n");
		return;
	}

	CSteamID sid(steamid64);
	if (sid.GetEAccountType() != k_EAccountTypeIndividual || sid.GetEUniverse() != k_EUniversePublic)
	{
		Msg("Invalid steamid64 value.\n");
		return;
	}

	if (!g_LobbyMgr.AddRadiantPlayer(sid, args[2], args.ArgC() == 4 ? args[3] : nullptr))
	{
		Msg("Failed to add another player to radiant!\n");
	}
}

CON_COMMAND(add_dire_player, "add_dire_player <steamid64> <name> [hero entity name]")
{
	if (args.ArgC() < 3 || args.ArgC() > 4)
	{
		Msg("add_dire_player <steamid64> <name> [hero entity name]\n");
		return;
	}

	uint64 steamid64 = strtoull(args[1], nullptr, 10);
	if (steamid64 == 0)
	{
		Msg("Failed to parse steamid.\n");
		return;
	}

	CSteamID sid(steamid64);
	if (sid.GetEAccountType() != k_EAccountTypeIndividual || sid.GetEUniverse() != k_EUniversePublic)
	{
		Msg("Invalid steamid64 value.\n");
		return;
	}

	if (!g_LobbyMgr.AddDirePlayer(sid, args[2], args.ArgC() == 4 ? args[3] : nullptr))
	{
		Msg("Failed to add another player to dire!\n");
	}
}

CON_COMMAND(add_spectator_player, "add_spectator_player <steamid64> [name]")
{
	if (args.ArgC() < 2 || args.ArgC() > 3)
	{
		Msg("add_spectator_player <steamid64> [name]\n");
		return;
	}

	uint64 steamid64 = strtoull(args[1], nullptr, 10);
	if (steamid64 == 0)
	{
		Msg("Failed to parse steamid.\n");
		return;
	}

	CSteamID sid(steamid64);
	if (sid.GetEAccountType() != k_EAccountTypeIndividual || sid.GetEUniverse() != k_EUniversePublic)
	{
		Msg("Invalid steamid64 value.\n");
		return;
	}

	if (!g_LobbyMgr.AddSpectatorPlayer(sid, args.ArgC() == 3 ? args[2] : "Spectator"))
	{
		Msg("Failed to add another spectator player!\n");
	}
}

CON_COMMAND(set_series_data, "set_series_data <bo3|bo5> <radiantWins> <direWins>")
{
	if (args.ArgC() != 4)
	{
		Msg("set_series_data <bo3|bo5> <radiantWins> <direWins>\n");
		return;
	}

	DotaSeriesType type;
	if (!Q_strcmp(args[1], "bo3"))
	{
		type = DotaSeriesType::BO3;
	}
	else if (!Q_strcmp(args[1], "bo5"))
	{
		type = DotaSeriesType::BO5;
	}
	else
	{
		Msg("set_series_data <bo3|bo5> <radiantWins> <direWins>\n");
		return;
	}

	g_LobbyMgr.SetSeriesData(type, atoi(args[2]), atoi(args[3]));
}

CON_COMMAND(set_match_id, "set_match_id <id>")
{
	if (args.ArgC() != 2)
	{
		Msg("set_match_id <id>\n");
		return;
	}

	uint64 id = strtoull(args[1], nullptr, 10);
	if (id == 0)
	{
		Msg("Match id \"%s\" is not valid. Must be non-zero integer.\n", args[1]);
		return;
	}
	g_LobbyMgr.SetMatchId(id);
}

CON_COMMAND(set_match_type, "set_match_type <type>")
{
	if (args.ArgC() != 2)
	{
		Msg("set_match_type <type>\n");
		return;
	}

	uint32 id = strtoul(args[1], nullptr, 10);
	if (!CSODOTALobby_LobbyType_IsValid(id))
	{
		Msg("Match type \"%s\" is not valid.\n", args[1]);
		return;
	}
	g_LobbyMgr.SetMatchType(id);
}

CON_COMMAND(set_game_mode, "set_game_mode <mode>")
{
	if (args.ArgC() != 2)
	{
		Msg("set_game_mode <id>\n");
		return;
	}

	int mode = atoi(args[1]);
	if (!DOTA_GameMode_IsValid(mode))
	{
		Msg("Game mode \"%s\" is not valid.\n", args[1]);
		return;
	}
	g_LobbyMgr.SetGameMode(mode);
}

CON_COMMAND(finish_lobby_setup, "")
{
	g_LobbyMgr.CheckInjectLobby();
}

static bool s_bLobbyAllowsCheats = false;

bool LobbyManager::Hook_LobbyAllowsCheats() const
{
	RETURN_META_VALUE(MRES_SUPERCEDE, s_bLobbyAllowsCheats);
}

CON_COMMAND(make_team_lose, "make_team_lose")
{
	if (args.ArgC() != 2)
	{
		Msg("make_team_lose <2|3>\n");
		return;
	}

	int team = atoi(args[1]);
	if (team != kTeamRadiant && team != kTeamDire)
	{
		Msg("Team \"%s\" is not valid. 2 = Radiant, 3 = Dire.\n", args[1]);
		return;
	}

	s_bLobbyAllowsCheats = true;
	static ConVarRef sv_cheats("sv_cheats");
	sv_cheats.SetValue(true);
	if (team == kTeamRadiant)
		engine->ServerCommand("dota_dev bad_guys_win\n");
	else
		engine->ServerCommand("dota_dev good_guys_win\n");
}

const char *LobbyManager::GetPlayerHero(const CSteamID &steamId)
{
	for (auto *p : m_RadiantPlayers)
	{
		if (p->SteamId() == steamId)
			return p->Hero();
	}

	for (auto *p : m_DirePlayers)
	{
		if (p->SteamId() == steamId)
			return p->Hero();
	}

	return nullptr;
}

void LobbyManager::CheckInjectLobby()
{
	if (m_bLobbyInjected)
		return;

	{
		m_Lobby.set_lobby_id(k_LobbyId);
		m_Lobby.set_state(CSODOTALobby_State_UI);
		m_Lobby.set_game_state(DOTA_GAMERULES_STATE_INIT);
		m_Lobby.set_matchgroup(1);
		m_Lobby.set_dota_tv_delay(LobbyDotaTV_120);
		m_Lobby.set_lobby_type(m_LobbyType);

		if (m_CustomLobby.has_custom_difficulty())
			m_Lobby.set_custom_difficulty(m_CustomLobby.custom_difficulty());
		if (m_CustomLobby.has_custom_game_auto_created_lobby())
			m_Lobby.set_custom_game_auto_created_lobby(m_CustomLobby.custom_game_auto_created_lobby());
		if (m_CustomLobby.has_custom_game_crc())
			m_Lobby.set_custom_game_crc(m_CustomLobby.custom_game_crc());
		if (m_CustomLobby.has_custom_game_id())
			m_Lobby.set_custom_game_id(m_CustomLobby.custom_game_id());
		if (m_CustomLobby.has_custom_game_mode())
			m_Lobby.set_custom_game_mode(m_CustomLobby.custom_game_mode());
		if (m_CustomLobby.has_custom_game_timestamp())
			m_Lobby.set_custom_game_timestamp(m_CustomLobby.custom_game_timestamp());
		if (m_CustomLobby.has_custom_game_uses_account_records())
			m_Lobby.set_custom_game_uses_account_records(m_CustomLobby.custom_game_uses_account_records());

		if (m_CustomLobby.has_custom_map_name())
			m_Lobby.set_custom_map_name(m_CustomLobby.custom_map_name());
		else
			m_Lobby.set_custom_map_name("dota");

		if (m_CustomLobby.has_custom_max_players())
			m_Lobby.set_custom_max_players(m_CustomLobby.custom_max_players());
		if (m_CustomLobby.has_custom_min_players())
			m_Lobby.set_custom_min_players(m_CustomLobby.custom_min_players());

		CMsgSOCacheSubscribed sub;
		auto *pObject = sub.add_objects();
		pObject->set_type_id(2004);

		std::string data;
		m_Lobby.SerializeToString(&data);
		pObject->add_object_data(data);

		m_LobbyOwner.set_id(k_LobbyId);
		m_LobbyOwner.set_type(3);

		sub.mutable_owner_soid()->set_id(k_LobbyId);
		sub.mutable_owner_soid()->set_type(3);
		sub.set_version(s_LobbyVersion);

		uint32 emsg = k_ESOMsg_CacheSubscribed | 0x80000000;
		int headerSize = 0;

		std::string message;
		message.append((const char *)&emsg, sizeof(uint32));
		message.append((const char *)&headerSize, sizeof(int));
		sub.AppendToString(&message);

		Msg("Adding CacheSubscribed message to the queue\n");

		g_GCMgr.InjectGCMessage(message);
	}

	{
		PopulateLobbyData();

		m_Lobby.set_state(CSODOTALobby_State_SERVERASSIGN);
		m_Lobby.set_allow_spectating(true);

		m_Lobby.set_leagueid(1);

		CMsgSOSingleObject obj;
		obj.set_type_id(2004);
		obj.set_version(++s_LobbyVersion);
		obj.set_service_id(0);
		obj.mutable_owner_soid()->set_type(3);
		obj.mutable_owner_soid()->set_id(k_LobbyId);
		m_Lobby.SerializeToString(obj.mutable_object_data());

		uint32 emsg = k_ESOMsg_Create | 0x80000000;
		int headerSize = 0;

		std::string message;
		message.append((const char *)&emsg, sizeof(uint32));
		message.append((const char *)&headerSize, sizeof(int));
		obj.AppendToString(&message);

		Msg("Adding SOCreate message to the queue\n");

		g_GCMgr.InjectGCMessage(message);
	}

	{
		CSteamID serverID = engine->GetGameServerSteamID();
		m_Lobby.set_server_id(serverID.ConvertToUint64());

		m_Lobby.set_connect("server ip here");

		m_Lobby.set_state(CSODOTALobby_State_SERVERSETUP);

		CMsgSOMultipleObjects objs;
		auto obj = objs.add_objects_modified();
		obj->set_type_id(2004);
		m_Lobby.SerializeToString(obj->mutable_object_data());
		objs.set_version(++s_LobbyVersion);
		objs.set_service_id(0);
		objs.mutable_owner_soid()->set_type(3);
		objs.mutable_owner_soid()->set_id(k_LobbyId);

		uint32 emsg = k_ESOMsg_UpdateMultiple | 0x80000000;
		int headerSize = 0;

		std::string message;
		message.append((const char *)&emsg, sizeof(uint32));
		message.append((const char *)&headerSize, sizeof(int));
		objs.AppendToString(&message);

		Msg("[Stub] Adding SOUpdateMultiple message to the queue\n");

		g_GCMgr.InjectGCMessage(message);
	}

	m_bLobbyInjected = true;
}

void LobbyManager::PopulateLobbyData()
{
	int i;

	i = 1;
	for (auto &p : m_RadiantPlayers)
	{
		auto *pMember = m_Lobby.add_members();
		pMember->set_id(p->SteamId().ConvertToUint64());
		pMember->set_team(DOTA_GC_TEAM_GOOD_GUYS);
		pMember->set_name(p->Name());
		pMember->set_slot(i++); // 1-5, 1-5
		pMember->set_party_id(1);
		pMember->set_leaver_status(DOTA_LEAVER_NEVER_CONNECTED);
		pMember->set_channel(6);
		pMember->set_coach_team(DOTA_GC_TEAM_NOTEAM);
		pMember->set_partner_account_type(PARTNER_NONE);
		pMember->set_cameraman(false);
	}

	i = 1;
	for (auto &p : m_DirePlayers)
	{
		auto *pMember = m_Lobby.add_members();
		pMember->set_id(p->SteamId().ConvertToUint64());
		pMember->set_team(DOTA_GC_TEAM_BAD_GUYS);
		pMember->set_name(p->Name());
		pMember->set_slot(i++); // 1-5, 1-5
		pMember->set_party_id(2);
		pMember->set_leaver_status(DOTA_LEAVER_NEVER_CONNECTED);
		pMember->set_channel(6);
		pMember->set_coach_team(DOTA_GC_TEAM_NOTEAM);
		pMember->set_partner_account_type(PARTNER_NONE);
		pMember->set_cameraman(false);
	}

	i = 1;
	for (auto &p : m_SpectatorPlayers)
	{
		auto *pMember = m_Lobby.add_members();
		pMember->set_id(p->SteamId().ConvertToUint64());
		pMember->set_team(DOTA_GC_TEAM_SPECTATOR);
		pMember->set_name(p->Name());
		pMember->set_slot(i++); // 1-5, 1-5
		pMember->set_party_id(3);
		pMember->set_leaver_status(DOTA_LEAVER_NEVER_CONNECTED);
		pMember->set_channel(6);
		pMember->set_coach_team(DOTA_GC_TEAM_NOTEAM);
		pMember->set_partner_account_type(PARTNER_NONE);
		pMember->set_cameraman(false);
	}

	m_Lobby.set_pause_setting(LobbyDotaPauseSetting_Limited);

	m_Lobby.set_series_type((uint32)m_SeriesData.Type);
	m_Lobby.set_radiant_series_wins(m_SeriesData.RadiantWins);
	m_Lobby.set_dire_series_wins(m_SeriesData.DireWins);

	m_Lobby.set_game_mode(m_GameMode);

	//m_Lobby.set_match_id(m_MatchId);
}

bool LobbyManager::AddRadiantPlayer(const CSteamID &steamId, const char *pszName, const char *pszHero)
{
	if (m_bLobbyInjected)
	{
		Msg("It is too late to modify lobby data.\n");
		return false;
	}

	if (m_RadiantPlayers.size() >= kMaxTeamPlayers)
		return false;

	for (auto &p : m_RadiantPlayers)
	{
		if (p->SteamId() == steamId)
		{
			Msg("Player already added!.\n");
			return false;
		}
	}

	m_RadiantPlayers.push_back(new Player(steamId, pszName, pszHero));

	return true;
}

bool LobbyManager::AddDirePlayer(const CSteamID &steamId, const char *pszName, const char *pszHero)
{
	if (m_bLobbyInjected)
	{
		Msg("It is too late to modify lobby data.\n");
		return false;
	}

	if (m_DirePlayers.size() >= kMaxTeamPlayers)
		return false;

	for (auto &p : m_DirePlayers)
	{
		if (p->SteamId() == steamId)
		{
			Msg("Player already added!.\n");
			return false;
		}
	}

	m_DirePlayers.push_back(new Player(steamId, pszName, pszHero));

	return true;
}

bool LobbyManager::AddSpectatorPlayer(const CSteamID &steamId, const char *pszName)
{
	if (m_bLobbyInjected)
	{
		Msg("It is too late to modify lobby data.\n");
		return false;
	}

	for (auto &p : m_SpectatorPlayers)
	{
		if (p->SteamId() == steamId)
		{
			Msg("Player already added!.\n");
			return false;
		}
	}

	m_SpectatorPlayers.push_back(new Player(steamId, pszName));

	return true;
}

void LobbyManager::GetPlayersWithoutHeroPicks(CUtlVector<CSteamID> &out)
{
	for (auto m : m_Lobby.members())
	{
		if (m.hero_id() == 0)
			out.AddToTail(CSteamID((uint64)m.id()));
	}
}

int LobbyManager::GetPlayerTeam(const CSteamID &steamId)
{
	for (auto *p : m_RadiantPlayers)
	{
		if (p->SteamId() == steamId)
			return kTeamRadiant;
	}

	for (auto *p : m_DirePlayers)
	{
		if (p->SteamId() == steamId)
			return kTeamDire;
	}

	for (auto *p : m_SpectatorPlayers)
	{
		if (p->SteamId() == steamId)
			return kTeamSpectators;
	}

	return kTeamUnassigned;
}

void LobbyManager::UpdatePlayerName(const CSteamID &sid, const char *pszName)
{
	for (auto m : m_Lobby.members())
	{
		if (m.id() == sid.ConvertToUint64())
		{
			m.set_name(pszName);
			SendLobbySOUpdate();
			break;
		}
	}
}

void LobbyManager::SetSeriesData(DotaSeriesType type, uint8 radiantWins, uint8 direWins)
{
	m_SeriesData.Type = type;
	m_SeriesData.RadiantWins = radiantWins;
	m_SeriesData.DireWins = direWins;
}

void LobbyManager::SetMatchId(uint64 id)
{
	m_MatchId = id;
	g_Logger.SetMatchId(id);
}

void LobbyManager::SetGameMode(uint32 mode)
{
	m_GameMode = mode;
}

void LobbyManager::SetMatchType(uint32 matchType)
{
	m_LobbyType = (CSODOTALobby_LobbyType)matchType;
}

void LobbyManager::FinalizeLobby()
{
	m_Lobby.set_state(CSODOTALobby_State_RUN);
	m_Lobby.set_match_id(g_LobbyMgr.MatchId());

	SendLobbySOUpdate();
}

void LobbyManager::SendLobbySOUpdate()
{
	CMsgSOMultipleObjects objs;
	auto obj = objs.add_objects_modified();
	obj->set_type_id(k_LobbySOType);
	m_Lobby.SerializeToString(obj->mutable_object_data());
	objs.set_version(++s_LobbyVersion);
	objs.set_service_id(0);
	objs.mutable_owner_soid()->set_type(k_LobbyOwnerType);
	objs.mutable_owner_soid()->set_id(k_LobbyId);

	uint32 emsg = k_ESOMsg_UpdateMultiple | 0x80000000;
	int headerSize = 0;

	std::string message;
	message.append((const char *)&emsg, sizeof(uint32));
	message.append((const char *)&headerSize, sizeof(int));
	objs.AppendToString(&message);

	Msg("[Stub] Adding SOUpdateMultiple message to the queue\n");

	g_GCMgr.InjectGCMessage(message);
}

void LobbyManager::EnterPostGame(EMatchOutcome outcome)
{
	m_Lobby.set_state(CSODOTALobby_State_POSTGAME);
	m_Lobby.set_match_outcome(outcome);
	SendLobbySOUpdate();
}

void LobbyManager::DeleteLobby()
{
	return;

	CMsgSOMultipleObjects objs;
	auto obj = objs.add_objects_removed();
	obj->set_type_id(2004);
	m_Lobby.SerializeToString(obj->mutable_object_data());
	objs.set_version(++s_LobbyVersion);
	objs.set_service_id(0);
	objs.mutable_owner_soid()->set_type(3);
	objs.mutable_owner_soid()->set_id(k_LobbyId);

	uint32 emsg = k_ESOMsg_UpdateMultiple | 0x80000000;
	int headerSize = 0;

	std::string message;
	message.append((const char *)&emsg, sizeof(uint32));
	message.append((const char *)&headerSize, sizeof(int));
	objs.AppendToString(&message);

	Msg("[Stub] Adding SOUpdateMultiple message to the queue\n");

	g_GCMgr.InjectGCMessage(message);
}

void LobbyManager::OnPlayerConnected(const CSteamID &steamId)
{
	for (auto &p : *m_Lobby.mutable_members())
	{
		if (p.id() == steamId.ConvertToUint64())
		{
			p.set_leaver_status(DOTA_LEAVER_NONE);
			break;
		}
	}
}

void LobbyManager::OnPlayerDisconnected(const CSteamID &steamId)
{
	for (auto &p : *m_Lobby.mutable_members())
	{
		if (p.id() == steamId.ConvertToUint64())
		{
			p.set_leaver_status(DOTA_LEAVER_DISCONNECTED);
			break;
		}
	}
}

int LobbyManager::GetConnectedPlayerCount() const
{
	int cnt = 0;
	for (auto m : m_Lobby.members())
	{
		if (m.leaver_status() == DOTA_LEAVER_NONE)
		{
			++cnt;
		}
	}

	return cnt;
}

void LobbyManager::GetNotConnectedPlayerNames(char *pszNames, size_t len)
{
	std::string names;
	bool bGotOne = false;
	for (auto m : m_Lobby.members())
	{
		if (m.leaver_status() != DOTA_LEAVER_NONE)
		{
			if (bGotOne)
				names.append(",");

			names.append(m.name());
			bGotOne = true;
		}
	}

	Q_snprintf(pszNames, len, "%s", names.c_str());
}

void LobbyManager::HandleConnectedPlayers(const CMsgConnectedPlayers &msg)
{
	for (auto &connected : msg.connected_players())
	{
		for (auto &p : *m_Lobby.mutable_members())
		{
			if (p.id() == connected.steam_id())
			{
				p.set_leaver_status(DOTA_LEAVER_NONE);
				p.set_hero_id(connected.hero_id());
				break;
			}
		}
	}

	for (auto &disconnected : msg.disconnected_players())
	{
		for (auto &p : *m_Lobby.mutable_members())
		{
			if (p.id() == disconnected.steam_id())
			{
				p.set_leaver_status(DOTA_LEAVER_DISCONNECTED);
				break;
			}
		}
	}

	m_Lobby.set_first_blood_happened(msg.first_blood_happened());
	m_Lobby.set_game_state(msg.game_state());

	SendLobbySOUpdate();
}

CSteamID LobbyManager::MemberSteamIdFromAccountId(AccountID_t aid)
{
	for (auto m : m_Lobby.members())
	{
		CSteamID sid = CSteamID((uint64)m.id());
		if (sid.GetAccountID() == aid)
			return sid;
	}

	static CSteamID steamIdInvalid = CSteamID();
	return steamIdInvalid;
}
