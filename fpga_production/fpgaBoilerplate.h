#pragma once

#include <CL/opencl.h>

// OpenCL runtime configuration
extern cl_platform_id platform;
extern cl_uint numDevices;
extern cl_device_id* deviceIDs;

void launchKernel(cl_mem* input, cl_mem* output, cl_uint bufferSize, cl_uint numEventsInWaitList = 0, const cl_event* eventWaitlist = NULL, cl_event* eventOutput = NULL);

void initPlatform();
void initCRCExceptionCallback();
void cleanup();
cl_int getDeviceTemperature(cl_device_id device);
