
#define EVENT_USE_SELECT
/* #define EVENT_USE_POLL */
/* #define EVENT_USE_EPOLL */

#if !((defined(EVENT_USE_SELECT) && !defined(EVENT_USE_POLL) && !defined(EVENT_USE_EPOLL)) \
    ||(!defined(EVENT_USE_SELECT) && defined(EVENT_USE_POLL) && !defined(EVENT_USE_EPOLL)) \
    ||(!defined(EVENT_USE_SELECT) && !defined(EVENT_USE_POLL) && defined(EVENT_USE_EPOLL)) )
# error "Must only define one of EVENT_USE_SELECT, EVENT_USE_POLL and EVENT_USE_EPOLL"
#endif

