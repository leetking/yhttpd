#ifndef CONNECTION_H__
#define CONNECTION_H__

/* 内部维护一个状态机 */

struct connection {
    int fd;
};

struct connection *connection_create(int fd);
int connection_write(struct connection *c);
int connection_read(struct connection *c);
int connection_isvalid(struct connection const *c);
void connection_destory(struct connection **c);

#endif /* CONNECTION_H__ */
