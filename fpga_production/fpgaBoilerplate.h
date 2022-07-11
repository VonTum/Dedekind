#pragma once

#include <CL/opencl.h>
#include "../dedelib/funcTypes.h"

constexpr size_t INPUT_BUFFER_COUNT = 1;
constexpr size_t RESULT_BUFFER_COUNT = 1;

// OpenCL runtime configuration
extern cl_platform_id platform;
extern cl_device_id device;
extern cl_context context;
extern cl_command_queue queue;
extern cl_kernel fullPipelineKernel;
extern cl_program program;
extern cl_mem mbfLUTMemA;
extern cl_mem mbfLUTMemB;
extern cl_mem inputMem[INPUT_BUFFER_COUNT];
extern cl_mem resultMem[RESULT_BUFFER_COUNT];

void launchKernel(cl_mem* input, cl_mem* output, cl_uint bufferSize);

void init(const char* kernelFile, const Monotonic<7>* mbfs);
void cleanup();
