#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>

#ifndef DATA_TYPES_H
#define DATA_TYPES_H

#ifdef DATA_TYPES_C
#define EXTERN
#else
#define EXTERN extern
#endif /* DATA_TYPES_C */

#define flag_t int32_t

#ifdef UNIX
#define SOCKET int32_t
#endif /* UNIX */

/* operations between peers (flags) */
typedef enum {
    START_TRANSFER         = 0x001,
    ABORT_TRANSFER         = 0x002,
    END_TRANSFER           = 0x004,
    CONTINUE_TRANSFER      = 0x008,
    FILE_TYPE              = 0x010,
    DIR_TYPE               = 0x020,
    SEND_OPERATION         = 0x040,
    RECEIVE_OPERATION      = 0x080,
    FILE_SIZE              = 0x100
} communication_protocol_flags;

typedef enum {
    SEND_BUF_SIZE          = 512,
    PATH_SIZE              = 1024,
    NET_PACKET_HEADER_SIZE = sizeof(uint32_t) + sizeof(flag_t)
} size_limits;

typedef union {
    flag_t      val;
    char        str[sizeof(flag_t)];
} flag_union;

/* network packets between peers */
typedef struct {
    uint32_t    size;
    flag_union  flags;
    char        *data;
} net_packet_t;

/* functions */
EXTERN int8_t send_packet(SOCKET sock_desc, char *buff, uint32_t size, flag_t flags);
EXTERN net_packet_t *recv_packet(SOCKET sock_desc, int recv_flags);
EXTERN void destroy_packet(net_packet_t *packet);

#undef EXTERN
#endif /* DATA_TYPES_H */
