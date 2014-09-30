#include "actionmanager.h"
#include "me.h"
#include "exec.h"

void am_execute(subaction_execute_t *s)
{
   debugme("[RUN] action execute\n");

   exec(s->command);

   return;
}
