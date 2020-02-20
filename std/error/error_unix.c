#define ERROR_C

#define _ISOC99_SOURCE
#include <stdio.h>
#include <wchar.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "error.h"

void error(const char *file, const char *func, const uint32_t line, const char *call, const char *args, const uint8_t type)
{   
    if (type == ERROR_APP) {
#ifdef PRINT_ERROR_ENABLED
        fwprintf(stderr, L"Error: %s: %s(): line %d: %hs %hs \n", file, func, line, call, args);
        fflush(stderr);
#endif /* PRINT_ERROR_ENABLED */
    }
    else if (type == ERROR_OS) {
#ifdef PRINT_ERROR_ENABLED
        fwprintf(stderr, L"Error: %s: %s(): line %d: %hs(%hs) returned [0x%08x] - %s \n",
                 file, func, line, call, args, errno, strerror(errno));
        fflush(stderr);
#endif /* PRINT_ERROR_ENABLED */
    }
}

#undef ERROR_C
