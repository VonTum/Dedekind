#pragma once

#include <stddef.h>
#include <string>


extern bool BUFMANAGEMENT_MMAP;
extern bool BUFMANAGEMENT_MMAP_POPULATE;
extern bool BUFMANAGEMENT_MMAP_HUGETLB;
enum class BufmanagementPageSize {
	HUGETLB_4KB,
	HUGETLB_2MB,
	HUGETLB_1GB
};
extern BufmanagementPageSize BUFMANAGEMENT_MMAP_PAGE_SIZE;


void writeFlatVoidBuffer(const void* data, const std::string& fileName, size_t size);
const void* readFlatVoidBuffer(const std::string& fileName, size_t size);
void freeFlatVoidBuffer(const void* buffer, size_t size);
void free_const(const void* buffer);

template<typename T>
void writeFlatBuffer(const T* data, const std::string& fileName, size_t size) {
	writeFlatVoidBuffer(data, fileName, sizeof(T) * size);
}

template<typename T>
const T* readFlatBuffer(const std::string& fileName, size_t size) {
	return static_cast<const T*>(readFlatVoidBuffer(fileName, sizeof(T) * size));
}

template<typename T>
void freeFlatBuffer(const T* buffer, size_t size) {
	freeFlatVoidBuffer(buffer, sizeof(T) * size);
}
