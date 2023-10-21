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

#include "httpmgr.h"

#include "util.h"

extern ConVar match_post_url;

HTTPManager g_HTTPManager;

#undef strdup

HTTPManager::TrackedRequest::TrackedRequest(HTTPRequestHandle hndl, SteamAPICall_t hCall, const char *pszText)
{
	m_hHTTPReq = hndl;
	m_CallResult.SetGameserverFlag();
	m_CallResult.Set(hCall, this, &TrackedRequest::OnHTTPRequestCompleted);

	m_pszText = strdup(pszText);

	g_HTTPManager.m_PendingRequests.push_back(this);
}

HTTPManager::TrackedRequest::~TrackedRequest()
{
	for (auto e = g_HTTPManager.m_PendingRequests.begin(); e != g_HTTPManager.m_PendingRequests.end(); ++e)
	{
		if (*e == this)
		{
			g_HTTPManager.m_PendingRequests.erase(e);
			break;
		}
	}

	free(m_pszText);
}

void HTTPManager::TrackedRequest::OnHTTPRequestCompleted(HTTPRequestCompleted_t *arg, bool bFailed)
{
	if (bFailed || arg->m_eStatusCode < 200 || arg->m_eStatusCode > 299)
	{
		g_HTTPManager.PostJSONToMatchUrl(m_pszText);
	}
	else
	{
		uint32 size;
		http->GetHTTPResponseBodySize(arg->m_hRequest, &size);

		uint8 *response = new uint8[size];
		http->GetHTTPResponseBodyData(arg->m_hRequest, response, size);

		if (size < 2 || response[0] != 'o' || response[1] != 'k')
		{
			g_HTTPManager.PostJSONToMatchUrl(m_pszText);
		}
	}

	if (http)
	{
		http->ReleaseHTTPRequest(arg->m_hRequest);
	}
	delete this;
}

void HTTPManager::PostJSONToMatchUrl(const char *pszText)
{
	//	UTIL_MsgAndLog("Sending HTTP:\n%s\n", pszText);

	if (!match_post_url.GetString()[0])
	{
		return;
	}

	auto hReq = http->CreateHTTPRequest(k_EHTTPMethodPOST, match_post_url.GetString());

	//	UTIL_MsgAndLog("HTTP request: %p\n", hReq);

	int size = strlen(pszText);
	if (http->SetHTTPRequestRawPostBody(hReq, "application/json", (uint8 *)pszText, size))
	{
		SteamAPICall_t hCall;
		http->SendHTTPRequest(hReq, &hCall);

		new TrackedRequest(hReq, hCall, pszText);
	}
	else
	{
		//		UTIL_MsgAndLog("Failed to SetHTTPRequestRawPostBody\n");
	}
}