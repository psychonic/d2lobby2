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

#include "eventlog.h"

#include "d2lobby.h"
#include "lobbymgr.h"
#include "httpmgr.h"
#include "util.h"

#include <filesystem.h>
#include <fmtstr.h>

#include <jansson.h>

#include <steam/steam_gameserver.h>

#include <generated_proto/dota_usermessages.pb.h>

EventLogger g_EventLogger;

extern ConVar match_post_url;

SH_DECL_HOOK2_void(ConCommand, Dispatch, SH_NOATTRIB, 0, const CCommandContext &, const CCommand &);
SH_DECL_HOOK1_void(ISource2GameClients, SetCommandClient, SH_NOATTRIB, 0, CPlayerSlot);

bool EventLogger::OnLoad()
{
	ConCommand *pSay = g_pCVar->FindCommand("say");
	ConCommand *pGG = g_pCVar->FindCommand("dota_call_gg");
	ConCommand *pCancelGG = g_pCVar->FindCommand("dota_cancel_GG");

	if (!pSay)
	{
		Msg("Couldn't find \"say\" command\n");
		return false;
	}

	if (!pGG)
	{
		Msg("Couldn't find \"dota_call_gg\" command\n");
		return false;
	}

	if (!pCancelGG)
	{
		Msg("Couldn't find \"dota_cancel_GG\" command\n");
		return false;
	}

	int h;

	h = SH_ADD_HOOK(ConCommand, Dispatch, pSay, SH_MEMBER(this, &EventLogger::Hook_OnCmdSay), true);
	m_Hooks.push_back(h);

	h = SH_ADD_HOOK(ConCommand, Dispatch, pGG, SH_MEMBER(this, &EventLogger::Hook_OnCmdGG), true);
	m_Hooks.push_back(h);

	h = SH_ADD_HOOK(ConCommand, Dispatch, pCancelGG, SH_MEMBER(this, &EventLogger::Hook_OnCmdCancelGG), true);
	m_Hooks.push_back(h);

	h = SH_ADD_HOOK(ISource2GameClients, SetCommandClient, serverclients, SH_MEMBER(this, &EventLogger::Hook_SetCommandClient), true);
	m_Hooks.push_back(h);

	return true;
}

void EventLogger::OnUnload()
{
	for (int h : m_Hooks)
	{
		SH_REMOVE_HOOK_ID(h);
	}
}

void EventLogger::SendAndFreeEvent(json_t *pData)
{
#pragma message ("Remember to queue events or something if ISteamHTTP isn't available yet")
	if (!http)
	{
		UTIL_LogToFile("Can't send event, ISteamHTTP not available!!!!!!\n");
		return;
	}

	json_t *pContainer = json_object();
	json_t *pEvents = json_array();
	json_array_append_new(pEvents, pData);
	json_object_set_new(pContainer, "match_id", json_integer(g_LobbyMgr.MatchId()));
	json_object_set_new(pContainer, "events", pEvents);
	json_object_set_new(pContainer, "status", json_string("events"));
	json_object_set_new(pContainer, "has_events", json_boolean(true));

	char *pszOutput = json_dumps(pContainer, JSON_COMPACT);
	json_decref(pContainer);

	UTIL_LogToFile("Sending event:\n%s\n", pszOutput);

	if (match_post_url.GetString()[0])
	{
		g_HTTPManager.PostJSONToMatchUrl(pszOutput);
	}

	free(pszOutput);
}

json_t *EventLogger::CreateTimedEvent(EventType type)
{
	json_t *pEvent = json_object();
	//json_object_set_new(pEvent, "time", json_real(g_GameRules.GetGameTime()));
	json_object_set_new(pEvent, "time", json_real(Plat_FloatTime()));
	json_object_set_new(pEvent, "event_type", json_integer((int)type));

	return pEvent;
}

