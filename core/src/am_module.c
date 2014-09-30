#include "actionmanager.h"
#include "module.h"
#include "me.h"

void am_module(subaction_module_t *s)
{
   debugme("[RUN] action module\n");

   if(s->status == 1) {
      startmodule(s->module);
   } else {
      stopmodule(s->module);
   }

   return;
}
