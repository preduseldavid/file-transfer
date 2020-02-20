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
        wprintf(L"Error: %S: %S(): line %d: %hs %hs \n", file, func, line, call, args);
#endif /* PRINT_ERROR_ENABLED */
    }
    else if (type == ERROR_WSA || type == ERROR_OS) {
        DWORD dwMessageId = 0;
        if (type == ERROR_OS) dwMessageId = GetLastError();
        if (type == ERROR_SOCKET) dwMessageId = WSAGetLastError();
        
        LPWSTR pBuffer = NULL;
        FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_ALLOCATE_BUFFER, 
                       NULL,
                       dwMessageId,
                       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       (LPWSTR)&pBuffer,
                       0,
                       NULL);
#ifdef PRINT_ERROR_ENABLED
        wprintf(L"Error: %S: %S(): line %d: %hs(%hs) returned [0x%08x] - %s \n", file, func, line, call, args, dwMessageId, pBuffer);
#endif /* PRINT_ERROR_ENABLED */
        LocalFree(pBuffer);
    }
}

#undef ERROR_C
