#ifndef _EVIDENCEMANAGER_H
#define _EVIDENCEMANAGER_H 1

#define EVIDENCE_TYPE_DEVICE      0x0240
#define EVIDENCE_TYPE_POSITION    0x1220
#define EVIDENCE_TYPE_SCREENSHOT  0xB9B9
#define EVIDENCE_TYPE_INFO        0x0241
#define EVIDENCE_TYPE_CAMERA      0xE9E9
#define EVIDENCE_TYPE_MOUSE       0x0280
#define EVIDENCE_TYPE_DOWNLOAD    0xD0D0
#define EVIDENCE_TYPE_PASSWORD    0xFAFA
#define EVIDENCE_TYPE_URL         0x0180
#define EVIDENCE_TYPE_MAIL        0x1001
#define EVIDENCE_TYPE_ADDRESSBOOK 0x0200
#define EVIDENCE_TYPE_CHAT        0xC6C7
#define EVIDENCE_TYPE_CALLLIST    0x0231
#define EVIDENCE_TYPE_KEYLOG      0x0040
#define EVIDENCE_TYPE_FILESYSTEM  0xEDA1
#define EVIDENCE_TYPE_APPLICATION 0x1011
#define EVIDENCE_TYPE_EXEC        0xC0C1
#define EVIDENCE_TYPE_MIC         0xC2C2
#define EVIDENCE_TYPE_MONEY       0xB1C0

#include <openssl/bio.h>

BIO *evidence_open(int type, char *additional, long additionallen);
int evidence_add(BIO *evidence, char *data, long datalen);
int evidence_close(BIO *evidence);
int evidence_write(int type, char *additional, long additionallen, char *data, long datalen);

#endif /* _EVIDENCEMANAGER_H */
