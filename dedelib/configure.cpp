#include "configure.h"

#include "cmdParser.h"
#include "fileNames.h"
#include "flatBufferManagement.h"

#include <string>
#include <iostream>

void configure(const ParsedArgs& parsed) {
	std::string dataDir = parsed.getOptional("dataDir");
	if(!dataDir.empty()) {
		FileName::setDataPath(dataDir);
	}

	if(parsed.hasFlag("mmap")) {
		BUFMANAGEMENT_MMAP = true;
	}
	if(parsed.hasFlag("mmap_populate")) {
		BUFMANAGEMENT_MMAP = true;
		BUFMANAGEMENT_MMAP_POPULATE = true;
	}
	if(parsed.hasFlag("mmap_hugetlb")) {
		BUFMANAGEMENT_MMAP = true;
		BUFMANAGEMENT_MMAP_HUGETLB = true;
	}
	if(parsed.hasFlag("mmap_2MB")) {
		BUFMANAGEMENT_MMAP = true;
		BUFMANAGEMENT_MMAP_HUGETLB = true;
		BUFMANAGEMENT_MMAP_PAGE_SIZE = BufmanagementPageSize::HUGETLB_2MB;
	}
	if(parsed.hasFlag("mmap_1GB")) {
		BUFMANAGEMENT_MMAP = true;
		BUFMANAGEMENT_MMAP_HUGETLB = true;
		BUFMANAGEMENT_MMAP_PAGE_SIZE = BufmanagementPageSize::HUGETLB_1GB;
	}

	if(BUFMANAGEMENT_MMAP) {
		std::cout << "Using";
		if(BUFMANAGEMENT_MMAP_POPULATE) std::cout << " Populated";
		std::cout << " Memory Mapping";
		if(BUFMANAGEMENT_MMAP_HUGETLB) {
			std::cout << " With HugeTLB";
		}
		std::cout << ", Pages of Size ";
		switch(BUFMANAGEMENT_MMAP_PAGE_SIZE) {
			case BufmanagementPageSize::HUGETLB_4KB: std::cout << "4KB"; break;
			case BufmanagementPageSize::HUGETLB_2MB: std::cout << "2MB"; break;
			case BufmanagementPageSize::HUGETLB_1GB: std::cout << "1GB"; break;
		}
		std::cout << std::endl;
	}
}

