#include "actionmanager.h"
#include "info.h"
#include "me.h"

void am_log(subaction_log_t *s)
{
   debugme("[RUN] action log\n");

   info(s->text);

   return;
}
