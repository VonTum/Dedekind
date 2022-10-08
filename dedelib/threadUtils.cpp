#include "threadUtils.h"


#include <thread>
#include <pthread.h>
#include <iostream>


// AMD Milan 7763 architecture
constexpr int CORE_COMPLEX_SIZE = 8;
constexpr int CORES_PER_NUMA = 16;
constexpr int CORES_PER_SOCKET = 64;


static size_t getAffinityBlockSize(CPUAffinityType t) {
	switch(t) {
	case CPUAffinityType::CORE:
		return 1;
	case CPUAffinityType::COMPLEX:
		return 8;
	case CPUAffinityType::NUMA_DOMAIN:
		return 16;
	case CPUAffinityType::SOCKET:
		return 64;
	}
}

static cpu_set_t createCPUSet(int cpuI, int numCPUs) {
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	for(int i = 0; i < numCPUs; i++) {
		CPU_SET(cpuI + i, &cpuset);
	}
	return cpuset;
}
cpu_set_t createCPUSet(int cpuI, CPUAffinityType t) {
	size_t blockSize = getAffinityBlockSize(t);
	return createCPUSet(cpuI * blockSize, blockSize);
}

pthread_t createPThreadAffinity(int cpuI, CPUAffinityType t, void* (*func)(void*), void* data) {
	return createPThreadAffinity(createCPUSet(cpuI, t), func, data);
}

static void setCPUAffinity(pthread_t pt, int cpuI) {
	cpu_set_t cpuset = createCPUSet(cpuI, 1);
	int rc = pthread_setaffinity_np(pt, sizeof(cpu_set_t), &cpuset);
	if (rc != 0) {
		std::cerr << "Error calling pthread_setaffinity_np: " << rc << "\n";
	}
}

static void setCoreComplexAffinity(pthread_t pt, int coreComplex) {
	cpu_set_t cpuset = createCPUSet(coreComplex * CORE_COMPLEX_SIZE, CORE_COMPLEX_SIZE);
	int rc = pthread_setaffinity_np(pt, sizeof(cpu_set_t), &cpuset);
	if (rc != 0) {
		std::cerr << "Error calling pthread_setaffinity_np: " << rc << "\n";
	}
}

static void setNUMANodeAffinity(pthread_t pt, int numaNode) {
	cpu_set_t cpuset = createCPUSet(numaNode * CORES_PER_NUMA, CORES_PER_NUMA);
	int rc = pthread_setaffinity_np(pt, sizeof(cpu_set_t), &cpuset);
	if (rc != 0) {
		std::cerr << "Error calling pthread_setaffinity_np: " << rc << "\n";
	}
}

static void setSocketAffinity(pthread_t pt, int socketI) {
	cpu_set_t cpuset = createCPUSet(socketI * CORES_PER_SOCKET, CORES_PER_SOCKET);
	int rc = pthread_setaffinity_np(pt, sizeof(cpu_set_t), &cpuset);
	if (rc != 0) {
		std::cerr << "Error calling pthread_setaffinity_np: " << rc << "\n";
	}
}

void setCPUAffinity(std::thread& th, int cpuI) {
	setCPUAffinity(th.native_handle(), cpuI);
}

void setCoreComplexAffinity(std::thread& th, int coreComplex) {
	setCoreComplexAffinity(th.native_handle(), coreComplex);
}

void setNUMANodeAffinity(std::thread& th, int numaNode) {
	setNUMANodeAffinity(th.native_handle(), numaNode);
}

void setSocketAffinity(std::thread& th, int socketI) {
	setSocketAffinity(th.native_handle(), socketI);
}


void setCPUAffinity(int cpuI) {
	setCPUAffinity(pthread_self(), cpuI);
}

void setCoreComplexAffinity(int coreComplex) {
	setCoreComplexAffinity(pthread_self(), coreComplex);
}

void setNUMANodeAffinity(int numaNode) {
	setNUMANodeAffinity(pthread_self(), numaNode);
}

void setSocketAffinity(int socketI) {
	setSocketAffinity(pthread_self(), socketI);
}


void setThreadName(const char* name) {
	pthread_setname_np(pthread_self(), name);
}

void setThreadName(std::thread& t, const char* name) {
	pthread_setname_np(t.native_handle(), name);
}


pthread_t createPThreadAffinity(cpu_set_t cpuset, void* (*func)(void*), void* data) {
	pthread_t thread;
#ifdef USE_NUMA
	pthread_attr_t pta;
	pthread_attr_init(&pta);
	pthread_attr_setaffinity_np(&pta, sizeof(cpu_set_t), &cpuset);
	if(pthread_create(&thread, &pta, func, data) != 0) {
		std::cerr << "Error creating thread!" << std::endl;
		exit(-1);
	}
	pthread_attr_destroy(&pta);
#else
	if(pthread_create(&thread, nullptr, func, data) != 0) {
		std::cerr << "Error creating thread!" << std::endl;
		exit(-1);
	}
#endif
	return thread;
}

pthread_t createCPUPThread(int cpuI, void* (*func)(void*), void* data) {
	return createPThreadAffinity(createCPUSet(cpuI, 1), func, data);
}
pthread_t createCoreComplexPThread(int coreComplex, void* (*func)(void*), void* data) {
	return createPThreadAffinity(createCPUSet(CORE_COMPLEX_SIZE * coreComplex, CORE_COMPLEX_SIZE), func, data);
}
pthread_t createNUMANodePThread(int numaNode, void* (*func)(void*), void* data) {
	return createPThreadAffinity(createCPUSet(CORES_PER_NUMA * numaNode, CORES_PER_NUMA), func, data);
}
pthread_t createSocketPThread(int socketI, void* (*func)(void*), void* data) {
	return createPThreadAffinity(createCPUSet(CORES_PER_SOCKET * socketI, CORES_PER_SOCKET), func, data);
}


pthread_t copyThreadAffinity(void* (*func)(void*), void* data) {
	cpu_set_t cpuset;
	pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
	return createPThreadAffinity(cpuset, func, data);
}
