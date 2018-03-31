#ifndef FASTCGI_H__
#define FASTCGI_H__

#include "setting.h"
#include <stdint.h>

#define FCGI_LISTENSOCK_FILENO   0

typedef struct {
#define FCGI_VERSION_1 1
    uint8_t version;
#define FCGI_BEGIN_REQUEST       1
#define FCGI_ABORT_REQUEST       2
#define FCGI_END_REQUEST         3
#define FCGI_PARAMS              4
#define FCGI_STDIN               5
#define FCGI_STDOUT              6
#define FCGI_STDERR              7
#define FCGI_DATA                8
#define FCGI_GET_VALUES          9
#define FCGI_GET_VALUES_RESULT  10
#define FCGI_UNKNOWN_TYPE       11
#define FCGI_MAXTYPE            (FCGI_UNKNOWN_TYPE)
    uint8_t type;
#define FCGI_NULL_REQUEST_ID     0
    uint8_t requsetIdB1;
    uint8_t requsetIdB0;
    uint8_t contentLengthB1;
    uint8_t contentLengthB0;
    uint8_t paddingLength;
    uint8_t reversed;
} FCGI_Header;

#define FCGI_HEADER_LEN 8   /* sizeof(fcgihdr_t) */
typedef struct {
#define FCGI_RESPONDER  1
#define FCGI_AUTHORIZER 2
#define FCGI_FILTER     3
    uint8_t roleB1;
    uint8_t roleB0;
#define FCGI_KEEP_CONN  1
    uint8_t flags;
    uint8_t reserved[5];
} FCGI_BeginRequestBody;

typedef struct {
    FCGI_Header header;
    FCGI_BeginRequestBody body;
} FCGI_BeginRequestRecord;

typedef struct {
    uint8_t appStatusB3;
    uint8_t appStatusB2;
    uint8_t appStatusB1;
    uint8_t appStatusB0;
#define FCGI_REQUEST_COMPLETE 0
#define FCGI_CANT_MPX_CONN    1
#define FCGI_OVERLOADED       2
#define FCGI_UNKNOWN_ROLE     3
    uint8_t protocolStatus;
    uint8_t reserved[3];
} FCGI_EndRequestBody;

typedef struct {
    FCGI_Header header;
    FCGI_EndRequestBody body;
} FCGI_EndRequestRecord;

/*
 * Variable names for FCGI_GET_VALUES / FCGI_GET_VALUES_RESULT records
 */
#define FCGI_MAX_CONNS  "FCGI_MAX_CONNS"
#define FCGI_MAX_REQS   "FCGI_MAX_REQS"
#define FCGI_MPXS_CONNS "FCGI_MPXS_CONNS"

typedef struct {
    uint8_t type;
    uint8_t reserved[7];
} FCGI_UnknownTypeBody;

typedef struct {
    FCGI_Header header;
    FCGI_UnknownTypeBody body;
} FCGI_UnknownTypeRecord;

#include "buffer.h"
#include "connection.h"

typedef struct {
    buffer_t *req_buffer;
    buffer_t *res_buffer;

    connection_t *requst_con;
} http_fastcgi_t;

extern connection_t *fastcgi_connection_get(struct setting_fastcgi *fcgi);
extern http_fastcgi_t *http_fastcgi_init();
extern int http_fastcgi_parse_response();
extern void http_fastcgi_build_th();
extern void http_fastcgi_build_bh();

#endif

