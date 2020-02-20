/**
 * @file receive_file_linux.c
 * @brief Receives a file with zero copy semantics ( https://en.wikipedia.org/wiki/Zero-copy )
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#include "config.h"
#include "data_types.h"
#include "error.h"
#include "receive.h"
#include "send.h"

#define RECEIVE_FILE_C
#include "receive_file_linux.h"

/* internal variables */
static int8_t aborted_transfer;

int32_t receive_file_linux(int32_t sock_desc, char *path, uint64_t filesize)
{
    int32_t     pipefd[2];
    int32_t     file_desc;
    uint64_t    total_received = 0;
    int64_t     received;
    time_t      now;
    time_t      last_time;
    pipefd[0] = pipefd[1] = file_desc = -1;
    
    /* Reset the abortion */
    aborted_transfer = 0;
    
    /* open the file */
    if ( (file_desc = open(path, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR|S_IROTH)) == -1) {
        ERROR("open", path, ERROR_OS);
        abort_transfer(sock_desc, &aborted_transfer, 1);
        goto error;
    }
    /* create the pipe */
    if (pipe(pipefd) == -1) {
        ERROR("pipe", "", ERROR_OS);
        abort_transfer(sock_desc, &aborted_transfer, 1);
        goto error;
    }
    /* receive the file */
    time(&last_time);
    while (total_received < filesize) {
        if ((received = splice(sock_desc, NULL, pipefd[1], NULL, filesize - total_received, SPLICE_F_NONBLOCK)) == -1) {
            ERROR("splice", "socket to pipe", ERROR_OS);
            abort_transfer(sock_desc, &aborted_transfer, 1);
            goto error;
        }
        if (splice(pipefd[0], NULL, file_desc, NULL, received, SPLICE_F_MOVE) == -1) {
            ERROR("splice", "pipe to file", ERROR_OS);
            abort_transfer(sock_desc, &aborted_transfer, 1);
            goto error;
        }
        total_received += received;
        
#ifdef PRINT_PERCENTAGE
        if (time(&now) > last_time) {
            fprintf(stdout, "%.1lf %%\r", ((double)total_received / filesize) * 100);
            fflush(stdout);
            last_time = now;
        }
#endif /* PRINT_PERCENTAGE */
    }
    
    close(file_desc);
    close(pipefd[0]);
    close(pipefd[1]);
    /* Success */
    return 0;
    
 error:
    if (file_desc != -1) close(file_desc);
    if (pipefd[0] != -1) close(pipefd[0]);
    if (pipefd[1] != -1) close(pipefd[1]);
    return -1;
}

#undef RECEIVE_FILE_C
