#ifndef LIST_H__
#define LIST_H__

#include "common.h"

/* thanks linux kernel */

typedef struct list_t {
    struct list_t *prev, *next;
} list_t;

#define LIST_NEW(list) \
    list_t list = {&list, &list}

#define LIST_INIT(list) do { \
    (list)->prev = list; \
    (list)->next = list; \
} while (0)

#define __list_add(new, _p, _n) do { \
    list_t *p = _p, *n = _n; \
    (p)->next = new; \
    (new)->next = n; \
    (new)->prev = p; \
    (n)->prev = new; \
} while (0)

#define __list_del(p, n) do { \
    (n)->prev = p; \
    (p)->next = n; \
} while (0)

#define list_empty(list) \
    ((list)->next == list)

#define list_add(list, new) \
    __list_add(new, list, (list)->next)

#define list_add_tail(list, new) \
    __list_add(new, (list)->prev, list)

#define list_del(entry) \
    __list_del((entry)->prev, (entry)->next)

#define list_head(list) \
    (list)->next
#define list_tail(list) \
    (list)->prev

#define list_entry(ptr, type, member) \
    container_of(ptr, type, member)

#define list_foreach(list, pos) \
    for (pos = (list)->next; pos != (list); pos = pos->next)

#define list_foreach_safe(list, pos, n) \
    for (pos = (list)->next, n = pos->next; pos != (list); \
            pos = n, n = pos->next)

#endif
