#pragma once

#include <stddef.h>
#include <cassert>

#ifdef USE_NUMA
#include <numa.h>
#else
void numa_free(void* ptr, size_t size);
void* numa_alloc_onnode(size_t size, int numaNode);
#endif

void* numa_alloc_onsocket(size_t size, unsigned int socket);

void* allocInterleaved(size_t bufSize, const char* nodeString);
void allocSocketBuffers(size_t bufSize, void* socketBuffers[2]);
void allocNumaNodeBuffers(size_t bufSize, void* buffers[8]);
void duplicateNUMAData(const void* from, void** buffers, size_t numBuffers, size_t bufferSize);

template<typename T>
T* numa_alloc_T(size_t bufSize, size_t numaNode) {
	return (T*) numa_alloc_onnode(bufSize * sizeof(T), numaNode);
}

template<typename T>
T* numa_alloc_interleaved_T(size_t bufSize, const char* nodeString) {
	return (T*) allocInterleaved(bufSize * sizeof(T), nodeString);
}

template<typename T>
T* numa_alloc_socket_T(size_t bufSize, unsigned int socket) {
	return (T*) numa_alloc_onsocket(bufSize * sizeof(T), socket);
}

template<typename T>
void numa_free_T(T* mem, size_t bufSize) {
	numa_free((void*) mem, bufSize * sizeof(T));
}

template<typename T>
void numa_deleter_T(T* mem) {
	mem->~T();
	numa_free(mem, sizeof(T));
}

template<typename T, typename... Args>
T* numa_alloc_T(int numaNode, Args... args) {
	T* result = (T*) numa_alloc_onnode(sizeof(T), numaNode);
	new(result) T(args...);
	return result;
}

template<typename T>
struct unique_numa_ptr {
	T* ptr;

	unique_numa_ptr() : ptr(nullptr) {}
	unique_numa_ptr(T* ptr) : ptr(ptr) {}

	template<typename... Args>
	static unique_numa_ptr alloc_onnode(int numaNode, Args... args) {
		T* result = (T*) numa_alloc_onnode(sizeof(T), numaNode);
		new(result) T(args...);
		return unique_numa_ptr(result);
	}

	template<typename... Args>
	static unique_numa_ptr alloc_onsocket(int socketI, Args... args) {
		T* result = (T*) numa_alloc_onsocket(sizeof(T), socketI);
		new(result) T(args...);
		return unique_numa_ptr(result);
	}

	unique_numa_ptr(unique_numa_ptr&& other) noexcept : ptr(other.ptr) {
		other.ptr = nullptr;
	}
	unique_numa_ptr& operator=(unique_numa_ptr&& other) noexcept {
		T* tmp = this->ptr;
		this->ptr = other.ptr;
		other.ptr = tmp;
		return *this;
	}

	unique_numa_ptr(const unique_numa_ptr&) = delete;
	unique_numa_ptr& operator=(const unique_numa_ptr&) = delete;

	~unique_numa_ptr() {
		if(ptr != nullptr) {
			ptr->~T();
			numa_free(ptr, sizeof(T));
		}
	}

	T& operator*() const {return *ptr;}
	T* operator->() const {return ptr;}
};

template<typename T>
struct NUMAArray {
	T* buf;
	T* bufEnd;
	
	NUMAArray() : buf(nullptr) {}
	NUMAArray(T* buf, size_t size) : buf(buf), bufEnd(buf + size) {}

	static NUMAArray alloc_onnode(size_t size, int numaNode) {
		return NUMAArray((T*) numa_alloc_onnode(sizeof(T) * size, numaNode), size);
	}
	static NUMAArray alloc_onsocket(size_t size, int socketI) {
		return NUMAArray((T*) numa_alloc_onsocket(sizeof(T) * size, socketI), size);
	}

	~NUMAArray() {
		if(this->buf != nullptr) {
			size_t size = bufEnd - buf;
			numa_free((void*) this->buf, sizeof(T) * size);
		}
	}
	NUMAArray& operator=(NUMAArray&& other) noexcept {
		T* b = this->buf; this->buf = other.buf; other.buf = b;
		T* be = this->bufEnd; this->bufEnd = other.bufEnd; other.bufEnd = be;
		return *this;
	}
	NUMAArray(NUMAArray&& other) noexcept : buf(other.buf), bufEnd(other.bufEnd) {
		other.buf = nullptr;
	}
	NUMAArray(const NUMAArray& other) = delete;
	NUMAArray& operator=(const NUMAArray& other) = delete;
	
	T& operator[](size_t idx) {assert(buf + idx < bufEnd); return buf[idx];}

	bool owns(const T* ptr) const {
		return ptr >= buf && ptr < bufEnd;
	}
};
