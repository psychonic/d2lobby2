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

#include <sourcehook.h>
#include <eiface.h>
#include <vector>

class IPluginSystem
{
public:
	IPluginSystem();
public:
	virtual ~IPluginSystem() {}
	virtual const char *GetName() const = 0;
	virtual bool OnLoad() { return true; }
	virtual void OnUnload() {}
	virtual void OnServerActivated() {}
	virtual void OnLevelShutdown() {}
	virtual void OnDOTAGameStateChange(uint32 oldState, uint32 newState) {}
};

std::vector<IPluginSystem *> &PluginSystems();