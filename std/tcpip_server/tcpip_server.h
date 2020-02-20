/**
 * @file tcpip_server.h
 * @brief The TCP/IP server header
 */

#ifndef TCPIP_SERVER_H
#define TCPIP_SERVER_H

#include <stdlib.h>
#include <inttypes.h>
#include <errno.h>
#include <pthread.h>

#ifdef TCPIP_SERVER_C
#define EXTERN
#else
#define EXTERN extern
#endif /* TCPIP_SERVER_C */

/* tcp/ip structs and flags */
typedef enum {
    LISTEN              = 0x01,         /* The server is avabile and accept an incoming connection */
    UNINITIALIZED       = 0x02,         /* The server is not initialized to listen on any port */
    REJECT              = 0x03          /* The server doesn't accept any connection, work only with the connected clients */
} tcpip_server_state_t;

typedef enum {
    IS_LISTENING        = -3,
    NOT_LISTENING       = -4,
    IS_REJECTING        = -5,
    UNDEFINED_FLAG      = -6,
    NO_CALLBACK         = -7
} tcpip_server_error_t;

typedef struct {
    #ifdef UNIX
    int32_t             socket;
    #else
    SOCKET              socket;
    #endif /* UNIX */
    void                *callback_on_accept;
    int8_t              state;
    uint8_t             port;
    pthread_t           listen_TID;
} tcpip_server_t;

/* functions */
EXTERN tcpip_server_t* tcpip_server_create(uint16_t port);
EXTERN int32_t tcpip_server_setstate(tcpip_server_t* server, void* callback, uint8_t flag);
EXTERN int32_t tcpip_server_destroy(tcpip_server_t* server);


#undef EXTERN
#endif /* TCPIP_SERVER_H */
