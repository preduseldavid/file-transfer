/**
 * @file main.c
 * @brief The main file(and function) of project
 */

#define _BSD_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#ifdef UNIX
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif /* UNIX */

#include "data_types.h"
#include "tcpip_server.h"
#include "send.h"
#include "receive.h"

/* internal variables */
static volatile int recv_lock;

/* internal functions' prototypes */
static void callback_on_accept(SOCKET* sock, struct sockaddr_in *client_addr);
static void client_test(char *path);

static void callback_on_accept(SOCKET* sock, struct sockaddr_in *client_addr)
{
    SOCKET              sock_desc = *sock;
    struct sockaddr_in  client_info = *client_addr;
    char                *client_ip = inet_ntoa(client_info.sin_addr);
    //int32_t             ret;
    net_packet_t        *packet = NULL;
    
    /* release the arguments */
    free(sock);
    free(client_addr);
    
    /* lock atomically the recv lock */
    while (__sync_lock_test_and_set(&recv_lock, 1));
    
    fprintf(stderr, "New connection from %s\n", client_ip);
    
    packet = recv_packet(sock_desc, 0);
    if (packet == NULL)
        goto end;
    
    if (packet->flags.val & START_TRANSFER) {
        if (packet->flags.val & SEND_OPERATION) {
            __recv(sock_desc, packet->flags.val, "/home/theprdv/workspace/received");
        }
        else if (packet->flags.val & RECEIVE_OPERATION) {
            __send(sock_desc, packet->data);
        }
    }
    
    destroy_packet(packet);
    
    /* release the recv lock */
    __sync_lock_release(&recv_lock);
    goto end;
    
 end:
    close(sock_desc);
}

static void client_test(char *path)
{
    int         sock_desc;
    struct      sockaddr_in remote_addr;
    int32_t     s;
    
    /* Zeroing remote_addr struct */
    memset(&remote_addr, 0, sizeof(remote_addr));
    
    /* Construct remote_addr struct */
    remote_addr.sin_family = AF_INET;
    inet_pton(AF_INET, "localhost", &(remote_addr.sin_addr));
    remote_addr.sin_port = htons(8899);
    
    /* Create client socket */
    sock_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_desc == -1) {
        fprintf(stderr, "Error creating socket --> %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    /* Connect to the server */
    if (connect(sock_desc, (struct sockaddr *)&remote_addr, sizeof(struct sockaddr)) == -1) {
        fprintf(stderr, "Error on connect --> %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    /*
    s = __send(sock_desc, path);
    */
    char *recv_dir = "/home/theprdv/workspace/received/received2";
    send_packet(sock_desc, path, strlen(path), START_TRANSFER|RECEIVE_OPERATION);
    
    net_packet_t *packet = recv_packet(sock_desc, 0);
    if (packet->flags.val & START_TRANSFER) {
        s = __recv(sock_desc, packet->flags.val, recv_dir);
    }
    else if (packet->flags.val & ABORT_TRANSFER) {
        s = -1;
        fprintf(stderr, "Aborted transfer...\n");
    }
    destroy_packet(packet);
    
    fprintf(stderr, "%d\n", s);
    
    close(sock_desc);
}


int main(int argc, char **argv) {
    if (argc == 1) {
        int ret;
        tcpip_server_t *server = tcpip_server_create(8899);
        if (!server) {
            perror("create_server");
            return 1;
        }
        
        ret = tcpip_server_setstate(server, &callback_on_accept, LISTEN);
        if (ret != 0) {
            perror("set online 1");
        }
        
        fprintf(stderr, "online\n");
        sleep(234234);
        
        ret = tcpip_server_setstate(server, &callback_on_accept, REJECT);
        if (ret != 0) {
            perror("set sleep");
        }
        
        fprintf(stderr, "sleep\n");
        sleep(10);
        
        ret = tcpip_server_destroy(server);
        if (ret != 0) {
            perror("destroy()");
        }
    }
    else {
        client_test(argv[1]);
    }
    exit(EXIT_SUCCESS);
}
