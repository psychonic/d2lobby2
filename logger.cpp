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

#include "logger.h"
#include "d2lobby.h"

#include <time.h>
#include <inttypes.h>

#include <filesystem.h>
#include <fmtstr.h>
#include <icommandline.h>

Logger g_Logger;

bool Logger::OnLoad()
{
	const char *pszIP = CommandLine()->ParmValue("-ip", "");
	if (!pszIP)
	{
		pszIP = CommandLine()->ParmValue("+ip", "");
		if (!pszIP)
		{
			pszIP = "0.0.0.0";
		}
	}

	static ConVarRef hostport("hostport");

	OpenNewLog(CFmtStrN<32>("%s_%u", pszIP, hostport.GetInt()));

	return true;
}

void Logger::OnUnload()
{
	if (m_pLogFile)
		filesystem->Close(m_pLogFile);
}

void Logger::SetMatchId(uint64 matchId)
{
	if (m_pLogFile)
	{
		filesystem->Close(m_pLogFile);
	}

	char szId[24];
	Q_snprintf(szId, sizeof(szId), "%" PRIu64, matchId);
	OpenNewLog(szId);
}

void Logger::OpenNewLog(const char *pszFileName)
{
	if (!filesystem->IsDirectory("d2lobby_logs"))
		filesystem->CreateDirHierarchy("d2lobby_logs");

	char szLogFile[260];
	Q_snprintf(szLogFile, sizeof(szLogFile), "d2lobby_logs/%s.txt", pszFileName);

	Msg("Opening new log file \"%s\"\n", szLogFile);
	m_pLogFile = filesystem->Open(szLogFile, "a");
}

void Logger::InternalLogToFile(const char *pszText)
{
	static char	string[1100];

	time_t t = time(nullptr);
	tm *today = localtime(&t);

	Q_snprintf(string, sizeof(string), "L %02i/%02i/%04i - %02i:%02i:%02i: %s",
		today->tm_mon + 1, today->tm_mday, 1900 + today->tm_year,
		today->tm_hour, today->tm_min, today->tm_sec, pszText);

	filesystem->FPrintf(m_pLogFile, "%s", string);
}
