#include "vector.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>


vector* init(size_t s)
{
	vector* vec = (vector*)calloc(1,sizeof(vector));
	assert(vec);

	pthread_mutex_init( &(vec->mu),NULL );
	pthread_cond_init(&vec->cond,NULL);
	vec->capacity = s;
	vec->count = 0;
	vec->vec = (int*)calloc(s,sizeof(int));
	assert(vec->vec);

	return vec;
}

void destroy(vector* v)
{
	pthread_mutex_destroy(&v->mu);
	pthread_cond_destroy(&v->cond);
	free(v->vec);
}

void fd_remove(vector* v,int fd)
{
	size_t i = 0;
	for(; i<v->count; i++)
	{
		if(v->vec[i] == fd)
		{
			memmove(v->vec+i,v->vec+i+1,(v->count-i-1)*sizeof(int));
			break;
		}
	}
	assert(i != v->count);
	--v->count;
}


void fd_push(vector* v,int fd)
{
	if(v->count == v->capacity)
	{
		size_t new = v->capacity * 2;
		v->vec = realloc(v->vec,new*sizeof(int));
		memset(v->vec + v->capacity*sizeof(int), 0, sizeof(int)*v->capacity);
		v->capacity = new;
	}

	v->vec[v->count] = fd;
	v->count++;
}

