#include "threadUtils.h"


#include <thread>
#include <pthread.h>
#include <iostream>


// AMD Milan 7763 architecture
constexpr int CORE_COMPLEX_SIZE = 8;
constexpr int CORES_PER_SOCKET = 64;

static void setCPUAffinity(pthread_t pt, int cpuI) {
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(cpuI, &cpuset);
	int rc = pthread_setaffinity_np(pt, sizeof(cpu_set_t), &cpuset);
	if (rc != 0) {
		std::cerr << "Error calling pthread_setaffinity_np: " << rc << "\n";
	}
}

static void setCoreComplexAffinity(pthread_t pt, int coreComplex) {
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	for(int i = 0; i < CORE_COMPLEX_SIZE; i++) {
		CPU_SET(coreComplex * CORE_COMPLEX_SIZE + i, &cpuset);
	}
	int rc = pthread_setaffinity_np(pt, sizeof(cpu_set_t), &cpuset);
	if (rc != 0) {
		std::cerr << "Error calling pthread_setaffinity_np: " << rc << "\n";
	}
}

static void setSocketAffinity(pthread_t pt, int socketI) {
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	for(int i = 0; i < CORES_PER_SOCKET; i++) {
		CPU_SET(socketI * CORES_PER_SOCKET + i, &cpuset);
	}
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

void setSocketAffinity(std::thread& th, int socketI) {
	setSocketAffinity(th.native_handle(), socketI);
}

void setCPUAffinity(int cpuI) {
	setCPUAffinity(pthread_self(), cpuI);
}

void setCoreComplexAffinity(int coreComplex) {
	setCoreComplexAffinity(pthread_self(), coreComplex);
}

void setSocketAffinity(int socketI) {
	setSocketAffinity(pthread_self(), socketI);
}