void ChatDebug(CDOTAUserMsg_ChatEvent &msg)
{
	Msg("Chat event %s\n", DOTA_CHAT_MESSAGE_Name(msg.type()).c_str());
	if (msg.has_value())
		Msg("- Value: %u\n", msg.value());
	if (msg.has_value2())
		Msg("- Value2: %u\n", msg.value2());
	if (msg.has_value3())
		Msg("- Value3: %u\n", msg.value3());
	if (msg.has_playerid_1())
		Msg("- Player1: %d\n", msg.playerid_1());
	if (msg.has_playerid_2())
		Msg("- Player2: %d\n", msg.playerid_2());
	if (msg.has_playerid_3())
		Msg("- Player3: %d\n", msg.playerid_3());
	if (msg.has_playerid_4())
		Msg("- Player4: %d\n", msg.playerid_4());
	if (msg.has_playerid_5())
		Msg("- Player5: %d\n", msg.playerid_5());
	if (msg.has_playerid_6())
		Msg("- Player6: %d\n", msg.playerid_6());
}

void EventLogger::OnGameEvent(uint16 id, const void *pData, int clientCount, const unsigned char *clients)
{
	switch (id)
	{
		case DOTA_UM_ChatEvent:
		{
			auto &chatEvent = *(CDOTAUserMsg_ChatEvent *) pData;
			switch (chatEvent.type())
			{
			case CHAT_MESSAGE_COURIER_LOST:
				ChatDebug(chatEvent);
				LogCourierKill(chatEvent.playerid_1() == kTeamDire ? kTeamRadiant : kTeamDire);
				break;
			case CHAT_MESSAGE_HERO_KILL:
				{
					std::vector<int> vecKillers;
					if (chatEvent.has_playerid_2())
					{
						vecKillers.push_back(chatEvent.playerid_2());
						if (chatEvent.has_playerid_3())
						{
							vecKillers.push_back(chatEvent.playerid_3());
							if (chatEvent.has_playerid_4())
							{
								vecKillers.push_back(chatEvent.playerid_4());
								if (chatEvent.has_playerid_5())
								{
									vecKillers.push_back(chatEvent.playerid_5());
									if (chatEvent.has_playerid_6())
									{
										vecKillers.push_back(chatEvent.playerid_6());
									}
								}
							}
						}
					}
					LogHeroKill(chatEvent.playerid_1(), vecKillers, chatEvent.value());
				}
				break;
			case CHAT_MESSAGE_RUNE_BOTTLE:
				LogRuneBottle(chatEvent.playerid_1(), (DotaRune) chatEvent.value());
				break;
			case CHAT_MESSAGE_RUNE_PICKUP:
				LogRuneUse(chatEvent.playerid_1(), (DotaRune) chatEvent.value());
				break;
			case CHAT_MESSAGE_ROSHAN_KILL:
				ChatDebug(chatEvent);
				LogRoshanKill(chatEvent.playerid_1(), chatEvent.value());
				break;
			case CHAT_MESSAGE_AEGIS:
				LogSimplePlayerEvent(EventType::AegisPickup, chatEvent.playerid_1());
				break;
			case CHAT_MESSAGE_AEGIS_STOLEN:
				LogSimplePlayerEvent(EventType::AegisSteal, chatEvent.playerid_1());
				break;
			//case CHAT_MESSAGE_HERO_DENY:
			case CHAT_MESSAGE_BUYBACK:
				LogSimplePlayerEvent(EventType::Buyback, chatEvent.playerid_1());
				break;
				// player id
			case CHAT_MESSAGE_DENIED_AEGIS:
				LogSimplePlayerEvent(EventType::AegisDeny, chatEvent.playerid_1());
				break;
			case CHAT_MESSAGE_FIRSTBLOOD:
				LogSimplePlayerEvent(EventType::FirstBlood, chatEvent.playerid_1());
				break;
			case CHAT_MESSAGE_TOWER_DENY:
				LogSimplePlayerEvent(EventType::TowerDeny, chatEvent.playerid_1());
				break;
			case CHAT_MESSAGE_TOWER_KILL:
				LogTowerKill(chatEvent.playerid_1(), chatEvent.value());
				break;
#if 0
			case CHAT_MESSAGE_ITEM_PURCHASE:
				{
					bool skip = false;
					int ourTeam = g_LobbyMgr.GetPlayerTeam(chatEvent.playerid_1());
					for (int i = 0; i < clientCount; ++i)
					{
						const CSteamID *sid = engine->GetClientSteamID(clients[i]);
						int team = g_LobbyMgr.GetPlayerTeam(*sid);
						if (team != ourTeam)
						{
							skip = true;
							break;
						}
					}

					if (!skip)
					{
						LogItemPurchase(chatEvent.playerid_1(), chatEvent.value());
					}
				}
				break;
#endif
			}
		}
		break;
		case DOTA_UM_ChatWheel:
		{
			auto &chatWheel = *(CDOTAUserMsg_ChatWheel *)pData;
			if (chatWheel.chat_message() == k_EDOTA_CW_All_GG || chatWheel.chat_message() == k_EDOTA_CW_All_GGWP)
			{
				HandlePossibleGG(CSteamID(chatWheel.account_id(), k_unSteamUserDefaultInstance, k_EUniversePublic, k_EAccountTypeIndividual));
			}
		}
		break;
	}
}

