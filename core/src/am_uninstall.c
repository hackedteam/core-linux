#include "actionmanager.h"
#include "me.h"
#include "uninstall.h"

void am_uninstall(void)
{
   debugme("[RUN] action uninstall\n");

   uninstall();

   return;
}
