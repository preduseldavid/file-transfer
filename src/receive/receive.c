/**
 * @file receive.c
 * @brief Implementation of the receive part of the app
 *
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

#ifdef LINUX
#include "receive_file_linux.h"
#endif /* LINUX */

#define RECEIVE_C
#include "config.h"
#include "receive.h"
#include "send.h"
#include "data_types.h"
#include "error.h"

/* internal functions' prototypes */
static int32_t receive_file(SOCKET sock_desc, char filepath[]);

/* internal variables */
static char    directory_path_prefix[PATH_SIZE];
static int8_t  aborted_transfer;

int32_t __recv(SOCKET sock_desc, flag_t flag, char path[])
{
    int8_t       s;
    int32_t      end;
    net_packet_t *packet = NULL;
    
    /* Reset the abortion */
    aborted_transfer = 0;
    
    fprintf(stdout, "Starting to receive...\n");
    
    memcpy(directory_path_prefix, path, strlen(path));
    /* get the child nodes (directories/files) */
    for (end = 0, s = 0; !end && !s; ) {
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
                    ERROR("mkdir", dirpath, ERROR_OS);
                    abort_transfer(sock_desc, &aborted_transfer, 1);
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
            packet->flags.val & ABORT_TRANSFER ? fprintf(stdout, "Abort transfer...\n") :
            packet->flags.val & END_TRANSFER   ? fprintf(stdout, "End transfer\n")      :
                                                 fprintf(stdout, "Unknown error occured...\n");
        }
        destroy_packet(packet);
    }
    return s;
}

static int32_t receive_file(SOCKET sock_desc, char filepath[])
{
    char                path[PATH_SIZE];
    volatile uint64_t   filesize;
    net_packet_t        *packet = NULL;
    int32_t             s = 0;
    
    /* the file path is received */
    sprintf(path, "%s/%s", directory_path_prefix, filepath);
    /* receive the file size */
    if ( (packet = recv_packet(sock_desc, 0)) == NULL)
        goto error;
    if (packet->flags.val & ABORT_TRANSFER) {
        abort_transfer(sock_desc, &aborted_transfer, 0);
        goto error;
    }
    memcpy((char *) &filesize, packet->data, packet->size);
    destroy_packet(packet);
    
    fprintf(stdout, "Receiving file %s ...\n", path);
    
#ifdef LINUX
    s = receive_file_linux(sock_desc, path, filesize);
#endif /* LINUX */
    return s;
    
 error:
    destroy_packet(packet);
    return -1;
}

#undef RECEIVE_C
