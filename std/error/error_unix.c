#define ERROR_C

#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>

#include "error.h"

void error(const char *file, const char *func, const uint32_t line, const char *call, const char *args, const uint8_t type)
{
    if (type == ERROR_APP) {
#ifdef PRINT_ERROR_ENABLED
        wprintf(L"Error: %s: %s(): line %d: %hs %hs \n", file, func, line, call, args);
#endif /* PRINT_ERROR_ENABLED */
    }
    else if (type == ERROR_OS) {
#ifdef PRINT_ERROR_ENABLED
        wprintf(L"Error: %s: %s(): line %d: %hs(%hs) returned [0x%08x] - %s \n", file, func, line, call, args, errno, strerror(errno));
#endif /* PRINT_ERROR_ENABLED */
    }
}

#undef ERROR_C
