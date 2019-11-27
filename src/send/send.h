/**
 * @file send.h
 * @brief The send op. header
 */

#ifndef SEND_H
#define SEND_H

#include <inttypes.h>

#include "data_types.h"

#ifdef SEND_C
#define EXTERN
#else
#define EXTERN extern
#endif /* SEND_C */

/* send functions */
EXTERN int8_t __send(SOCKET sock_desc, char *path);

#undef EXTERN
#endif /* SEND_H */