void EventLogger::HandlePossibleGG(const CSteamID &sid)
{
	if (g_LobbyMgr.GetGameState() == DOTA_GAMERULES_STATE_GAME_IN_PROGRESS && m_GGTeam == kTeamUnassigned)
	{
		int team = g_LobbyMgr.GetPlayerTeam(sid);
		if (team == kTeamRadiant || team == kTeamDire)
		{
			m_GGTeam = (DotaTeam)team;
			g_EventLogger.LogGGCall(sid.ConvertToUint64(), team);
		}
	}
}

void EventLogger::Hook_OnCmdSay(const CCommandContext &ctx, const CCommand &args)
{
	if (!Q_stricmp(args.ArgS(), "\"gg\""))
	{
		const CSteamID *sid = engine->GetClientSteamID(m_CommandClient.Get() + 1);
		if (sid)
		{
			HandlePossibleGG(*sid);
		}
	}
	RETURN_META(MRES_SUPERCEDE);
}

void EventLogger::Hook_OnCmdGG(const CCommandContext &ctx, const CCommand &args)
{
	RETURN_META(MRES_SUPERCEDE);
}

void EventLogger::Hook_OnCmdCancelGG(const CCommandContext &ctx, const CCommand &args)
{
	if (g_LobbyMgr.GetGameState() == DOTA_GAMERULES_STATE_GAME_IN_PROGRESS && m_GGTeam != kTeamUnassigned)
	{
		const CSteamID *sid = engine->GetClientSteamID(m_CommandClient.Get() + 1);
		if (sid)
		{
			int team = g_LobbyMgr.GetPlayerTeam(*sid);
			if (team == m_GGTeam)
			{
				m_GGTeam = kTeamUnassigned;
				g_EventLogger.LogGGCancel(sid->ConvertToUint64(), team);
			}
		}
	}

	RETURN_META(MRES_SUPERCEDE);
}

void EventLogger::Hook_SetCommandClient(CPlayerSlot slot)
{
	m_CommandClient = slot;
	RETURN_META(MRES_SUPERCEDE);
}


void EventLogger::OnDOTAGameStateChange(uint32 oldState, uint32 newState)
{
	json_t *pEvent = CreateTimedEvent(EventType::GameStateChange);
	json_object_set_new(pEvent, "new_state", json_integer(newState));

	SendAndFreeEvent(pEvent);
}

void EventLogger::LogPlayerConnect(const char *pszName, const CSteamID &steamId)
{
	json_t *pEvent = CreateTimedEvent(EventType::PlayerConnect);
	json_object_set_new(pEvent, "player", json_integer(steamId.ConvertToUint64()));

	SendAndFreeEvent(pEvent);
}

void EventLogger::LogPlayerDisconnect(const char *pszName, const CSteamID &steamId, int reason)
{
	json_t *pEvent = CreateTimedEvent(EventType::PlayerDisconnect);
	json_object_set_new(pEvent, "player", json_integer(steamId.ConvertToUint64()));
	json_object_set_new(pEvent, "reason", json_integer(reason));

	SendAndFreeEvent(pEvent);
}

#if 0
void EventLogger::LogPlayerTeam(const char *pszName, int userid, int team)
{
	json_t *pEvent = CreateTimedEvent(EventType::PlayerTeam);
	auto *sid = UserIdToCSteamID(userid);
	json_object_set_new(pEvent, "player", json_integer(sid ? sid->ConvertToUint64() : 0));
	json_object_set_new(pEvent, "team", json_integer(team));

	SendAndFreeEvent(pEvent);
}
#endif

