#pragma once

#include <CL/opencl.h>

struct PCoeffKernel {
	cl_device_id device;
	cl_context context;
	cl_program program;
	cl_command_queue queue;
	cl_kernel kernel;
	cl_mem mbfLUTA;
	cl_mem mbfLUTB;
	cl_mem* inputMems;
	cl_mem* resultMems;
	size_t memCount;

	PCoeffKernel() = default;
	void init(cl_device_id device, const uint64_t* mbfLUT, const char* kernelFile);
	void createBuffers(size_t memCount, size_t bufferSize);
	void resetKernels();

	~PCoeffKernel();

	PCoeffKernel(const PCoeffKernel&) = delete;
	PCoeffKernel& operator=(const PCoeffKernel&) = delete;
	PCoeffKernel(const PCoeffKernel&&) = delete;
	PCoeffKernel& operator=(const PCoeffKernel&&) = delete;

	cl_event writeBuffer(cl_mem mem, const uint32_t* buf, size_t size);
	cl_event readBuffer(cl_mem mem, uint64_t* resultBuf, size_t size, cl_uint numEventsInWaitList, const cl_event* eventWaitlist);
	cl_event launchKernel(cl_mem input, cl_mem output, cl_uint bufferSize, cl_uint numEventsInWaitList, const cl_event* eventWaitlist);
	void finish();
	void reset(); // Resets the fpga fabric
};

// Used for proper reset/initialization of the kernel. Memory blocks do not get properly reset it seems. 
void dryRunKernels(PCoeffKernel* kernels, size_t numKernels);
