#include <stdio.h>
#include "list.h"

int main(void)
{
   list_t *list = NULL, *cur;

   list = list_add(list, "uno");
   list = list_add(list, "due");
   list = list_add(list, "tre");

   for(cur = list; cur; cur = cur->next) printf("%s\n", (char *)cur->data);

   list_free(list, NULL);

   return 0;
}
