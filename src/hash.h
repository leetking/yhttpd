#ifndef HASH_H__
#define HASH_H__

#include <stddef.h>
#include <stdint.h>

typedef struct hash_t hash_t;

#define HASH_KEY_ERROR      ((uint32_t)-1)

typedef void hash_free_t(void *x);

extern hash_t *hash_create(int cap, hash_free_t *free);
extern void hash_destroy(hash_t **t);

extern uint32_t hash_add(hash_t *, char const *key, size_t len, void *x);
extern void *hash_getk(hash_t *t, char const *key, size_t len);
extern void *hash_getv(hash_t *t, uint32_t vlu);
extern void *hash_del(hash_t *t, char const *key, size_t len);

#endif