void EventLogger::LogHeroKill(int victimId, std::vector<int> &vecKillers, uint gold)
{
	json_t *pEvent = CreateTimedEvent(EventType::HeroDeath);
	json_object_set_new(pEvent, "player", json_integer(UTIL_PlayerIdToSteamId(victimId).ConvertToUint64()));
	json_object_set_new(pEvent, "gold", json_integer(gold));
	json_t *pKillers = json_array();
	for (int k : vecKillers)
	{
		if (k != -1)
			json_array_append_new(pKillers, json_integer(UTIL_PlayerIdToSteamId(k).ConvertToUint64()));
	}	
	json_object_set_new(pEvent, "killers", pKillers);

	SendAndFreeEvent(pEvent);
}

void EventLogger::LogCourierKill(int team)
{
	json_t *pEvent = CreateTimedEvent(EventType::CourierKill);
	json_object_set_new(pEvent, "team", json_integer(team));

	SendAndFreeEvent(pEvent);
}

void EventLogger::LogRoshanKill(int team, uint gold)
{
	json_t *pEvent = CreateTimedEvent(EventType::RoshanKill);
	json_object_set_new(pEvent, "team", json_integer(team));
	json_object_set_new(pEvent, "gold", json_integer(gold));

	SendAndFreeEvent(pEvent);
}

void EventLogger::LogTowerKill(int playerId, int team)
{
	json_t *pEvent = CreateTimedEvent(EventType::TowerKill);
	json_object_set_new(pEvent, "player", json_integer(UTIL_PlayerIdToSteamId(playerId).ConvertToUint64()));
	json_object_set_new(pEvent, "team", json_integer(team));

	SendAndFreeEvent(pEvent);
}

void EventLogger::LogSimplePlayerEvent(EventType type, int playerId)
{
	json_t *pEvent = CreateTimedEvent(type);
	json_object_set_new(pEvent, "player", json_integer(UTIL_PlayerIdToSteamId(playerId).ConvertToUint64()));

	SendAndFreeEvent(pEvent);
}

void EventLogger::LogRuneBottle(int playerId, DotaRune rune)
{
	json_t *pEvent = CreateTimedEvent(EventType::RuneBottled);
	json_object_set_new(pEvent, "player", json_integer(UTIL_PlayerIdToSteamId(playerId).ConvertToUint64()));
	json_object_set_new(pEvent, "rune_type", json_integer((int) rune));

	SendAndFreeEvent(pEvent);
}

void EventLogger::LogRuneUse(int playerId, DotaRune rune)
{
	json_t *pEvent = CreateTimedEvent(EventType::RuneUsed);
	json_object_set_new(pEvent, "player", json_integer(UTIL_PlayerIdToSteamId(playerId).ConvertToUint64()));
	json_object_set_new(pEvent, "rune_type", json_integer((int) rune));

	SendAndFreeEvent(pEvent);
}

void EventLogger::LogItemPurchase(int playerId, int itemId)
{
	json_t *pEvent = CreateTimedEvent(EventType::ItemPurchase);
	json_object_set_new(pEvent, "player", json_integer(UTIL_PlayerIdToSteamId(playerId).ConvertToUint64()));
	json_object_set_new(pEvent, "item_id", json_integer(itemId));

	SendAndFreeEvent(pEvent);
}

void EventLogger::LogGGCall(uint64 steamId64, int team)
{
	json_t *pEvent = CreateTimedEvent(EventType::CallGG);
	json_object_set_new(pEvent, "player", json_integer(steamId64));
	json_object_set_new(pEvent, "team", json_integer(team));

	SendAndFreeEvent(pEvent);
}

void EventLogger::LogGGCancel(uint64 steamId64, int team)
{
	json_t *pEvent = CreateTimedEvent(EventType::CancelGG);
	json_object_set_new(pEvent, "player", json_integer(steamId64));
	json_object_set_new(pEvent, "team", json_integer(team));

	SendAndFreeEvent(pEvent);
}
