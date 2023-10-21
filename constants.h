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

#define MSG_TAG "[D2Lobby] "

const int kMaxTeamPlayers = 5;
const int kMaxGamePlayerIds = 10;
const int kSpectatorIdStart = kMaxGamePlayerIds;
const int kMaxTotalPlayerIds = 32;
const int kMaxBroadcastChannels = 6;
const int kMaxBroadcastChannelSlots = 4;
const int kMaxPlayerNameLength = 128;

// Classic Source colors
extern const char *g_ColorGreen;
extern const char *g_ColorDarkGreen;
extern const char *g_ColorBlue;
extern const char *g_ColorRed;
extern const char *g_ColorYellow;
extern const char *g_ColorGrey;

enum DotaTeam
{
	kTeamUnassigned = 0,
	kTeamSpectators = 1,
	kTeamRadiant = 2,
	kTeamDire = 3,
	kTeamNeutrals = 4,
};

enum DotaRune : int
{
	DoubleDamage,
	Haste,
	Illusion,
	Invisibility,
	Regeneration,
	Bounty,
	Arcane,
};

enum class DotaSeriesType
{
	None,
	BO3,
	BO5,
};

#define HUD_PRINTNOTIFY		1
#define HUD_PRINTCONSOLE	2
#define HUD_PRINTTALK		3
#define HUD_PRINTCENTER		4
