#pragma once

#include <cstddef>

void* aligned_malloc(std::size_t size, std::size_t align);
void aligned_free(void* ptr);

template<typename T>
T* aligned_mallocT(std::size_t size, std::size_t align) {
	return static_cast<T*>(aligned_malloc(size*sizeof(T), align));
}
