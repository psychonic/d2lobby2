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

#include "d2lobby.h"

#include <filesystem.h>
#include <tier1/fmtstr.h>

static bool s_bScriptLastCompileScriptMissing = false;
static int s_ScriptServerRunScriptDepth = 0;

static HSCRIPT VScriptServerCompileScript(const char *pszScriptName, bool bWarnMissing)
{
	s_bScriptLastCompileScriptMissing = false;

	if (!scriptvm)
	{
		return NULL;
	}

	const char *pszClearVMExtension = ".lua";
	const char *pszIncomingExtension = V_strrchr(pszScriptName, '.');
	if (pszIncomingExtension && (V_strcmp(pszIncomingExtension, pszClearVMExtension) != 0))
	{
		Msg("Script file type does not match VM type\n");
		return NULL;
	}

	CFmtStr clearScriptPath;
	if (pszIncomingExtension)
	{
		clearScriptPath.sprintf("scripts/vscripts/%s", pszScriptName);
	}
	else
	{
		clearScriptPath.sprintf("scripts/vscripts/%s%s", pszScriptName, pszClearVMExtension);
	}

	CUtlBuffer bufferScript;


	// Prefer the unencrypted form of the script if available
	bool bResult = filesystem->ReadFile(clearScriptPath, "GAME", bufferScript);

	if (!bResult)
	{
		// No compiled one available - abort
		if (bWarnMissing)
		{
			DevMsg("Script not found (%s) \n", pszScriptName);
		}
		s_bScriptLastCompileScriptMissing = true;
		return NULL;
	}

	const char *pBase = (const char *)bufferScript.Base();

	// empty file - set the "missing, not failed compile" flag, then return NULL
	if (bufferScript.TellPut() == 0)
	{
		s_bScriptLastCompileScriptMissing = true;
		return NULL;
	}

	if (!pBase || !*pBase)
	{
		return NULL;
	}

	char szFullPath[MAX_PATH];
	const char *pFullPath = filesystem->RelativePathToFullPath(clearScriptPath.Access(), "GAME", szFullPath, ARRAYSIZE(szFullPath));

	if (!pFullPath)
	{
		//The reported name affects squirrel debugging. Breakpoints will not work if the relative path does not match up here and in the squirrel project.
		//We might want to always report the clear name. There's no clearly right way to do it since I don't believe we could possibly debug an encrypted file.
		V_strncpy(szFullPath, clearScriptPath.Access(), ARRAYSIZE(szFullPath));
		pFullPath = szFullPath;
	}

	V_FixSlashes(szFullPath, '/'); //all scripts paths use forward slashes for standardization

	HSCRIPT hScript = scriptvm->CompileScript(pBase, szFullPath);
	if (!hScript)
	{
		DevMsg("FAILED to compile and execute script file named %s\n", pszScriptName);
	}
	return hScript;
}

CON_COMMAND(script_execute, "")
{
	if (!*args[1])
	{
		Msg("No script specified\n");
		return;
	}

	if (!scriptvm)
	{
		Msg("Scripting disabled or no server running\n");
		return;
	}

	// Prevent infinite recursion in VM
	if (s_ScriptServerRunScriptDepth > 16)
	{
		Warning("IncludeScript stack overflow\n");
		return;
	}

	s_ScriptServerRunScriptDepth++;
	HSCRIPT	hScript = VScriptServerCompileScript(args[1], true);
	bool bSuccess = false;
	if (hScript)
	{
		bSuccess = (scriptvm->Run(hScript) != SCRIPT_ERROR);
		if (!bSuccess)
		{
			DevMsg("Error running script named %s\n", args[1]);
		}
		scriptvm->ReleaseScript(hScript);
	}
	s_ScriptServerRunScriptDepth--;
}

CON_COMMAND(script, "Run the text as a script")
{
	if (!*args[1])
	{
		Msg("No script specified\n");
		return;
	}

	if (!scriptvm)
	{
		Msg("Scripting disabled or no server running\n");
		return;
	}

	const char *pszScript = args.GetCommandString();
	pszScript += 6;

	while (*pszScript == ' ')
	{
		pszScript++;
	}

	if (!*pszScript)
	{
		return;
	}

	if (*pszScript != '\"')
	{
		scriptvm->Run(pszScript);
	}
	else
	{
		pszScript++;
		const char *pszEndQuote = pszScript;
		while (*pszEndQuote != '\"')
		{
			pszEndQuote++;
		}
		if (!*pszEndQuote)
		{
			return;
		}
		*((char *)pszEndQuote) = 0;
		scriptvm->Run(pszScript);
		*((char *)pszEndQuote) = '\"';
	}
}
