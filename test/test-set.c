#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "../set.h"

static void test_set_int(void)
{
    printf("test_set_int begin\n");
    set_t set = set_create(sizeof(int), NULL, NULL);
    assert(set);
    assert(set_isempty(set));
    assert(0 == set_size(set));
    assert(!set_isfull(set));

    set_add(set, (void*)1);
    assert(!set_isempty(set));
    assert(1 == set_size(set));

    set_add(set, (void*)2);
    assert(!set_isempty(set));
    assert(2 == set_size(set));

    int ele;
    set_foreach(set, ele) {
        printf("%d ", ele);
    }
    printf("\n");

    set_remove(set, (void*)2);
    assert(!set_isempty(set));
    assert(1 == set_size(set));

    set_remove(set, (void*)1);
    assert(set_isempty(set));
    assert(0 == set_size(set));

    for (int i = 0; i < 10; i++)
        set_add(set, (void*)i);
    set_foreach(set, ele) {
        printf("%d ", ele);
    }
    printf("\n");

    for (int i = 0; i < 5; i++)
        set_remove(set, (void*)i);
    set_foreach(set, ele) {
        printf("%d ", ele);
    }
    printf("\n");

    for (int i = 0; i < 4; i++)
        set_add(set, (void*)i);
    set_foreach(set, ele) {
        printf("%d ", ele);
    }
    printf("\n");

    set_foreach(set, ele) {
        set_remove(set, (void*)ele);
    }

    /* it should be no print */
    printf("it should be no print\n");
    set_foreach(set, ele) {
        printf("%d\n", ele);
    }

    set_destory(&set);
    assert(NULL == set);

    printf("test_set_int end\n\n");
}

struct stu {
    char *name;
    int age;
};
void stu_free(struct stu **pobjp)
{
    assert(pobjp);
    assert(*pobjp);
    free((*pobjp)->name);
}
struct stu *stu_new(char const *name, int age)
{
    struct stu *s = malloc(sizeof(*s));
    assert(s);
    s->name = strdup(name);
    s->age = age;

    return s;
}
char *stu_str(struct stu const *s)
{
    static char buff[512];
    snprintf(buff, 512, "%s: %d", s->name, s->age);
    return buff;
}

static void test_set_obj(void)
{
    set_t set = set_create(sizeof(struct stu*), NULL, (free_t*)stu_free);
    assert(set);
    assert(0 == set_size(set));
    assert(set_isempty(set));

    for (int i = 0; i < 50000; i++) {
        char buff[46] = "";
        snprintf(buff, 46, "%d-name", i);
        struct stu *s = stu_new(buff, i);
        set_add(set, s);
    }
    struct stu *s;
    set_foreach(set, s) {
        printf("%s\n", stu_str(s));
    }

    set_destory(&set);
}

int main()
{
    test_set_int();
    test_set_obj();
    return 0;
}
