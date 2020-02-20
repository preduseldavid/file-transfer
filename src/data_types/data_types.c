#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>

#ifdef UNIX
#include <sys/types.h>
#include <sys/socket.h>
#endif /* UNIX */

#define DATA_TYPES_C
#include "data_types.h"
#include "error.h"
#include "config.h"

/* internal functions' prototypes */
inline static uint32_t char_to_uint32(char *buff);

int8_t send_packet(SOCKET sock_desc, char *buff, uint32_t size, flag_t flags)
{
    int32_t     remaining = size;
    int32_t     sent_size;
    char        *offset = buff;
    char        header[NET_PACKET_HEADER_SIZE + 1];
    
    /* Send the header */
    memcpy(header, &size, sizeof(size));
    memcpy(&header[sizeof(size)], &flags, sizeof(flags));
    if (send(sock_desc, header, sizeof(header) - 1, 0) == -1) {
        ERROR("send", "", ERROR_OS);
        return -1;
    }
    
    /* Send the data */
    while (remaining > 0) {
        if ((sent_size = send(sock_desc, offset, remaining, 0)) == -1) {
            ERROR("send", "", ERROR_OS);
            return -1;
        }
        remaining -= sent_size;
        offset += sent_size;
    }
    /* Success */
    return 0;
}

net_packet_t *recv_packet(SOCKET sock_desc, int recv_flags)
{   
    net_packet_t *packet = (net_packet_t *) malloc(sizeof(net_packet_t));
    char         header[NET_PACKET_HEADER_SIZE + 1];
    uint32_t     remaining;
    int32_t      recv_size;
    char         *data = NULL;
    
    memset(header, 0, sizeof(header));
    /* Receive the header */
    errno = 0;
    if (recv(sock_desc, header, sizeof(header) - 1, recv_flags) <= 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            errno = 0;
            memset(header, 0, sizeof(header));
        }
        else {
            ERROR("recv", "", ERROR_OS);
            goto error;
        }
    }
    packet->size = char_to_uint32(header);
    memcpy(packet->flags.str, &header[sizeof(packet->size)], sizeof(flag_t));
    data = (char *) malloc(sizeof(char) * packet->size + 1);
    packet->data = data;
    remaining = packet->size;
    
    /* Receive the data */
    errno = 0;
    while (remaining > 0) {
        if ((recv_size = recv(sock_desc, data, packet->size, recv_flags)) <= 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                errno = 0;
                break;
            }
            else {
                ERROR("recv", "", ERROR_OS);
                goto error;
            }
        }
        remaining -= recv_size;
        data += recv_size;
    }
    /* Success */
    *data = '\0';
    return packet;
    
 error:
    free(data);
    free(packet);
    return NULL;
}

void destroy_packet(net_packet_t *packet)
{
    if (!packet) return;
    free(packet->data);
    free(packet);
    packet = NULL;
}

inline static uint32_t char_to_uint32(char *buff)
{
    uint32_t val;
    memcpy(&val, buff, sizeof(val));
    return val;
}

#undef DATA_TYPES_C
