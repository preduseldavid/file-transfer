/**
 * @file tcpip_server_linux.c
 * @brief The implementation file of TCP/IP unix server
 */

#define TCPIP_SERVER_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "tcpip_server.h"
#include "error.h"

/* internal functions prototypes */
static int32_t server_set_listen(tcpip_server_t* server, void* callback);
static int32_t server_set_reject(tcpip_server_t* server);
static void    thread_accept_connections(tcpip_server_t* server);

tcpip_server_t* tcpip_server_create(uint16_t port)
{
    tcpip_server_t      *ret_server = NULL;
    struct              addrinfo hints;
    struct              addrinfo *result, *rp;
    int32_t             socketfd, s;
    char                buf[16];
    
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;          /* Internet domain socket */
    hints.ai_socktype = SOCK_STREAM;    /* Stream socket */
    hints.ai_flags = AI_PASSIVE;        /* For wildcard IP address */
    hints.ai_protocol = 0;              /* Any protocol */
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;
    
    errno = 0;
    sprintf(buf, "%d", port);
    if (errno) {
        ERROR("sprintf", "", ERROR_OS);
        return NULL;
    }
    s = getaddrinfo(NULL, buf, &hints, &result);
    if (s != 0) {
        errno = s;
        ERROR("getaddrinfo", "", ERROR_OS);
        return NULL;
    }
    /* getaddrinfo() returns a list of address structures.
     *                          Try each address until we successfully bind(2).
     *                          If socket(2) (or bind(2)) fails, we (close the socket
     *                          and) try the next address. */
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        socketfd = socket(rp->ai_family, rp->ai_socktype,
                          rp->ai_protocol);
        if (socketfd == -1)
            continue;
        int enable = 1;
        if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
            ERROR("setsockopt", "SO_REUSEADDR", ERROR_OS);
            close(socketfd);
            continue;
        }
        if (setsockopt(socketfd, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(int)) < 0) {
             ERROR("setsockopt", "SO_KEEPALIVE", ERROR_OS);
            close(socketfd);
            continue;
        }
        if (bind(socketfd, rp->ai_addr, rp->ai_addrlen) == 0)
            break;                  /* Success */
        close(socketfd);
    }
    freeaddrinfo(result);           /* No longer needed */
    if (rp == NULL) {               /* No address succeeded */
        ERROR("bind", "", ERROR_OS);
        return NULL;
    }
    
    ret_server = (tcpip_server_t*) malloc(sizeof(tcpip_server_t));
    if (!ret_server) {
        ERROR("malloc", "", ERROR_OS);
        return NULL;
    }
    
    ret_server->state = UNINITIALIZED;
    ret_server->port = port;
    ret_server->socket = socketfd;
    ret_server->callback_on_accept = NULL;
    ret_server->listen_TID = -1;
    
    return ret_server;
}

int32_t tcpip_server_setstate(tcpip_server_t* server, void* callback, uint8_t flag)
{
    int32_t s = 0;
    
    switch(flag) {
        case LISTEN:
            if (server->state != LISTEN && callback != NULL)
                s = server_set_listen(server, callback);
            else if (server->state == LISTEN)
                s = IS_LISTENING;
            else s = NO_CALLBACK;
            break;
        case REJECT:
            if (server->state != REJECT)
                s = server_set_reject(server);
            else s = IS_REJECTING;
            break;
        default:
            s = UNDEFINED_FLAG;
            break;
    }
    return s;
}

int32_t tcpip_server_destroy(tcpip_server_t* server)
{
    int32_t s;
    
    s = server_set_reject(server);
    if (s == -1) {
        return -1;
    }
    s = close(server->socket);
    if (s != 0) {
        return -1;
    }
    free(server);
    return 0;
}

static int32_t server_set_listen(tcpip_server_t* server, void* callback)
{
    uint32_t s;
    
    if (server->state != REJECT) {
        /* listen for clients on the socket */
        s = listen(server->socket, SOMAXCONN);
        if (s == -1) {
            ERROR("listen", "", ERROR_OS);
            return -1;
        }
    }
    server->callback_on_accept = callback;
    /* create thread */
    s = pthread_create(&(server->listen_TID), NULL, (void*)&thread_accept_connections, server);
    if (s != 0) {
        errno = s;
        ERROR("pthread_create", "", ERROR_OS);
        return -1;
    }
    /* Success */
    server->state = LISTEN;
    return 0;
}

static int32_t server_set_reject(tcpip_server_t* server)
{
    int32_t s = 0;
    void* res;
    
    if (server->state == LISTEN) {
        s = pthread_cancel(server->listen_TID);
        if (s != 0) {
            errno = s;
            ERROR("pthread_cancel", "", ERROR_OS);
            return -1;
        }
        s = pthread_join(server->listen_TID, &res);
        if (res != PTHREAD_CANCELED) {
            errno = s;
            ERROR("pthread_join", "", ERROR_OS);
            return -1;
        }
        server->state = REJECT;
    } else s = NOT_LISTENING;
    return s;
}

static void thread_accept_connections(tcpip_server_t* server)
{
    int32_t s, c;
    struct sockaddr_in client;
    c = sizeof(struct sockaddr_in);
    
    /* Enable a cancelation request */
    s = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    if (s != 0) {
        errno = s;
        ERROR("pthread_setcancelstate", "", ERROR_OS);
        pthread_exit(NULL);
    }
    while (1) {
        int32_t sock;
        /* accept(3) is a cancelation point */
        sock = accept(server->socket, (struct sockaddr*)&client, (socklen_t*)&c);
        if (sock < 0) {
            /* errno is setted by accept(3) */
            ERROR("accept", "", ERROR_OS);
            pthread_exit(NULL);
        }
        int32_t* client_socket = (int32_t*) malloc(sizeof(int32_t));
        struct sockaddr_in *client_addr = (struct sockaddr_in *) malloc(sizeof(struct sockaddr_in));
        if (!client_socket || !client_addr) {
            /* errno is setted by malloc */
            ERROR("malloc", "", ERROR_OS);
            close(sock);
            free(client_socket);
            free(client_addr);
            pthread_exit(NULL);
        }
        memcpy(client_addr, &client, sizeof(struct sockaddr_in));
        *client_socket = sock;
        /* call the callback_on_accept function */
        ((void(*)())server->callback_on_accept)(client_socket, client_addr);
    }
}

#undef TCPIP_SERVER_C
