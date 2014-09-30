#ifndef _LIST_H
#define _LIST_H

typedef struct list {
   void *data;
   struct list *next;
} list_t;

typedef void (*listfreecallback_t)(void *);

list_t *list_add(list_t *list, void *data);
unsigned int list_len(list_t *list);
void list_free(list_t *list, listfreecallback_t callback);

#endif /* _LIST_H */
