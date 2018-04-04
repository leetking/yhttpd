#ifndef COMMON_H__
#define COMMON_H__

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>

#define BUFF_SIZE           (2048)
#define CPU_ID_MAX          (2)
#define SUFFIX_LEN          5

#define MAX(x, y)       ((x)>(y)? (x): (y))
#define MIN(x, y)       ((x)<(y)? (x): (y))
#define ARRSIZE(arr)    (sizeof(arr)/sizeof(arr[0]))
#define SSTR_LEN(str)   (sizeof(str)-1)

#define YHTTP_OK     (0)
#define YHTTP_AGAIN  (-1)
#define YHTTP_BLOCK  (-2)
#define YHTTP_FAILE  (-3)
#define YHTTP_ERROR  (-4)

typedef intptr_t msec_t;

typedef struct string_t {
    char const *str;
    int len;
} string_t;
#define string_equal(s1, s2)    ((s1)->len == (s2)->len \
        && !strncmp((s1)->str, (s2)->str, (s1)->len))
#define string_newstr(str)      {(str), SSTR_LEN(str)}
#define string_null             {NULL, 0}
#define string_isnull(s)        ((s)->str == NULL && (s)->len == 0)
extern void string_tolower(char *str, size_t len);

typedef struct str_pairt_t {
    string_t key, value;
} str_pairt_t;

/* read stable, ignore interrupt */
ssize_t write_s(int fd, uint8_t const *buff, ssize_t n);
ssize_t read_s(int fd, uint8_t *buff, ssize_t n);

extern void *memfind(void const *mem, ssize_t memlen, void const *pat, ssize_t patlen);
extern void set_cpu_affinity(int cpuid);
extern void set_nonblock(int fd);

#ifndef offsetof
# define offsetof(type, member) \
    ((int)&((type*)0)->member)
#endif
#ifndef container_of
# define container_of(ptr, type, member) \
    ((type*)((char*)(ptr)-offsetof(type, member)))
#endif

#ifdef YHTTP_DEBUG
void bug_on(void);
# define BUG_ON(exp)    do { \
    if (exp) { \
        printf("BUG ON %s:%d(%s) %s\n", __FILE__, __LINE__, __FUNCTION__, #exp); \
        bug_on(); \
    } \
} while (0)
#else
# define BUG_ON(exp)    ((void)0)
#endif /* YHTTP_DEBUG */

#ifdef __GNUC__
# define likely(exp)       __builtin_expect(!!(exp), 1)
# define unlikely(exp)     __builtin_expect(!!(exp), 0)
#else
# define likely(exp)     (exp)
# define unlikely(exp)   (exp)
#endif /* __GNUC__ */

#endif /* COMMON_H__ */
