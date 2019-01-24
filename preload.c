#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>

typedef void * (*orig_malloc_type)(size_t size);
typedef void (*orig_free_type)(void *ptr);

static size_t jonas_count = 0;

void *malloc(size_t size){
//	printf("MALLOC [%d]", size);
	jonas_count += size;
	orig_malloc_type orig_malloc;
	orig_malloc = (orig_malloc_type)dlsym(RTLD_NEXT, "malloc");
	return orig_malloc(size);
}

void free(void *ptr){
	printf("[SIZE] %d\n", jonas_count);
	orig_free_type orig_free;
	orig_free = (orig_free_type)dlsym(RTLD_NEXT, "free");
	return orig_free(ptr);
}

