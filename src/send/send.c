/**
 * @file send.c
 * @brief The send op. implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <unistd.h>

#ifdef UNIX
#include <unistd.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <dirent.h>
#include <time.h>
#endif /* UNIX */

#define SEND_C
#include "config.h"
#include "send.h"
#include "data_types.h"
#include "error.h"

/* internal functions' prototypes */
static int8_t send_directory(SOCKET sock_desc, char *dirpath, int32_t node);
static int8_t send_file(SOCKET sock_desc, char *path);

/* internal variables */
static int32_t send_directory_prefix_len;
static int8_t  aborted_transfer;

int8_t __send(SOCKET sock_desc, char *path)
{
    int32_t     s;
    flag_union  flag;
    struct stat statbuf;
    
    /* Reset the abortion */
    aborted_transfer = 0;
    
    /* the sending path is a regular file or a directory? */
    if (stat(path, &statbuf) == -1) {
        ERROR("stat", path, ERROR_OS);
        abort_transfer(sock_desc, &aborted_transfer, 1);
        return -1;
    }
    
    /* get the main directory (the last directory from path) */
    char *main_dir = strrchr(path, '/') + 1;
    send_directory_prefix_len = strlen(path) - strlen(main_dir);
    
    flag.val = START_TRANSFER | SEND_OPERATION;
    if (send_packet(sock_desc, NULL, 0, flag.val) == -1)
        return -1;
    
    if (S_ISDIR(statbuf.st_mode)) {
        s = send_directory(sock_desc, path, 1);
    }
    else if (S_ISREG(statbuf.st_mode)) {
        s = send_file(sock_desc, path);
    }
    
    if (s != -1) {
        flag.val = END_TRANSFER;
        if (send_packet(sock_desc, NULL, 0, flag.val) == -1)
            s = -1;
    }
    fprintf(stdout, "End transfering...\n");
    fflush(stdout);
    return s;
}

/** 
 * Unsafe @function, it is @recursive (but not tail recursive) and risks a stack overflow
 * To avoid it, path size must be a finite number
 */
int8_t send_directory(SOCKET sock_desc, char *dirpath, int32_t node)
{   
    DIR             *dir;
    struct dirent   *entry;
    int32_t         s = 0;
    int32_t         ret;
    flag_t          flag;
    uint32_t        send_size;
    char            *send_data;
    char            path[PATH_SIZE];
    
    dir = opendir(dirpath);
    if (!dir) {
        ERROR("opendir", dirpath, ERROR_OS);
        abort_transfer(sock_desc, &aborted_transfer, 1);
        return -1;
    }
    entry = readdir(dir);
    if (!entry) {
        ERROR("readdir", dirpath, ERROR_OS);
        abort_transfer(sock_desc, &aborted_transfer, 1);
        return -1;
    }
    
    /* send the name of directory */
    flag = DIR_TYPE;
    send_data = &dirpath[send_directory_prefix_len];
    send_size = strlen(&dirpath[send_directory_prefix_len]);
    s = send_packet(sock_desc, send_data, send_size, flag);
    if (s == -1) {
        closedir(dir);
        abort_transfer(sock_desc, &aborted_transfer, 1);
        return -1;
    }
    
    do {
        /* directory type */
        if (entry->d_type == DT_DIR) {
            int32_t len = snprintf(path, sizeof(path)-1, "%s/%s", dirpath, entry->d_name);
            path[len] = 0;
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;
            
            ret = send_directory(sock_desc, path, node + 1);
            if (ret == -1)
                s = -1;
        }
        /* file type */
        else {
            int32_t len = snprintf(path, sizeof(path) - 1, "%s/%s", dirpath, entry->d_name);
            path[len] = 0;
            ret = send_file(sock_desc, path);
            if (ret == -1)
                s = -1;
        }
        /* for readdir function test */
        errno = 0;
    } while (!s && !aborted_transfer && (entry = readdir(dir)));
    
    if (errno) {
        ERROR("readdir", dirpath, ERROR_OS);
        abort_transfer(sock_desc, &aborted_transfer, 1);
        s = -1;
    }
    
    closedir(dir);
    return s;
}

int8_t send_file(SOCKET sock_desc, char *path)
{
    int32_t     s;
    uint64_t    filesize;
    uint64_t    total_sent;
    int64_t     sent;
    int32_t     file_desc = -1;
    int         on = 1;
    int         off = 0;
    struct stat stat_buf;
    off_t       offset;
    flag_t      flag = 0;
    
    fprintf(stdout, "Sending %s ...\n", path);
    fflush(stdout);
    
    /* open the file to be sent */
    file_desc = open(path, O_RDONLY);
    if (file_desc == -1) {
        ERROR("open", path, ERROR_OS);
        abort_transfer(sock_desc, &aborted_transfer, 1);
        goto error;
    }
    
    /* send the file path of the file (starting from the sending directory offset) */
    flag = FILE_TYPE;
    s = send_packet(sock_desc, &path[send_directory_prefix_len],
                strlen(&path[send_directory_prefix_len]), flag);
    if (s == -1) {
        goto error;
    }
    /* send the file size */
    if (fstat(file_desc, &stat_buf) == -1) {
        ERROR("fstat", path, ERROR_OS);
        abort_transfer(sock_desc, &aborted_transfer, 1);
        goto error;
    }
    flag |= FILE_SIZE;
    s = send_packet(sock_desc, (char *)&(stat_buf.st_size), sizeof(stat_buf.st_size), flag);
    if (s == -1) {
        goto error;
    }
    
    /* employ TCP_CORK option to tune performance */
    if (setsockopt(sock_desc, IPPROTO_TCP, TCP_CORK, (void *) &on, sizeof(on)) == -1) {
        ERROR("setsockopt", "TCP_CORK, on", ERROR_OS);
        abort_transfer(sock_desc, &aborted_transfer, 1);
        goto error;
    }
    /* begin the transfer using sendfile */
    offset = 0;
    filesize = stat_buf.st_size;
    total_sent = 0;
    while (total_sent < filesize) {
        sent = sendfile(sock_desc, file_desc, (void *) &offset, filesize - total_sent);
        if (sent == -1) {
            ERROR("sendfile", path, ERROR_OS);
            abort_transfer(sock_desc, &aborted_transfer, 1);
            goto error;
        }
        total_sent += sent;
    }
    
    /* to ensure all waiting data is sent, TCP_CORK must be removed */
    if (setsockopt(sock_desc, IPPROTO_TCP, TCP_CORK, &off, sizeof(off)) == -1) {
        ERROR("setsockopt", "TCP_CORK, off", ERROR_OS);
        abort_transfer(sock_desc, &aborted_transfer, 1);
        goto error;
    }
    /* SUCCESS transfer */
    return 0;
    
 error:
    if (file_desc != -1) close(file_desc);
    return -1;
}

void abort_transfer(SOCKET sock_desc, int8_t *abortion_var, int8_t send_abortion)
{
    /* 
     * We got an error (a system call error, a TCP/IP error or an abortion flag).
     * If the peer sent me an abortion, just set the variable and exit. Else if we got
     * an error, try to send an abortion flag to my peer
     */
    if (abortion_var != NULL)
        *abortion_var = 1;
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

#undef SEND_C
