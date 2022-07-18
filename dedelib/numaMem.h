#pragma once

#include <stddef.h>

void allocSocketBuffers(size_t bufSize, void* socketBuffers[2]);
void allocCoreComplexBuffers(size_t bufSize, void* buffers[8]);
void duplicateNUMAData(const void* from, void** buffers, size_t numBuffers, size_t bufferSize);
