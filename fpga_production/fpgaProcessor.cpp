#include "fpgaProcessor.h"

#include <chrono>
#include <iostream>

#include "../dedelib/knownData.h"

#include <CL/opencl.h>
#include <CL/cl_ext_intelfpga.h>
#include "AOCLUtils/aocl_utils.h"


using namespace aocl_utils;

void PCoeffKernel::init(cl_device_id device, const uint64_t* mbfLUT, const char* kernelFile) {
	cl_int status;

	// Create the context.
	this->context = clCreateContext(NULL, 1, &device, &oclContextCallback, NULL, &status);
	checkError(status, "Failed to create context");

	// Create the command queue.
	//queue = clCreateCommandQueue(context, device, CL_QUEUE_PROFILING_ENABLE, &status);
	this->queue = clCreateCommandQueue(this->context, device, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, &status);
	checkError(status, "Failed to create command queue");

	// Create constant mbf Look Up Table data buffer
	mbfLUTA = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_CHANNEL_1_INTELFPGA, mbfCounts[7]*16 /*16 bytes per MBF*/, nullptr, &status);
	checkError(status, "Failed to create the mbfLUTA buffer");
	mbfLUTB = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_CHANNEL_2_INTELFPGA, mbfCounts[7]*16 /*16 bytes per MBF*/, nullptr, &status);
	checkError(status, "Failed to create the mbfLUTB buffer");

	std::cout << "Start MBF LUT upload...\n" << std::flush;

	status = clEnqueueWriteBuffer(queue,mbfLUTA,0,0,mbfCounts[7]*16 /*16 bytes per MBF*/,mbfLUT,0,0,0);
	checkError(status, "Failed to enqueue writing to mbfLUTA buffer");
	status = clEnqueueWriteBuffer(queue,mbfLUTB,0,0,mbfCounts[7]*16 /*16 bytes per MBF*/,mbfLUT,0,0,0);
	checkError(status, "Failed to enqueue writing to mbfLUTB buffer");

	auto startKernelInitialization = std::chrono::system_clock::now();
	std::cout << "Initializing kernel..." << std::endl;

	// Create the program.
	std::cout << "Using kernel file " << kernelFile << "[.aocx]" << std::endl;
	std::string binary_file = getBoardBinaryFile(kernelFile, device);
	printf("Using AOCX: %s\n", binary_file.c_str());
	this->program = createProgramFromBinary(context, binary_file.c_str(), &device, 1);


	// Build the program that was just created.
	status = clBuildProgram(this->program, 0, NULL, "", NULL, NULL);
	checkError(status, "Failed to build program");

	// Create the kernel - name passed in here must match kernel name in the
	// original CL file, that was compiled into an AOCX file using the AOC tool
	this->kernel = clCreateKernel(this->program, "dedekindAccelerator", &status);
	checkError(status, "Failed to create dedekindAccelerator");


	// Preset the kernel arguments to these constant buffers
	status = clSetKernelArg(this->kernel,0,sizeof(cl_mem),&mbfLUTA);
	checkError(status, "Failed to set fullPipelineKernel mbfLUTA");
	status = clSetKernelArg(this->kernel,1,sizeof(cl_mem),&mbfLUTB);
	checkError(status, "Failed to set fullPipelineKernel mbfLUTB");

	std::cout << "Kernel initialized successfully! ";
	auto kernelInitializedDone = std::chrono::system_clock::now();
	std::cout << "Took " << std::chrono::duration<double>(kernelInitializedDone - startKernelInitialization).count() << "s\n";
}

void PCoeffKernel::createBuffers(cl_mem* inputMems, cl_mem* outputMems, size_t memCount, size_t bufferSize) {
	cl_int status;
	// Create the input and output buffers
	for(size_t i = 0; i < memCount; i++) {
		bool even = i % 2 == 0;
		inputMems[i] = clCreateBuffer(this->context, CL_MEM_READ_ONLY | (even ? CL_CHANNEL_3_INTELFPGA : CL_CHANNEL_4_INTELFPGA), bufferSize*sizeof(uint32_t), nullptr, &status);
		checkError(status, "Failed to create the inputMem buffer");
	}
	for(size_t i = 0; i < memCount; i++) {
		bool odd = i % 2 == 1;
		outputMems[i] = clCreateBuffer(this->context, CL_MEM_WRITE_ONLY | (odd ? CL_CHANNEL_3_INTELFPGA : CL_CHANNEL_4_INTELFPGA), bufferSize*sizeof(uint64_t), nullptr, &status);
		checkError(status, "Failed to create the resultMem buffer");
	}
}

PCoeffKernel::~PCoeffKernel() {
	clReleaseMemObject(mbfLUTA);
	clReleaseMemObject(mbfLUTB);
	clReleaseKernel(kernel);
	clReleaseProgram(program);
	clReleaseCommandQueue(queue);
	clReleaseContext(context);
}

cl_event PCoeffKernel::writeBuffer(cl_mem mem, const uint32_t* buf, size_t size) {
	cl_event writeFinished;
	cl_int status = clEnqueueWriteBuffer(queue,mem,0,0,size*sizeof(uint32_t),buf,0,0,&writeFinished);
	checkError(status, "Failed to enqueue writing to buffer");
	return writeFinished;
}
cl_event PCoeffKernel::readBuffer(cl_mem mem, uint64_t* resultBuf, size_t size, cl_uint numEventsInWaitList, const cl_event* eventWaitlist) {
	cl_event readFinished;
	cl_int status = clEnqueueReadBuffer(queue, mem, 0, 0, size*sizeof(uint64_t), resultBuf, numEventsInWaitList, eventWaitlist, &readFinished);
	checkError(status, "Failed to enqueue read buffer");
	return readFinished;
}
cl_event PCoeffKernel::launchKernel(cl_mem input, cl_mem output, cl_uint bufferSize, cl_uint numEventsInWaitList, const cl_event* eventWaitlist) {
	// Set the kernel arguments for kernel
	// The 0th and 1st argument, mbfLUTMemA/B is a constant buffer and remains unchanged throughout a run. 
	cl_int status = clSetKernelArg(this->kernel,2,sizeof(cl_mem),&input);
	checkError(status, "Failed to set kernel arg 2:inputMem");
	status = clSetKernelArg(this->kernel,3,sizeof(cl_mem),&output);
	checkError(status, "Failed to set kernel arg 3:resultMem");
	status = clSetKernelArg(this->kernel,4,sizeof(cl_uint),&bufferSize);
	checkError(status, "Failed to set kernel arg 4:job size");

	// work group size, can be referred to from enqueueNDRangeKernel
	static const size_t gSize = 1;
	static const size_t lSize = 1;
	cl_event outEvent;
	status = clEnqueueNDRangeKernel(this->queue, this->kernel, 1, NULL, &gSize, &lSize, numEventsInWaitList, eventWaitlist, &outEvent);
	checkError(status, "Failed to launch kernel");
	std::cout << "Kernel launched for size " << bufferSize << std::endl;
	return outEvent;
}
void PCoeffKernel::finish() {
	clFinish(this->queue);
}
