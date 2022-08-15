#pragma once

#include <CL/opencl.h>
#include "../dedelib/funcTypes.h"


static constexpr uint64_t MEMORY_SIZE = 1024*1024ULL*1024*8*2; // 16GB of available memory
static constexpr uint64_t BUFFER_SIZE = 214319104; // Can fit 5 buffers more than getMaxDeduplicatedBufferSize(7), aligned to 2^14 = 16384 elements

constexpr size_t NUM_BUFFERS = 6;

// OpenCL runtime configuration
extern cl_platform_id platform;
extern cl_uint numDevices;
extern cl_device_id* devices;

void launchKernel(cl_mem* input, cl_mem* output, cl_uint bufferSize, cl_uint numEventsInWaitList = 0, const cl_event* eventWaitlist = NULL, cl_event* eventOutput = NULL);

void initPlatform();
const uint64_t* initMBFLUT(const void* voidMBFs);
void init(const char* kernelFile);
void cleanup();
