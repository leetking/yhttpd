#include <stdio.h>
#include <assert.h>

#include "../src/hash.h"

int main()
{
    hash_t t = hash_create(1000, NULL, NULL);

    uint32_t k0 = hash_add(t, "1234", 4, (void*)0);
    uint32_t k1 = hash_add(t, "12345", 5, (void*)1);

    assert((void*)0 == hash_getk(t, "1234", 4));
    assert((void*)1 == hash_getk(t, "12345", 5));

    assert((void*)0 == hash_getv(t, k0));
    assert((void*)1 == hash_getv(t, k1));

    for (intptr_t i = 0; i < 5000; i++) {
        uint32_t k = hash_add(t, (char*)&i, sizeof(i), (void*)i);
        assert((void*)i == hash_getk(t, (char*)&i, sizeof(i)));
        assert((void*)i == hash_getv(t, k));
    }
    for (intptr_t i = 0; i < 5000; i++) {
        assert((void*)i == hash_delk(t, (char*)&i, sizeof(i)));
    }

    hash_destroy(&t);
    return 0;
}
