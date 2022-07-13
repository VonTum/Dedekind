#pragma once

#include <cstddef>

template<typename IntT>
IntT alignUpTo(IntT v, IntT align) {
	IntT alignDifference = v % align;
	if(alignDifference == 0) {
		return v;
	} else {
		return v + align - alignDifference;
	}
}

void* aligned_malloc(size_t size, size_t align);
void aligned_free(void* ptr);

template<typename T>
T* aligned_mallocT(size_t size, size_t align) {
	return static_cast<T*>(aligned_malloc(size*sizeof(T), align));
}
