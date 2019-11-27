/**
 * @file receive.c
 * @brief The receive op. header
 */

#include <inttypes.h>

#include "data_types.h"

#ifndef RECEIVE_H
#define RECEIVE_H

#ifdef RECEIVE_C
#define EXTERN
#else
#define EXTERN extern
#endif /* RECEIVE_C */

/* receive functions */
EXTERN int32_t __recv(SOCKET sock_desc, flag_t flag, char path[]);

#undef EXTERN
#endif /* RECEIVE_H */
