#ifndef USER_THREAD_H
#define USER_THREAD_H

#ifdef USER_THREAD_C
#define EXTERN
#else
#define EXTERN extern
#endif /* USER_THREAD_C */

typedef struct {
    TC_t *TC;
} user_thread_arg_t;

EXTERN void user_thread(void *arg_ptr);

#undef EXTERN
#endif /* USER_THREAD_H */