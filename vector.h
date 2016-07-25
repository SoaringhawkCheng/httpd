#ifndef __VERTOR_H__
#define __VERTOR_H__

#include <pthread.h>

typedef struct vector{
	pthread_mutex_t mu;
	pthread_cond_t cond;
	size_t capacity;
	size_t count;
	int *vec;
}vector;

extern void fd_remove(vector* v,int fd);
extern void fd_push(vector* v,int fd);
extern vector* init(size_t s);
extern void destroy(vector* vec);

#endif