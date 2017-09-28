#ifndef HTTP_H__
#define HTTP_H__

#define METHOD_LEN  5

struct http {
    char method[METHOD_LEN];
};

struct http *http_build();

#endif /* HTTP_H__ */
