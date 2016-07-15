#ifndef _THREAD_H_
#define _THREAD_H

#include <pthread.h>
// Threads

#define	PI_THREAD(X)	void *X (void *dummy)

// Threads

extern int  piThreadCreate      (void *(*fn)(void *)) ;
extern void piLock              (int key) ;
extern void piUnlock            (int key) ;
extern void init_mutex(int);

#endif
