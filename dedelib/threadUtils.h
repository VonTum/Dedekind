#pragma once

#include <thread>

void setCPUAffinity(int cpuI);
void setCPUAffinity(std::thread& th, int cpuI);

void setCoreComplexAffinity(int coreComplex);
void setCoreComplexAffinity(std::thread& th, int coreComplex);

void setSocketAffinity(int socketI);
void setSocketAffinity(std::thread& th, int socketI);
