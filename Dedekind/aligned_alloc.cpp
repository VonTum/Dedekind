#include "aligned_alloc.h"

#include <malloc.h>

void* aligned_malloc(size_t size, size_t align) {
	return _aligned_malloc(size, align);
}
void aligned_free(void* ptr){
	_aligned_free(ptr);
}
