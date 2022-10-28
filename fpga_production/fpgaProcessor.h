#pragma once

#include <CL/opencl.h>

constexpr uint64_t MEMORY_SIZE = 1024*1024ULL*1024*8*2; // 16GB of available memory
constexpr uint64_t BUFFER_SIZE = 214319104; // Can fit 5 buffers more than getMaxDeduplicatedBufferSize(7), aligned to 2^14 = 16384 elements
constexpr int NUM_BUFFERS = 6;
constexpr int DEVICE_COUNT = 2;

const uint64_t* initMBFLUT(const void* voidMBFs);

struct PCoeffKernelPart {
	cl_command_queue queue;
	cl_mem mbfLUTA;
	cl_mem mbfLUTB;
	cl_mem inputMems[NUM_BUFFERS];
	cl_mem resultMems[NUM_BUFFERS];
	void createBuffers(cl_device_id device, cl_context context);
	cl_event launchWriteKernel(cl_kernel kernel, int memBufIdx, cl_uint bufferSize, const uint32_t* inputBuf);
	cl_event launchRead(int memBufIdx, cl_uint bufferSize, uint64_t* resultBuf, cl_event kernelFinished);
};

struct PCoeffMultiKernel {
	cl_context context;
	cl_program program;
	cl_kernel kernel;
	cl_event doneEvent;
	PCoeffKernelPart kernelParts[DEVICE_COUNT];

	PCoeffMultiKernel(const char* kernelFile);
	void initBuffers(const void* mbfs);
	void resetKernels();

	~PCoeffMultiKernel();

	PCoeffMultiKernel(const PCoeffMultiKernel&) = delete;
	PCoeffMultiKernel& operator=(const PCoeffMultiKernel&) = delete;
	PCoeffMultiKernel(const PCoeffMultiKernel&&) = delete;
	PCoeffMultiKernel& operator=(const PCoeffMultiKernel&&) = delete;
};
