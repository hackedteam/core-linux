#include <stdlib.h>

#include "list.h"

list_t *list_add(list_t *list, void *data)
{
   list_t *new, *last;

   if(!(new = calloc(1, sizeof(list_t)))) return list;
   new->data = data;
   new->next = NULL;

   if(!list) return new;

   for(last = list; last->next; last = last->next);
   last->next = new;

   return list;
}

unsigned int list_len(list_t *list)
{
   unsigned int len = 0;

   for(; list; list = list->next) len++;

   return len;
}

void list_free(list_t *list, listfreecallback_t callback)
{
   list_t *next;

   while(list) {
      next = list->next;
      if(callback) callback(list->data);
      free(list);
      list = next;
   }

   return;
}
