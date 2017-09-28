#ifndef SET_H__
#define SET_H__

/* 简单的集合类型 */
typedef struct set set_t;

/* public function */
extern set_t *set_create(int cap);
extern void set_destory(set_t *s);
extern int set_isempty(set_t const *s);
extern int set_isfull(set_t const *s);
extern int set_add(set_t *s, int ele);
extern int set_remove(set_t *s, int ele);

/* a sample interater */
#define set_foreach(s, ele) \
        for (set_start(s); set_isend(s); set_iterate(s, &ele))

/* private function, though we can invoke them, but may be rmeoved. */
int set_start(set_t *s);
int set_iterate(set_t *s, int *ele);
int set_isend(set_t *s);

#endif /* SET_H__ */
