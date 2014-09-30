#ifndef _ACTIONMANAGER_H
#define _ACTIONMANAGER_H 1

#include <stdio.h>
#include <json.h>

typedef struct subaction {
   int type;
   void *param;
} subaction_t;

#define SUBACTION_NONE -1

#define SUBACTION_UNINSTALL 0
typedef void subaction_uninstall_t;
void am_uninstall(void);

#define SUBACTION_EXECUTE 1
typedef struct {
   char command[512];
} subaction_execute_t;
void am_execute(subaction_execute_t *e);

#define SUBACTION_LOG 2
typedef struct {
   char text[512];
} subaction_log_t;
void am_log(subaction_log_t *e);

#define SUBACTION_SYNCHRONIZE 3
typedef struct {
   char host[128];
   int bandwidth;
   int mindelay;
   int maxdelay;
   int stop;
} subaction_synchronize_t;
int am_synchronize(subaction_synchronize_t *e);

#define SUBACTION_DESTROY 4
typedef struct {
   int permanent;
} subaction_destroy_t;
void am_destroy(subaction_destroy_t *e);

#define SUBACTION_EVENT 5
typedef struct {
   int event;
   int status;
} subaction_event_t;
void am_event(subaction_event_t *e);

#define SUBACTION_MODULE 6
typedef struct {
   int module;
   int status;
} subaction_module_t;
void am_module(subaction_module_t *e);

typedef struct {
   int sched;
   int queue;
   int subactionc;
   subaction_t *subactionlist;
} action_t;

#define QUEUE_SLOW 0
#define QUEUE_FAST 1

#define STATUS_STOPPING 0
#define STATUS_STOPPED  1
#define STATUS_STARTING 2
#define STATUS_STARTED  3

void am_parseactions(json_object *config);
void am_flushactions(void);
void am_queueaction(int actionnum);
void am_startqueue(int q);
void am_stopqueue(int q);


#endif /* _ACTIONMANAGER_H */
