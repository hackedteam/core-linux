extern void SocialWinHttpSetup(WCHAR *DestURL);
extern DWORD HttpSocialRequest(WCHAR *Host, WCHAR *verb, WCHAR *resource, DWORD port, BYTE *s_buffer, DWORD sbuf_len, BYTE **r_buffer, DWORD *response_len, char *cookies);

