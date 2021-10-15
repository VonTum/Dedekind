#include "flatBufferManagement.h"

#include <fstream>
#include <iostream>
#ifdef __linux__
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#endif

bool BUFMANAGEMENT_MMAP = false;
bool BUFMANAGEMENT_MMAP_POPULATE = false;
BufmanagementPageSize BUFMANAGEMENT_MMAP_PAGE_SIZE = BufmanagementPageSize::HUGETLB_4KB;

void writeFlatVoidBuffer(const void* data, const std::string& fileName, size_t size) {
	std::ofstream file(fileName, std::ios::binary);
	file.write(reinterpret_cast<const char*>(data), size);
	file.close();
}
const void* readFlatVoidBuffer(const std::string& fileName, size_t size) {
	if(BUFMANAGEMENT_MMAP) {
		#ifdef __linux__
		int fd = open(fileName.c_str(), O_RDONLY);
		if(fd == -1) {
			std::cout << "Could not open file '" << fileName << "'!" << std::endl;
			exit(1);
		}
		int mmapFlags = MAP_PRIVATE;
		if(BUFMANAGEMENT_MMAP_POPULATE) mmapFlags |= MAP_POPULATE;
		if(BUFMANAGEMENT_MMAP_PAGE_SIZE != BufmanagementPageSize::HUGETLB_4KB) {
			mmapFlags |= MAP_HUGETLB;
			if(BUFMANAGEMENT_MMAP_PAGE_SIZE == BufmanagementPageSize::HUGETLB_2MB) {
				mmapFlags |= (21 << MAP_HUGE_SHIFT);
			} else if(BUFMANAGEMENT_MMAP_PAGE_SIZE == BufmanagementPageSize::HUGETLB_1GB) {
				mmapFlags |= (30 << MAP_HUGE_SHIFT);
			}
		}
		void* result = mmap(NULL, size, PROT_READ, mmapFlags, fd, 0);
		close(fd);
		return result;
		#else
		throw "MMAP Not Supported on Non-Linux platforms!";
		#endif
	} else {
		std::ifstream file(fileName, std::ios::binary);
		if(!file.good()) {
			std::cout << "Could not open file '" << fileName << "'!" << std::endl;
			exit(1);
		}
		void* buffer = malloc(size);
		file.read(static_cast<char*>(buffer), size);
		file.close();
		return buffer;
	}
}
void free_const(const void* buffer) {
	free(const_cast<void*>(buffer));
}
void freeFlatVoidBuffer(const void* buffer, size_t size) {
	if(BUFMANAGEMENT_MMAP) {
		#ifdef __linux__
		if(munmap(const_cast<void*>(buffer), size) != 0) {
			auto err = errno;
			std::cout << "Error while unmapping! Error: " << err << std::endl;
			exit(1);
		}
		#else
		throw "MMAP Not Supported on Non-Linux platforms!";
		#endif
	} else {
		free(const_cast<void*>(buffer));
	}
}