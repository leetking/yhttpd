#ifndef BUFFER_H__
#define BUFFER_H__

#include <stddef.h> /* for size_t */
#include <string.h> /* memcopy */

typedef struct buffer_t {
    char *pos;
    char *last;              /* point to position where data ends */
    char *end;
    char base[1];
} buffer_t;

extern buffer_t *buffer_malloc(size_t size);
extern void buffer_free(buffer_t *b);
extern buffer_t *buffer_copy(buffer_t *to, buffer_t const *from);

#define buffer_cavity(b) ((b)->pos - (b)->base)
#define buffer_len(b)    ((b)->last - (b)->pos)
#define buffer_rest(b)   ((b)->end - (b)->last)
#define buffer_init(b)   ((b)->pos = (b)->last = (b)->base)

#define buffer_fill(to, str, n) do { \
    memcpy((to)->last, str, n); \
    (to)->last += (n); \
} while (0)

#endif
