#pragma once

#include <thread>
#include <pthread.h>

void setCPUAffinity(int cpuI);
void setCPUAffinity(std::thread& th, int cpuI);

void setCoreComplexAffinity(int coreComplex);
void setCoreComplexAffinity(std::thread& th, int coreComplex);

void setSocketAffinity(int socketI);
void setSocketAffinity(std::thread& th, int socketI);

pthread_t createCPUPThread(int cpuI, void* (*func)(void*), void* data);
pthread_t createCoreComplexPThread(int coreComplex, void* (*func)(void*), void* data);
pthread_t createSocketPThread(int socketI, void* (*func)(void*), void* data);

pthread_t copyThreadAffinity(void* (*func)(void*), void* data);
