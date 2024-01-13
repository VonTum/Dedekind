#include "flatBufferManagement.h"

#include "numaMem.h"

#include <fstream>
#include <iostream>
#ifdef __linux__
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#endif

bool BUFMANAGEMENT_MMAP = false;
bool BUFMANAGEMENT_MMAP_POPULATE = false;
bool BUFMANAGEMENT_MMAP_HUGETLB = false;
BufmanagementPageSize BUFMANAGEMENT_MMAP_PAGE_SIZE = BufmanagementPageSize::HUGETLB_4KB;

void writeFlatVoidBuffer(const void* data, const std::string& fileName, size_t size) {
	std::ofstream file(fileName, std::ios::binary);
	file.write(reinterpret_cast<const char*>(data), size);
	file.close();
}

void readFlatVoidBufferNoMMAP(const std::string& fileName, size_t size, void* buffer) {
	std::ifstream file(fileName, std::ios::binary);
	if(!file.good()) {
		std::cout << "Could not open file '" << fileName << "'!" << std::endl;
		exit(1);
	}
	file.read(static_cast<char*>(buffer), size);
	file.close();
}

/*void readFlatVoidBufferNoMMAP(const char* fileName, size_t size, void* target) {
	std::cout << "Reading flat file " << fileName << " into void*\n" << std::flush;
	FILE* file = fopen(fileName, O_RDONLY);
	if(file == NULL) {
		std::cout << "Could not open file '" << fileName << "'!" << std::endl;
		exit(1);
	}
	fread(static_cast<char*>(target), size, 1, file);
	fclose(file);
}*/

void* readFlatVoidBufferNoMMAP(const std::string& fileName, size_t size) {
	void* buffer = numa_alloc_interleaved(size); // Strong alignment is required for DMA access with OpenCL
	readFlatVoidBufferNoMMAP(fileName, size, buffer);
	return buffer;
}
void freeFlatVoidBufferNoMMAP(const void* buffer, size_t size) {
	numa_free(const_cast<void*>(buffer), size);
}
void* mmapFlatVoidBuffer(const std::string& fileName, size_t size) {
	#ifdef __linux__
	int fd = open(fileName.c_str(), O_RDONLY);
	if(fd == -1) {
		std::cout << "Could not open file '" << fileName << "'!" << std::endl;
		exit(1);
	}
	int mmapFlags = MAP_PRIVATE;
	if(BUFMANAGEMENT_MMAP_POPULATE) mmapFlags |= MAP_POPULATE;
	if(BUFMANAGEMENT_MMAP_HUGETLB) {
		mmapFlags |= MAP_HUGETLB;
		if(BUFMANAGEMENT_MMAP_PAGE_SIZE == BufmanagementPageSize::HUGETLB_2MB) {
			mmapFlags |= (21 << MAP_HUGE_SHIFT);
		} else if(BUFMANAGEMENT_MMAP_PAGE_SIZE == BufmanagementPageSize::HUGETLB_1GB) {
			mmapFlags |= (30 << MAP_HUGE_SHIFT);
		}
	}
	void* result = mmap(NULL, size, PROT_READ, mmapFlags, fd, 0);
	if(result == MAP_FAILED) {
		int err = errno;
		perror("mmap");
		if(err == EINVAL && BUFMANAGEMENT_MMAP_HUGETLB) std::cout << "This filesystem probably doesn't support HugeTLB Pages" << std::endl;
		exit(1);
	}
	close(fd);
	return result;
	#else
	throw "MMAP Not Supported on Non-Linux platforms!";
	#endif
}
void* readFlatVoidBuffer(const std::string& fileName, size_t size) {
	if(BUFMANAGEMENT_MMAP) {
		return mmapFlatVoidBuffer(fileName, size);
	} else {
		return readFlatVoidBufferNoMMAP(fileName, size);
	}
}
void free_const(const void* buffer) {
	free(const_cast<void*>(buffer));
}
void munmapFlatVoidBuffer(const void* buffer, size_t size) {
	#ifdef __linux__
	if(munmap(const_cast<void*>(buffer), size) != 0) {
		auto err = errno;
		std::cout << "Error while unmapping! Error: " << err << std::endl;
		exit(1);
	}
	#else
	throw "MMAP Not Supported on Non-Linux platforms!";
	#endif
}
void freeFlatVoidBuffer(const void* buffer, size_t size) {
	if(BUFMANAGEMENT_MMAP) {
		munmapFlatVoidBuffer(buffer, size);
	} else {
		numa_free(const_cast<void*>(buffer), size);
	}
}