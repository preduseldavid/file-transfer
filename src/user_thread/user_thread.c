#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef UNIX
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif /* UNIX */

#include "config.h"
#include "data_types.h"
#include "send.h"
#include "receive.h"
#include "error.h"

#define USER_THREAD_C
#include "user_thread.h"

/* INTERNAL FUNCTIONS */
static int8_t   connect_to_peer(char *ip);
static int8_t   send_to_peer(char *path, char *ip, TC_t *TC);
static int8_t   receive_from_peer(char *path, char *ip, TC_t *TC);
static char     **split_string(char *string, int32_t *cnt, char delim);

void user_thread(void *arg_ptr)
{
    char buf[PATH_SIZE];
    user_thread_arg_t arg_data = *(user_thread_arg_t *) arg_ptr;
    
    /* free the arguments */
    free(arg_ptr);
    
    while (1) {
        while (__sync_lock_test_and_set(&arg_data.TC->lock, 1)) {
            usleep(100);
        }
        fprintf(stdout, "\n-> ");
        fflush(stdout);
        __sync_lock_release(&arg_data.TC->lock);
        if (!fgets(buf, sizeof(buf), stdin)) {
            break;
        }
        
        int32_t cnt;
        buf[strlen(buf) - 1] = '\0';
        char **tokens = split_string(buf, &cnt, ' ');
        char *cmd = tokens[0];
        int locked = __sync_fetch_and_and(&arg_data.TC->lock, 0);
        
        if (!locked && cnt == 3 && !memcmp(cmd, "receive", strlen(cmd))) {
            receive_from_peer(tokens[1], tokens[2], arg_data.TC);
        }
        else if (!locked && cnt == 3 && !memcmp(cmd, "send", strlen(cmd))) {
            send_to_peer(tokens[1], tokens[2], arg_data.TC);
        }
        else if (cnt == 1 && !memcmp(cmd, "stop", strlen(cmd))) {
            break;
        }
        /* asking for permissions */
        else if (!memcmp(cmd, "Y", strlen(cmd)) || !memcmp(cmd, "y", strlen(cmd))) {
            arg_data.TC->action = ALLOW_ACTION;
            __sync_lock_release(&arg_data.TC->lock);
            usleep(500000);
        }
        else if (!memcmp(cmd, "N", strlen(cmd)) || !memcmp(cmd, "n", strlen(cmd))) {
            arg_data.TC->action = DENY_ACTION;
            __sync_lock_release(&arg_data.TC->lock);
            usleep(500000);
        }
        else if (locked) {
            arg_data.TC->action = WAITING_ACTION;
            fprintf(stdout, "Write an answer [Y/N]\n");
            fflush(stdout);
        }
        else {
            fprintf(stdout, "Incorect command format...\n");
            fflush(stdout);
        }
        
        free(tokens);
    }
}

static int8_t connect_to_peer(char *ip)
{
    int                 sock_desc;
    struct sockaddr_in  remote_addr;
    char                buf[128];
    
    /* Zeroing remote_addr struct */
    memset(&remote_addr, 0, sizeof(remote_addr));
    
    /* Construct remote_addr struct */
    remote_addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &(remote_addr.sin_addr));
    remote_addr.sin_port = htons(PORT);
    
    /* Create client socket */
    sock_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_desc == -1) {
        ERROR("socket", "client", ERROR_OS);
        return -1;
    }
    
    /* Connect to the other peer */
    if (connect(sock_desc, (struct sockaddr *)&remote_addr, sizeof(struct sockaddr)) == -1) {
        snprintf(buf, sizeof(buf), "to peer %s", ip);
        ERROR("connect", buf, ERROR_OS);
        return -1;
    }
    
    return sock_desc;
}

static int8_t send_to_peer(char *path, char *ip, TC_t *TC)
{
    int32_t s;
    int32_t peer_sock = connect_to_peer(ip);
    
    if (peer_sock == -1) {
        return -1;
    }
    while (__sync_lock_test_and_set(&TC->lock, 1)) {
        usleep(100);
    }
    s = __send(peer_sock, path);
    close(peer_sock);
    __sync_lock_release(&TC->lock);
    
    return s;
}

static int8_t receive_from_peer(char *path, char *ip, TC_t *TC)
{
    int32_t s;
    int32_t peer_sock = connect_to_peer(ip);
    
    if (peer_sock == -1) {
        return -1;
    }
    
    while (__sync_lock_test_and_set(&TC->lock, 1)) {
        usleep(100);
    }
    send_packet(peer_sock, path, strlen(path), START_TRANSFER|RECEIVE_OPERATION);
    net_packet_t *packet = recv_packet(peer_sock, 0);
    
    if (packet) {
        if (packet->flags.val & START_TRANSFER) {
            s = __recv(peer_sock, packet->flags.val, RECEIVING_PATH);
        }
        destroy_packet(packet);
    }
    else {
        s = -1;
    }
    close(peer_sock);
    __sync_lock_release(&TC->lock);
    
    return s;
}

static char **split_string(char *string, int32_t *cnt, char delim)
{
    int32_t tokens_cnt = 0;
    int32_t str_size = strlen(string);
    
    if (!str_size) {
        *cnt = 0;
        return NULL;
    }
    for (uint32_t i = 0; i < str_size; ++i)
        if (string[i] == delim)
            ++tokens_cnt;
    tokens_cnt += 2;
    
    char **tokens = (char **) malloc(sizeof(char *) * tokens_cnt);
    tokens[0] = string;
    for (uint32_t i = 0, j = 1; i < str_size; ++i)
        if (string[i] == delim) {
            string[i] = '\0';
            tokens[j++] = &string[i + 1];
        }
    
    tokens[tokens_cnt - 1] = NULL;
    *cnt = tokens_cnt - 1;
    return tokens;
}

#undef USER_THREAD_C
