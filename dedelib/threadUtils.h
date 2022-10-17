#pragma once

#include <thread>
#include <pthread.h>

enum class CPUAffinityType {
	CORE = 1,
	COMPLEX = 8,
	NUMA_DOMAIN = 16,
	SOCKET = 64
};

cpu_set_t createCPUSet(int cpuI, CPUAffinityType t);
pthread_t createPThreadAffinity(cpu_set_t cpuset, void* (*func)(void*), void* data);
pthread_t createPThreadAffinity(int cpuI, CPUAffinityType t, void* (*func)(void*), void* data);

void setCPUAffinity(int cpuI);
void setCPUAffinity(std::thread& th, int cpuI);

void setCoreComplexAffinity(int coreComplex);
void setCoreComplexAffinity(std::thread& th, int coreComplex);

void setNUMANodeAffinity(int numaNode);
void setNUMANodeAffinity(std::thread& th, int numaNode);

void setSocketAffinity(int socketI);
void setSocketAffinity(std::thread& th, int socketI);

void setThreadName(const char* name);
void setThreadName(std::thread& t, const char* name);

pthread_t createCPUPThread(int cpuI, void* (*func)(void*), void* data);
pthread_t createCoreComplexPThread(int coreComplex, void* (*func)(void*), void* data);
pthread_t createNUMANodePThread(int numaNode, void* (*func)(void*), void* data);
pthread_t createSocketPThread(int socketI, void* (*func)(void*), void* data);

pthread_t copyThreadAffinity(void* (*func)(void*), void* data);
