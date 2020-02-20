#include <inttypes.h>

#ifndef RECEIVE_FILE_H
#define RECEIVE_FILE_H

#ifdef RECEIVE_FILE_C
#define EXTERN
#else
#define EXTERN extern
#endif /* RECEIVE_FILE_C */

EXTERN int32_t receive_file_linux(int32_t sock_desc, char *path, uint64_t filesize);

#undef EXTERN
#endif /* RECEIVE_FILE_H */
