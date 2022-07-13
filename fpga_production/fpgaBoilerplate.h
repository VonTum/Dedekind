#pragma once

#include <CL/opencl.h>
#include "../dedelib/funcTypes.h"

// OpenCL runtime configuration
extern cl_platform_id platform;
extern cl_device_id device;
extern cl_context context;
extern cl_command_queue queue;
extern cl_kernel fullPipelineKernel;
extern cl_program program;
extern cl_mem mbfLUTMemA;
extern cl_mem mbfLUTMemB;
extern cl_mem inputMem;
extern cl_mem resultMem;

void launchKernel(cl_mem* input, cl_mem* output, cl_uint bufferSize, cl_uint numEventsInWaitList = 0, const cl_event* eventWaitlist = NULL, cl_event* eventOutput = NULL);

void init(const char* kernelFile, const Monotonic<7>* mbfs);
void cleanup();
