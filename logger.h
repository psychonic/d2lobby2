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
#include <filesystem.h>

class Logger : IPluginSystem
{
public:
	virtual const char *GetName() const override { return "Logger"; }
	virtual bool OnLoad() override;
	virtual void OnUnload() override;
public:
	void SetMatchId(uint64 matchId);
	void LogToFile(const char *pszText)
	{
		if (m_pLogFile)
		{
			InternalLogToFile(pszText);
		}
	}
	template <typename ... Ts>
	void LogToFilef(const char *pMsg, Ts ... ts)
	{
		if (m_pLogFile)
		{
			static char string[1024];
			Q_snprintf(string, sizeof(string), pMsg, ts...);
			InternalLogToFile(string);
		}
	}
private:
	void InternalLogToFile(const char *pszText);
	void OpenNewLog(const char *pszFileName);
private:
	FileHandle_t m_pLogFile = nullptr;
};

extern Logger g_Logger;
