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
void* readFlatVoidBuffer(const std::string& fileName, size_t size);
void readFlatVoidBufferNoMMAP(const std::string& fileName, size_t size, void* buffer);
void* readFlatVoidBufferNoMMAP(const std::string& fileName, size_t size);
void* mmapFlatVoidBuffer(const std::string& fileName, size_t size);
void munmapFlatVoidBuffer(const void* buf, size_t size);
void freeFlatVoidBuffer(const void* buffer, size_t size);
void free_const(const void* buffer);

template<typename T>
void writeFlatBuffer(const T* data, const std::string& fileName, size_t size) {
	writeFlatVoidBuffer(data, fileName, sizeof(T) * size);
}

template<typename T>
const T* readFlatBuffer(const std::string& fileName, size_t size) {
	return static_cast<const T*>(readFlatVoidBuffer(fileName.c_str(), sizeof(T) * size));
}

template<typename T>
const T* readFlatBufferNoMMAP(const std::string& fileName, size_t size) {
	return static_cast<const T*>(readFlatVoidBufferNoMMAP(fileName.c_str(), sizeof(T) * size));
}

template<typename T>
void freeFlatBuffer(const T* buffer, size_t size) {
	freeFlatVoidBuffer(buffer, sizeof(T) * size);
}
