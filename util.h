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

#include "logger.h"

template <typename ... Ts>
void UTIL_LogToFile(const char *pMsg, Ts ... ts)
{
	g_Logger.LogToFilef(pMsg, ts...);
}

template <typename ... Ts>
void UTIL_MsgAndLog(const char *pMsg, Ts ... ts)
{
	Msg(pMsg, ts...);
	g_Logger.LogToFilef(pMsg, ts...);
}

void *UTIL_FindAddress(void *startAddr, const char *sig, size_t len);

bool UTIL_IsPlayerConnected(CEntityIndex idx);

CSteamID UTIL_PlayerIdToSteamId(int playerId);

