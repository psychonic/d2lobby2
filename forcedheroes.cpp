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

#include "forcedheroes.h"

#include "d2lobby.h"
#include "lobbymgr.h"
#include "util.h"

#include <filesystem.h>
#include <fmtstr.h>
#include <KeyValues.h>
#include <utlhashdict.h>
#include <utlvector.h>
#include <vstdlib/random.h>

#include <inttypes.h>

#include <generated_proto/dota_gcmessages_common.pb.h>

static ForcedHeroes s_ForcedHeroes;

typedef CUtlHashDict<bool, false, true> BlockedHeroList;
static BlockedHeroList s_BlockedHeroes;

static CUtlVector<const char *> s_ValidHeroes;

SH_DECL_HOOK2_void(ISource2GameClients, ClientCommand, SH_NOATTRIB, 0, CEntityIndex, const CCommand &);

#define HERO_NAME_BASE "npc_dota_hero_"

CON_COMMAND(set_blocked_hero, "set_blocked_hero <short hero entity name> [short hero entity name] ...")
{
	if (args.ArgC() < 2)
	{
		Msg("set_blocked_hero <short hero entity name> [short hero entity name] ...\n");
		return;
	}

	char hero[64] = HERO_NAME_BASE;

	for (int i = 1; i < args.ArgC(); ++i)
	{
		Q_snprintf(&hero[sizeof(HERO_NAME_BASE) - 1], sizeof(hero) - sizeof(HERO_NAME_BASE) + 1, "%s", args[i]);
		s_BlockedHeroes.Insert(hero);

		FOR_EACH_VEC_BACK(s_ValidHeroes, j)
		{
			if (!Q_strcmp(s_ValidHeroes[j], hero))
			{
				s_ValidHeroes.Remove(j);
				break;
			}
		}
	}
}

CON_COMMAND(set_forced_hero, "set_forced_hero <hero entity name>")
{
	if (args.ArgC() < 2)
	{
		Msg("set_forced_hero <hero entity name>\n");
		return;
	}

	s_ForcedHeroes.SetHero(args[1]);
}

bool ForcedHeroes::OnLoad()
{
	m_szForcedHero[0] = '\0';
	m_pkvHeroes = new KeyValues("DOTAHeroes");
	bool bLoadedHeros = m_pkvHeroes->LoadFromFile(filesystem, "scripts/npc/npc_heroes.txt");

	FOR_EACH_SUBKEY(m_pkvHeroes, h)
	{
		if (!h->GetBool("Enabled"))
			continue;

		s_ValidHeroes.AddToTail(h->GetName());
	}

	int cmdHook = SH_ADD_HOOK(ISource2GameClients, ClientCommand, serverclients, SH_MEMBER(this, &ForcedHeroes::Hook_ClientCommand), false);

	return bLoadedHeros && cmdHook;
}

void ForcedHeroes::OnUnload()
{
	SH_REMOVE_HOOK(ISource2GameClients, ClientCommand, serverclients, SH_MEMBER(this, &ForcedHeroes::Hook_ClientCommand), false);

	s_ValidHeroes.Purge();
	m_pkvHeroes->deleteThis();

	s_BlockedHeroes.Purge();
}

void ForcedHeroes::SetHero(const char *pszHero)
{
	if (!Q_stricmp(pszHero, "random"))
	{
		const char *pszRandomHero = s_ValidHeroes[RandomInt(0, s_ValidHeroes.Count() - 1)];
		Q_strncpy(m_szForcedHero, pszRandomHero, sizeof(m_szForcedHero));
	}
	else if (!m_pkvHeroes->FindKey(pszHero))
	{
		Msg("Cannot find hero named \"%s\"\n", pszHero);
	}
	else
	{
		Q_strncpy(m_szForcedHero, pszHero, sizeof(m_szForcedHero));
	}
}

static CUtlVector<CSteamID> s_NoRepick;

void ForcedHeroes::OnDOTAGameStateChange(uint32 oldState, uint32 state)
{
	if (state == DOTA_GAMERULES_STATE_HERO_SELECTION)
	{
		for (int i = 1; i <= engine->GetServerGlobals()->maxClients; ++i)
		{
			if (!UTIL_IsPlayerConnected(i))
				continue;

			const char *pszHero;
			auto *sid = engine->GetClientSteamID(i);
			if (sid && (pszHero = g_LobbyMgr.GetPlayerHero(*sid)))
			{
				CCommand args;
				args.Tokenize(CFmtStr("dota_select_hero %s reserve", pszHero));
				serverclients->ClientCommand(i, args);
			}
			else if (m_szForcedHero[0])
			{
				CCommand args;
				args.Tokenize(CFmtStr("dota_select_hero %s reserve", m_szForcedHero));
				serverclients->ClientCommand(i, args);
			}
		}
	}
	else if (state == DOTA_GAMERULES_STATE_STRATEGY_TIME)
	{
		CUtlVector<CSteamID> players;
		g_LobbyMgr.GetPlayersWithoutHeroPicks(players);

		for (int i = 1; i <= engine->GetServerGlobals()->maxClients; ++i)
		{
			const CSteamID *pId = engine->GetClientSteamID(i);
			if (pId && players.Find(*pId) != -1)
			{
				s_NoRepick.AddToTail(*pId);
				PickRandomHero(i);
			}
		}
	}
}

void ForcedHeroes::PickRandomHero(CEntityIndex idx)
{
	const char *pszRandomHero = s_ValidHeroes[RandomInt(0, s_ValidHeroes.Count() - 1)];

	CCommand args;
	args.Tokenize(CFmtStr("dota_select_hero %s reserve", pszRandomHero));

	serverclients->ClientCommand(idx, args);
}

void ForcedHeroes::Hook_ClientCommand(CEntityIndex ent, const CCommand &args)
{
	const CSteamID *sid = engine->GetClientSteamID(ent);
	// dota_select_hero repick
	if (args.ArgC() >= 2)
	{
		if (!Q_stricmp(args[0], "possible_hero") && s_BlockedHeroes.Find(args[1]) != BlockedHeroList::InvalidHandle())
		{
			RETURN_META(MRES_SUPERCEDE);
		}
		else if (!Q_stricmp(args[0], "dota_select_hero"))
		{
			if (!Q_stricmp(args[1], "repick"))
			{
				if (m_szForcedHero[0] || (sid && s_NoRepick.Find(*sid) > -1))
				{
					// Block if forced hero is set (we bypass this already to set)
					UTIL_MsgAndLog("blocked! (Repick not allowed with set_forced_hero)\n");
					RETURN_META(MRES_SUPERCEDE);
				}
			}
			else if (s_BlockedHeroes.Count() > 0)
			{
				if (!Q_stricmp(args[1], "random"))
				{
					PickRandomHero(ent);
					UTIL_MsgAndLog("blocked! (set_blocked_hero enabled. Initiating internal random that excluded blocked heroes)\n");
					RETURN_META(MRES_SUPERCEDE);
				}
				else if (s_BlockedHeroes.Find(args[1]) != BlockedHeroList::InvalidHandle())
				{
					UTIL_MsgAndLog("blocked! (set_blocked_hero enabled and this hero is blocked)\n");
					RETURN_META(MRES_SUPERCEDE);
				}
			}
		}
	}
	
	UTIL_MsgAndLog("allowed\n");

	RETURN_META(MRES_IGNORED);
}
