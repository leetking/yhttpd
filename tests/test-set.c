#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "../set.h"

struct stu {
    char *name;
    int age;
};
void stu_free(struct stu **pobjp)
{
    assert(pobjp);
    assert(*pobjp);
    free((*pobjp)->name);
    free(*pobjp);
    *pobjp = NULL;
}
int stu_cmp(struct stu const *a, struct stu const *b)
{
    if (!strcmp(a->name, b->name) && (a->age == b->age))
        return 0;
    return 1;
}
uint32_t str_hash(char const *str)
{
    uint32_t hash = 0;
    for (; *str; str++)
        hash = hash*31+*str;
    return hash;
}
uint32_t stu_hash(struct stu const *obj)
{
    return str_hash(obj->name) ^ obj->age;
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
    const int CNT = 1000000;
    set_t set = set_create((cmp_t*)stu_cmp, (free_t*)stu_free, (hashfn_t*)stu_hash);
    assert(set);
    assert(0 == set_size(set));
    assert(set_isempty(set));

    for (int i = 0; i < CNT; i++) {
        char buff[46] = "";
        snprintf(buff, 46, "%d-name", i);
        struct stu *s = stu_new(buff, i);
        set_add(set, s);
        if (i+1 != set_size(set)) {
            printf("%d+1 != %d\n", i, set_size(set));
            assert(i+1 == set_size(set));
        }
    }
    struct stu *s;
    int cnt = 0;
    set_foreach(set, s) {
        printf("%s\n", stu_str(s));
        cnt++;
    }
    if (cnt != CNT) {
        printf("cnt: %d\n", cnt);
        assert(cnt == CNT);
    }

    struct stu hint;
    hint.name = "0-name";
    hint.age  = 0;
    set_remove(set, &hint);
    assert(CNT-1 == set_size(set));

    set_destory(&set);
}

static void test_set_order(void)
{
    set_t set = set_create((cmp_t*)stu_cmp, (free_t*)stu_free, (hashfn_t*)stu_hash);
    char buff[46];

    set_add(set, stu_new("0-name", 0));
    assert(1 == set_size(set));

    int cnt = 0;
    struct stu *stu;
    set_foreach(set, stu) {
        snprintf(buff, 46, "%d-name", cnt+10);
        struct stu *obj = stu_new(buff, cnt+10);
        set_add(set, obj);
        cnt++;
    }
    assert(1 == cnt);
    assert(2 == set_size(set));

    cnt = 0;
    set_foreach(set, stu) {
        cnt++;
    }
    printf("cnt: %d == set_size: %d == 2\n", cnt, set_size(set));
    assert(cnt == set_size(set));

    cnt = 0;
    set_foreach(set, stu) {
        set_remove(set, stu);
        cnt++;
    }
    printf("cnt: %d == 2\n", cnt);
    assert(cnt == 2);
    assert(0 == set_size(set));

    const int CNT = 50;
    for (int i = 0; i < CNT; i++) {
        snprintf(buff, 46, "%d-name", i);
        struct stu *obj = stu_new(buff, i);
        set_add(set, obj);
        if (i+1 != set_size(set)) {
            printf("%d+1 != %d\n", i, set_size(set));
            assert(i+1 == set_size(set));
        }
    }

    cnt = 0;
    set_foreach(set, stu) {
        cnt++;
    }
    printf("cnt: %d == set_size: %d\n", cnt, set_size(set));
    assert(cnt == set_size(set));

    set_destory(&set);
}

int main()
{
    test_set_obj();
    test_set_order();
    return 0;
}
