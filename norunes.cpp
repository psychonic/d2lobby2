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

#include "norunes.h"
#include "d2lobby.h"
#include "lobbymgr.h"
#include "util.h"

#include <tier0/icommandline.h>

#include <generated_proto/dota_clientmessages.pb.h>
#include <generated_proto/dota_gcmessages_common.pb.h>

#include <subhook.h>

static NoRunes s_NoRunes;

CON_COMMAND(set_no_runes, "")
{
	s_NoRunes.SetNoRunes();
}

CON_COMMAND(set_no_neutrals, "")
{
	s_NoRunes.SetNoNeutrals();
}

CON_COMMAND(set_no_bottle, "")
{
	s_NoRunes.SetNoBottle();
}

// To block courier, at start, teleport any existing (walking) courier to hidden area.
// BE CAREFUL OF SPOT. REMEMBER TREES CAN BE CHOPPED. STILL BEST TO DISABLE IT SOMEHOW.
// Can hide on cliff and maybe add modifiers to make invulnerable (and unselectable)
// Use Hammer to find coords
//
// MODIFIER_PROPERTY_PERSISTENT_INVISIBILITY 
//MODIFIER_STATE_UNSELECTABLE
//MODIFIER_STATE_INVULNERABLE
//MODIFIER_STATE_INVISIBLE
//https://developer.valvesoftware.com/wiki/Dota_2_Workshop_Tools/Scripting/API/CDOTA_BaseNPC.AddNewModifier
//https://developer.valvesoftware.com/wiki/Dota_2_Workshop_Tools/Scripting/API/CDOTA_BaseNPC.AddNoDraw

// Also block courier and flying courier buys

const int INDEX_BOTTLE = 41;
const int INDEX_COURIER = 45;
const int INDEX_FLYING_COURIER = 84;


static subhook_t s_AEOHook;
void *s_pAEOFunc;

class GenericClass {};
typedef void (GenericClass::*VoidFunc)();

inline void *GetCodeAddr(VoidFunc mfp)
{
	return *(void **)&mfp;
}
#define GetCodeAddress(mfp) GetCodeAddr(reinterpret_cast<VoidFunc>(mfp))

class CDOTAPlayer
{
public:
	void AddExecuteOrders_Hook(const CDOTAClientMsg_ExecuteOrders *);
};
typedef void(CDOTAPlayer::*myfunc)(const CDOTAClientMsg_ExecuteOrders *);

union ThiscallHelper
{
	void *pVoid;
	myfunc mfp;
};
static ThiscallHelper s_ThiscallHelper;

void CDOTAPlayer::AddExecuteOrders_Hook(const CDOTAClientMsg_ExecuteOrders *pOrders)
{
	if (s_NoRunes.GetNoBottle())
	{
		auto *pMutableOrders = const_cast<CDOTAClientMsg_ExecuteOrders *>(pOrders)->mutable_orders();
		for (auto iter = pMutableOrders->begin(), end = pMutableOrders->end(); iter != end; ++iter)
		{
			auto order = *iter;
			if (order.order_type() == DOTA_UNIT_ORDER_PURCHASE_ITEM)
			{
				// check if item is bottle
				//Msg("Got purchase item order:\n");
				//Msg("%s\n", order.DebugString().c_str());

#if 1
				if (order.ability_index() == INDEX_BOTTLE || order.ability_index() == INDEX_COURIER || order.ability_index() == INDEX_FLYING_COURIER)
#else
				if (order.ability_index() == INDEX_BOTTLE)
#endif
				{
					order.set_order_type(DOTA_UNIT_ORDER_NONE);
					//Msg("Blocking bottle purchase!\n");
					//return true;
				}
			}
		}
	}
#if defined( __x86_64__ ) || defined ( _M_X64 )
	subhook_remove(s_AEOHook);
	s_ThiscallHelper.pVoid = s_pAEOFunc;
	(this->*s_ThiscallHelper.mfp)(pOrders);
	subhook_install(s_AEOHook);
#else
	s_ThiscallHelper.pVoid = subhook_get_trampoline(s_AEOHook);
	(this->*s_ThiscallHelper.mfp)(pOrders);
#endif
}

bool NoRunes::OnLoad()
{
#if defined( _WIN64 )
	s_pAEOFunc = UTIL_FindAddress(gamedll, "\x40\x53\x56\x41\x56\x48\x83\xEC\x2A\x33\xDB\x4C", 12);
#elif defined( _WIN32 )
	s_pAEOFunc = UTIL_FindAddress(gamedll, "\x55\x8B\xEC\x83\xEC\x08\x8B\x45\x08\x53\x56\x57\x33\xFF", 14);
#elif defined( __linux__ )
	s_pAEOFunc = UTIL_FindAddress(gamedll, "\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x49\x89\xF5\x41\x54\x53\x48\x83\xEC\x08\x44\x8B\x4E\x20\x45", 25);
#endif
	if (!s_pAEOFunc)
	{
		Msg("Couldn't find AddExecuteOrders!\n");
		return false;
	}

	s_AEOHook = subhook_new(s_pAEOFunc, GetCodeAddress(&CDOTAPlayer::AddExecuteOrders_Hook),
#if defined( __x86_64__ ) || defined( _M_X64 )
		SUBHOOK_OPTION_64BIT_OFFSET
#else
		subhook_options_t(0)
#endif
	);
	subhook_install(s_AEOHook);

	return true;
}

void NoRunes::OnUnload()
{
	subhook_remove(s_AEOHook);
	subhook_free(s_AEOHook);
}

void NoRunes::OnDOTAGameStateChange(uint32 oldState, uint32 state)
{
	if (state == DOTA_GAMERULES_STATE_WAIT_FOR_PLAYERS_TO_LOAD)
	{
		if (m_bNoRunes)
		{
			scriptvm->Run(
				"runespawners = Entities:FindAllByName( \"dota_item_rune_spawner\" )\r\n"
				"for i = 1, #runespawners do\r\n"
					"\trunespawners[i]:SetOrigin(Vector(0.0, 0.0, -100.0))\r\n"
				"end\r\n"
				);
		}

		if (m_bNoNeutrals)
		{
			static ConVarRef dota_neutral_initial_spawn_delay("dota_neutral_initial_spawn_delay");
			dota_neutral_initial_spawn_delay.SetValue(1800000.0f);
		}
	}
#if 0
	else if (state == DOTA_GAMERULES_STATE_PRE_GAME)
	{
		scriptvm->Run(
			"couriers = Entities:FindAllByClassname( \"npc_dota_courier\" )\r\n"
			"for i = 1, #couriers do\r\n"
				"\tcouriers[i]:AddNewModifier(couriers[i], nil, \"modifier_tutorial_sleep\", {})"
			"end\r\n"
			);
	}
#endif
}

void NoRunes::SetNoRunes()
{
	m_bNoRunes = true;
}

void NoRunes::SetNoNeutrals()
{
	m_bNoNeutrals = true;
}

void NoRunes::SetNoBottle()
{
	m_bNoBottle = true;
}
