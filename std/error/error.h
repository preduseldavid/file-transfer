#ifndef ERROR_H
#define ERROR_H

#ifdef ERROR_C
#define EXTERN
#else
#define EXTERN extern
#endif /* ERROR_C */

#define ERROR_DEFAULT       ((uint8_t) 0)
#define ERROR_APP           ((uint8_t) 1)
#define ERROR_OS            ((uint8_t) 2)
#define ERROR_SOCKET        ((uint8_t) 3)

#define PRINT_ERROR_ENABLED

#define ERROR(call, args, type) error(__FILE__, __FUNCTION__, __LINE__, call, args, type);
EXTERN void error(const char *file, const char *func, const uint32_t line, const char *call, const char *args, const uint8_t type);

#undef EXTERN
#endif /* ERROR_H */