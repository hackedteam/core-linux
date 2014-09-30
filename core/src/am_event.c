#include "actionmanager.h"
#include "eventmanager.h"
#include "me.h"

void am_event(subaction_event_t *s)
{
   debugme("[RUN] action event\n");

   if(s->status) {
      em_enableevent(s->event);
   } else {
      em_disableevent(s->event);
   }

   return;
}
