#ifndef YHTTP_H__
#define YHTTP_H__

#define EXPORT  extern

#include <stdint.h>

struct yhttp_server {
};


EXPORT struct yhttp_server *yhttp_create();

/**
 * a embeding interface
 * ip:   ip of binding
 * port: port of listening
 * @return: 0: run over successfully
 *          -1: error
 */
EXPORT int yhttp_run(struct yhttp_server *server);
EXPORT void yhttp_stop(struct yhttp_server *server);
EXPORT void yhttp_free(struct yhttp_server **server);

#endif
