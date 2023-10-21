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

#include "util.h"
#include "d2lobby.h"

#include "lobbymgr.h"

#if defined( _WIN32 )
#include <Windows.h>
#elif defined( LINUX )
#include <elf.h>
#endif

void *UTIL_FindAddress(void *startAddr, const char *sig, size_t len)
{
	bool found;
	void *ptr, *end;
#ifdef _WIN32
	MEMORY_BASIC_INFORMATION mem;

	if (!startAddr)
		return nullptr;

	if (!VirtualQuery(startAddr, &mem, sizeof(mem)))
		return nullptr;

	IMAGE_DOS_HEADER *dos = reinterpret_cast<IMAGE_DOS_HEADER *>(mem.AllocationBase);
	IMAGE_NT_HEADERS *pe = reinterpret_cast<IMAGE_NT_HEADERS *>((intp)dos + dos->e_lfanew);

	if (pe->Signature != IMAGE_NT_SIGNATURE)
	{
		// GetDllMemInfo failedpe points to a bad location
		return nullptr;
	}

	ptr = mem.AllocationBase;
	end = (void *)((intptr_t)ptr + pe->OptionalHeader.SizeOfImage - len);

#elif defined(LINUX)
	Dl_info info;
	uint32_t segmentCount;
	size_t memorySize = 0;

	if (!dladdr(startAddr, &info) || !info.dli_fbase || !info.dli_fname)
		return nullptr;

	Elf64_Ehdr *file = reinterpret_cast<Elf64_Ehdr *>(info.dli_fbase);
	Elf64_Phdr *phdr = reinterpret_cast<Elf64_Phdr *>((intp)file + file->e_phoff);

	if (memcmp(ELFMAG, file->e_ident, SELFMAG) != 0)
		return nullptr;

	segmentCount = file->e_phnum;

	for (uint32_t i = 0; i < segmentCount; i++)
	{
		if (phdr[i].p_type == PT_LOAD && phdr[i].p_flags == (PF_X|PF_R))
		{
			int pageSize = getpagesize();
			memorySize += ((phdr[i].p_filesz + pageSize - 1) & ~(pageSize - 1));
		}
	}

	ptr = info.dli_fbase;
	end = (void *)((intptr_t)ptr + memorySize);
#else // LINUX
#error No UTIL_FindAddress impl!
#endif

	while (ptr < end)
	{
		found = true;
		for (size_t i = 0; i < len; i++)
		{
			if (sig[i] != '\x2A' && sig[i] != reinterpret_cast<char *>(ptr)[i])
			{
				found = false;
				break;
			}
		}

		if (found)
			return ptr;

		ptr = (void *)((intptr_t)ptr + 1);
	}

	return nullptr;
}

bool UTIL_IsPlayerConnected(CEntityIndex idx)
{
	static ScriptVariant_t ret;

	static HSCRIPT hCDOTA_PlayerResource = INVALID_HSCRIPT;
	if (hCDOTA_PlayerResource == INVALID_HSCRIPT)
	{
		ScriptVariant_t varPRClassDef;
		scriptvm->GetValue("CDOTA_PlayerResource", &varPRClassDef);
		hCDOTA_PlayerResource = varPRClassDef.m_hScript;
	}

	static HSCRIPT hGetConnectionState = scriptvm->LookupFunction("GetConnectionState", hCDOTA_PlayerResource);

	static HSCRIPT hPlayerResource = INVALID_HSCRIPT;
	if (hPlayerResource == INVALID_HSCRIPT)
	{
		scriptvm->GetValue("PlayerResource", &ret);
		hPlayerResource = ret.m_hScript;
	}

	static HSCRIPT hEntIndexToHScript = scriptvm->LookupFunction("EntIndexToHScript");

	static HSCRIPT hCBaseEntity = INVALID_HSCRIPT;
	if (hCBaseEntity == INVALID_HSCRIPT)
	{
		ScriptVariant_t varCBEClassDef;
		scriptvm->GetValue("CBaseEntity", &varCBEClassDef);
		hCBaseEntity = varCBEClassDef.m_hScript;
	}

	static HSCRIPT hIsPlayer = scriptvm->LookupFunction("IsPlayer", hCBaseEntity);

	static HSCRIPT hCDOTAPlayer = INVALID_HSCRIPT;
	if (hCDOTAPlayer == INVALID_HSCRIPT)
	{
		ScriptVariant_t varCDPClassDef;
		scriptvm->GetValue("CDOTAPlayer", &varCDPClassDef);
		hCDOTAPlayer = varCDPClassDef.m_hScript;
	}

	static HSCRIPT hGetPlayerID = scriptvm->LookupFunction("GetPlayerID", hCDOTAPlayer);

	scriptvm->Call<int>(hEntIndexToHScript, nullptr, true, &ret, idx.Get());
	HSCRIPT hPlayer = ret.m_hScript;
	if (hPlayer == INVALID_HSCRIPT)
		return false;

	scriptvm->Call<HSCRIPT>(hIsPlayer, nullptr, true, &ret, hPlayer);
	if (!ret.m_bool)
		return false;

	scriptvm->Call<HSCRIPT>(hGetPlayerID, nullptr, true, &ret, hPlayer);
	int playerId = (int)ret.m_float64;

	scriptvm->Call<HSCRIPT, int>(hGetConnectionState, nullptr, true, &ret, hPlayerResource, playerId);

	if ((DOTAConnectionState_t)(int)ret.m_float64 != DOTA_CONNECTION_STATE_CONNECTED)
		return false;

	return true;
}

CSteamID UTIL_PlayerIdToSteamId(int playerId)
{
	// Look up class def. It is the scope when looking up a class func.
	// Then look up the class instance var. It is prepended to args when calling a class func, like a thiscall.

	ScriptVariant_t varPRClassDef;
	scriptvm->GetValue("CDOTA_PlayerResource", &varPRClassDef);

	HSCRIPT hPRClassScope = varPRClassDef.m_hScript;
	HSCRIPT steamAccountId = scriptvm->LookupFunction("GetSteamAccountID", hPRClassScope);

	ScriptVariant_t varPRInstance;
	scriptvm->GetValue("PlayerResource", &varPRInstance);

	HSCRIPT hPRIntance = varPRInstance.m_hScript;

	ScriptVariant_t ret;
	scriptvm->Call<HSCRIPT, int>(steamAccountId, nullptr, true, &ret, hPRIntance, playerId);
	AccountID_t acctId = (AccountID_t)ret.m_float64;

	return g_LobbyMgr.MemberSteamIdFromAccountId(acctId);
}
