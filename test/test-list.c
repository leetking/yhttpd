#include <stdio.h>
#include <stdlib.h>

#include "../src/list.h"

typedef struct data_t {
    int x;
    list_t list;
} data_t;

int main()
{
    int i, CNT = 100;
    LIST_NEW(list);

    for (i = 0; i < CNT; i++) {
        data_t *data = malloc(sizeof(*data));
        data->x = i;
        list_add_tail(&list, &data->list);
    }

    list_t *pos;
    list_foreach(&list, pos) {
        data_t *d = list_entry(pos, data_t, list);
        printf("%d\n", d->x);
    }

    list_t *next;
    list_foreach_safe(&list, pos, next) {
        list_del(pos);
        data_t *d = list_entry(pos, data_t, list);
        printf("del: %d\n", d->x);
        free(d);
    }

    printf("empty: %d\n", list_empty(&list));

    return 0;
}
