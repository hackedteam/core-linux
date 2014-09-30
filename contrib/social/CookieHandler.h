
extern BOOL IsInterestingDomainW(WCHAR *domain);
extern BOOL IsInterestingDomainA(char *domain);
extern void ResetNewCookie(void);
extern BOOL AddCookieA(char *domain, char *name, char *value);
extern BOOL AddCookieW(WCHAR *domain, WCHAR *name, WCHAR *value);
extern char *GetCookieString(char *domain);
extern void NormalizeDomainW(WCHAR *domain);
extern void NormalizeDomainA(char *domain);
