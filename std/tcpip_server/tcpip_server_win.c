/**
 * @file tcpip_server_win.c
 * @brief The implementation file of TCP/IP windows server
 */

#define TCPIP_SERVER_C
#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

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
    struct              addrinfo *result;
    char                buf[16];
    WSADATA wsaData;
    int iResult;
    SOCKET ListenSocket = INVALID_SOCKET;
    
    /* Initialize Winsock */
    iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0) {
        ERROR("WSAStartup", "MAKEWORD(2,2)", ERROR_WSA);
        return NULL;
    }
    
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;
    
    /* Resolve the server address and port */
    sprintf(buf, "%d", port);
    iResult = getaddrinfo(NULL, buf, &hints, &result);
    if ( iResult != 0 ) {
        ERROR("getaddrinfo", buf, ERROR_WSA);
        WSACleanup();
        return NULL;
    }

    /* Create a SOCKET for connecting to server */
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        ERROR("socket", "", ERROR_WSA);
        freeaddrinfo(result);
        WSACleanup();
        return NULL;
    }
    
    /* Setup the listening socket */
    int enable = 1;
    if (setsockopt(ListenSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        ERROR("setsockopt", "SO_REUSEADDR", ERROR_WSA);
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        return NULL;
    }
    if (setsockopt(ListenSocket, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(int)) < 0) {
        ERROR("setsockopt", "SO_KEEPALIVE", ERROR_WSA);
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        return NULL;
    }
    
    /* Bind the socket */
    iResult = bind( ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        ERROR("bind", buf, ERROR_WSA);
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        return NULL;
    }
    freeaddrinfo(result);
    
    ret_server = (tcpip_server_t*) malloc(sizeof(tcpip_server_t));
    if (!ret_server) {
        ERROR("malloc", "", ERROR_OS);
        return NULL;
    }
    
    ret_server->state = UNINITIALIZED;
    ret_server->port = port;
    ret_server->socket = ListenSocket;
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
    s = closesocket(server->socket);
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
            closesocket(sock);
            free(client_socket);
            free(client_addr);
            pthread_exit(NULL);
        }
        memcpy(client_addr, &client, sizeof(struct sockaddr_in));
        *client_socket = sock;
        /* call the server's callback */
        ((void(*)())server->callback_on_accept)(client_socket, client_addr);
    }
}

#undef TCPIP_SERVER_C
