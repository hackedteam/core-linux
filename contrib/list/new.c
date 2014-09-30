#include <stdlib.h>

typedef struct list {
   void *data;
   struct list *next;
} list_t;

typedef void (*listfreecallback_t)(void *);

list_t *list_add(list_t *list, void *data);
void list_free(list_t *list, listfreecallback_t callback);

int main(void)
{
   return 0;
}

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

void list_free(list_t *list, listfreecallback_t callback)
{
   list_t *next;

   while(list) {
      next = list->next;
      callback(list->data);
      free(list);
      list = next;
   }

   return;
}
