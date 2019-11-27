/**
 * @file receive.c
 * @brief Implementation of the receive part of the app
 *        Receives a file with zero copy semantics ( https://en.wikipedia.org/wiki/Zero-copy )
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <time.h>

#ifdef UNIX
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#endif /* UNIX */

#define RECEIVE_C
#include "receive.h"
#include "data_types.h"
#include "error.h"

/* internal functions' prototypes */
static int32_t receive_file(SOCKET sock_desc, char filepath[]);
static void    abort_transfer(SOCKET sock_desc, int8_t send_abortion);

/* internal variables */
static char    directory_path_prefix[PATH_SIZE];
static int8_t  aborted_transfer;

int32_t __recv(SOCKET sock_desc, flag_t flag, char path[])
{
    int8_t       s;
    int32_t      end;
    net_packet_t *packet = NULL;
    
    memcpy(directory_path_prefix, path, strlen(path));
    /* get the child nodes (directories/files) */
    end = 0;
    s = 0;
    while (!end && !s) {
        if ( (packet = recv_packet(sock_desc, 0)) == NULL) {
            s = -1;
            break;
        }
        if (packet->flags.val & DIR_TYPE && !(packet->flags.val & ABORT_TRANSFER)) {
            /* create and open it */
            char dirpath[PATH_SIZE];
            sprintf(dirpath, "%s/%s", directory_path_prefix, packet->data);
            s = mkdir(dirpath, 0777);
            if (s == -1) {
                if (errno != EEXIST) {
                    ERROR("mkdir", "", ERROR_OS);
                    abort_transfer(sock_desc, 1);
                }
                else {
                    s = 0;
                    errno = 0;
                }
            }
        }
        else if (packet->flags.val & FILE_TYPE && !(packet->flags.val & ABORT_TRANSFER)) {
            s = receive_file(sock_desc, packet->data);
        }
        else {
            end = 1;
            packet->flags.val & ABORT_TRANSFER ? fprintf(stderr, "Abort transfer...\n") :
            packet->flags.val & END_TRANSFER   ? fprintf(stderr, "End transfer\n")      :
                                                  fprintf(stderr, "Unknown error occured...\n");
        }
        fprintf(stderr, "%s\n", packet->data);
        destroy_packet(packet);
    }
    
    return s;
}

static int32_t receive_file(SOCKET sock_desc, char filepath[])
{
    char                path[PATH_SIZE];
    int32_t             pipefd[2];
    int32_t             file_desc;
    volatile uint64_t   file_size;
    volatile uint64_t   total_received = 0;
    volatile int64_t    received;
    //volatile uint64_t   last_time = 0;
    //volatile uint64_t   curr_time;
    net_packet_t        *packet = NULL;
    
    pipefd[0] = pipefd[1] = file_desc = -1;
    
    /* the file path is received */
    sprintf(path, "%s/%s", directory_path_prefix, filepath);
    if ( (file_desc = open(path, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR|S_IROTH)) == -1) {
        ERROR("open", path, ERROR_OS);
        abort_transfer(sock_desc, 1);
        goto error;
    }
    
    /* receive the file size */
    if ( (packet = recv_packet(sock_desc, 0)) == NULL)
        goto error;
    if (packet->flags.val & ABORT_TRANSFER) {
        abort_transfer(sock_desc, 0);
        goto error;
    }
    memcpy((char *) &file_size, packet->data, packet->size);
    destroy_packet(packet);
    
    /* create the pipe */
    if (pipe(pipefd) == -1) {
        ERROR("pipe", "", ERROR_OS);
        abort_transfer(sock_desc, 1);
        goto error;
    }
    
    /* receive the file */
    while (total_received < file_size) {
        if ( (received = splice(sock_desc, NULL, pipefd[1], NULL, file_size - total_received, SPLICE_F_NONBLOCK)) == -1) {
            ERROR("splice", "socket to pipe", ERROR_OS);
            abort_transfer(sock_desc, 1);
            goto error;
        }
        if ( splice(pipefd[0], NULL, file_desc, NULL, received, SPLICE_F_MOVE) == -1) {
            ERROR("splice", "pipe to file", ERROR_OS);
            abort_transfer(sock_desc, 1);
            goto error;
        }
        total_received += received;
        /*
        if ((curr_time = time(NULL)) > last_time) {
            last_time = curr_time;
            int8_t percent = ((double)total_received / file_size) * 100;
            fprintf(stderr, "File transfering: %d%%\r", percent);
        }
        */
    }
    close(file_desc);
    close(pipefd[0]);
    close(pipefd[1]);
    /* Success */
    return 0;
    
 error:
    destroy_packet(packet);
    if (file_desc != -1) close(file_desc);
    if (pipefd[0] != -1) close(pipefd[0]);
    if (pipefd[1] != -1) close(pipefd[1]);
    return -1;
}

static void abort_transfer(SOCKET sock_desc, int8_t send_abortion)
{
    /* 
     * We got an error (a system call error, a TCP/IP error or an abortion flag).
     * If the peer sent me an abortion, just set the variable and exit. Else if we got
     * an error, try to send an abortion flag to my peer
     */
    aborted_transfer = 1;
    if (send_abortion) {
#ifdef PRINT_ERROR_ENABLED
#undef PRINT_ERROR_ENABLED
#define ENABLE_PRINT
#endif /* PRINT_ERROR_ENABLED */
    send_packet(sock_desc, NULL, 0, ABORT_TRANSFER);
#ifdef ENABLE_PRINT
#define PRINT_ERROR_ENABLED
#undef ENABLE_PRINT
#endif /* ENABLE_PRINT */
    }
}

#undef RECEIVE_C
