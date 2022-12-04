#include "fpgaProcessor.h"

#include <chrono>
#include <iostream>

#include "../dedelib/knownData.h"
#include "../dedelib/funcTypes.h"
#include "../dedelib/aligned_alloc.h"

#include <CL/opencl.h>
#include <CL/cl_ext_intelfpga.h>
#include <chrono>

#include "AOCLUtils/aocl_utils.h"
#include "fpgaBoilerplate.h"


using namespace aocl_utils;

constexpr size_t MBF_LUT_BYTE_SIZE = mbfCounts[7]*16; // 16 Bytes per Monotonic<7>
constexpr cl_uint DRY_BUF_SIZE = 1024*1024*32; // 2^25 = ~33M

static void generateDryBuf(uint32_t* dryBuf) {
	uint32_t dryTop = 400000000; // out of mbfCounts[7] == 490013148, high top

	dryBuf[0] = dryTop | 0x80000000;
	dryBuf[1] = dryTop | 0x80000000;

	// Intersperse the bottoms
	for(cl_uint i = 2; i < DRY_BUF_SIZE; i++) {
		dryBuf[i] = i * dryTop / mbfCounts[7];
	}
}

static uint32_t* generateDryBuf() {
	uint32_t* dryBuf = aligned_mallocT<uint32_t>(DRY_BUF_SIZE, 64);
	generateDryBuf(dryBuf);
	return dryBuf;
}

constexpr size_t BUF_FILL_BLOCK_SIZE = 128;
struct BufferFillData {
	char bufFillValue[BUF_FILL_BLOCK_SIZE];
	constexpr BufferFillData() : bufFillValue{} {
		for(size_t i = 0; i < BUF_FILL_BLOCK_SIZE; i++) {
			bufFillValue[i] = '\xEE';
		}
	}
};
constexpr BufferFillData bufFillData;

void PCoeffKernelPart::createBuffers(cl_device_id device, cl_context context) {
	cl_int status;
	//this->queue = clCreateCommandQueue(context, device, CL_QUEUE_PROFILING_ENABLE, &status);
	this->queue = clCreateCommandQueue(context, device, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, &status);
	checkError(status, "Failed to create command queue");

	// Create constant mbf Look Up Table data buffer
	this->mbfLUTA = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_CHANNEL_1_INTELFPGA, MBF_LUT_BYTE_SIZE, nullptr, &status);
	checkError(status, "Failed to create the mbfLUTA buffer");
	this->mbfLUTB = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_CHANNEL_2_INTELFPGA, MBF_LUT_BYTE_SIZE, nullptr, &status);
	checkError(status, "Failed to create the mbfLUTB buffer");

	for(size_t i = 0; i < NUM_BUFFERS; i++) {
		bool even = i % 2 == 0;
		this->inputMems[i] = clCreateBuffer(context, CL_MEM_READ_ONLY | (even ? CL_CHANNEL_3_INTELFPGA : CL_CHANNEL_4_INTELFPGA), BUFFER_SIZE*sizeof(uint32_t), nullptr, &status);
		checkError(status, "Failed to create the inputMem buffer");
		this->resultMems[i] = clCreateBuffer(context, CL_MEM_WRITE_ONLY | (even ? CL_CHANNEL_4_INTELFPGA : CL_CHANNEL_3_INTELFPGA), BUFFER_SIZE*sizeof(uint64_t), nullptr, &status);
		checkError(status, "Failed to create the resultMem buffer");
	}

	for(size_t i = 0; i < NUM_BUFFERS; i++) {
		checkError(clEnqueueFillBuffer(this->queue, this->inputMems[i], bufFillData.bufFillValue, BUF_FILL_BLOCK_SIZE, 0, BUFFER_SIZE * sizeof(uint32_t), 0, nullptr, nullptr), "Failed to enqueue Fill Buffer");
	}
	for(size_t i = 0; i < NUM_BUFFERS; i++) {
		checkError(clEnqueueFillBuffer(this->queue, this->resultMems[i], bufFillData.bufFillValue, BUF_FILL_BLOCK_SIZE, 0, BUFFER_SIZE * sizeof(uint64_t), 0, nullptr, nullptr), "Failed to enqueue Fill Buffer");
	}
}
cl_event PCoeffKernelPart::launchWriteKernel(cl_kernel kernel, int memBufIdx, cl_uint bufferSize, const uint32_t* inputBuf) {
	cl_event writeFinished[1];
	checkError(clEnqueueWriteBuffer(this->queue, this->inputMems[memBufIdx], 0, 0, bufferSize*sizeof(uint32_t), inputBuf, 0, nullptr, &writeFinished[0]), "Failed to enqueue writing to buffer");
	//checkError(clEnqueueFillBuffer(this->queue, this->resultMems[memBufIdx], bufFillData.bufFillValue, BUF_FILL_BLOCK_SIZE, 0, BUFFER_SIZE * sizeof(uint64_t), 0, nullptr, &writeFinished[1]), "Failed to enqueue Fill Buffer");

	// Set the kernel arguments for kernel
	// The 0th and 1st argument, mbfLUTMemA/B is a constant buffer and remains unchanged throughout a run. 
	checkError(clSetKernelArg(kernel, 0, sizeof(cl_mem), &this->mbfLUTA), "Failed to set kernel arg 0:mbfLUTA");
	checkError(clSetKernelArg(kernel, 1, sizeof(cl_mem), &this->mbfLUTB), "Failed to set kernel arg 1:mbfLUTB");
	checkError(clSetKernelArg(kernel, 2, sizeof(cl_mem), &this->inputMems[memBufIdx]), "Failed to set kernel arg 2:inputMem");
	checkError(clSetKernelArg(kernel, 3, sizeof(cl_mem), &this->resultMems[memBufIdx]), "Failed to set kernel arg 3:resultMem");
	checkError(clSetKernelArg(kernel, 4, sizeof(cl_uint), &bufferSize), "Failed to set kernel arg 4:job size");

	// work group size, can be referred to from enqueueNDRangeKernel
	static const size_t gSize = 1;
	static const size_t lSize = 1;
	cl_event outEvent;
	checkError(clEnqueueNDRangeKernel(this->queue, kernel, 1, NULL, &gSize, &lSize, 1, writeFinished, &outEvent), "Failed to launch kernel");
	//std::cout << "Kernel launched for size " << bufferSize << std::endl;
	return outEvent;
}

cl_event PCoeffKernelPart::launchRead(int memBufIdx, cl_uint bufferSize, uint64_t* resultBuf, cl_event kernelFinished) {
	cl_event readFinished;
	checkError(clEnqueueReadBuffer(this->queue, this->resultMems[memBufIdx], 0, 0, bufferSize*sizeof(uint64_t), resultBuf, 1, &kernelFinished, &readFinished), "Failed to enqueue read buffer");
	return readFinished;
}

PCoeffMultiKernel::PCoeffMultiKernel(const char* kernelFile) {
	cl_int status;

	// Create the context.
	std::cout << "\033[31m[FPGA Processor] Creating context...\033[39m\n" << std::flush;
	this->context = clCreateContext(NULL, DEVICE_COUNT, deviceIDs, &oclContextCallback, NULL, &status);
	checkError(status, "Failed to create context");

	for(size_t deviceI = 0; deviceI < DEVICE_COUNT; deviceI++) {
		std::cout << "\033[31m[FPGA Processor] Creating buffers for device " + std::to_string(deviceI) + "...\033[39m\n" << std::flush;
		this->kernelParts[deviceI].createBuffers(deviceIDs[deviceI], context);
	}

	// Create the program.
	std::cout << "\033[31m[FPGA Processor] Initializing kernel...\nUsing kernel file " << kernelFile << "[.aocx]\033[39m\n" << std::flush;
	std::string binary_file = getBoardBinaryFile(kernelFile, deviceIDs[0]);
	std::cout << "\033[31m[FPGA Processor] Using AOCX: " + binary_file + "\033[39m\n";
	this->program = createProgramFromBinary(context, binary_file.c_str(), deviceIDs, DEVICE_COUNT);

	// Build the program that was just created.
	status = clBuildProgram(this->program, 0, NULL, "", NULL, NULL);
	checkError(status, "Failed to build program");

	// Create the kernel - name passed in here must match kernel name in the
	// original CL file, that was compiled into an AOCX file using the AOC tool
	this->kernel = clCreateKernel(this->program, "dedekindAccelerator", &status);
	checkError(status, "Failed to create dedekindAccelerator");

	doneEvent = clCreateUserEvent(this->context, &status);
	checkError(status, "Error creating finishing cl event");
}

#include "../dedelib/toString.h"
const uint64_t* initMBFLUT(const void* voidMBFs) {
	auto mbfBufPrepareStart = std::chrono::system_clock::now();
	std::cout << "\033[31m[FPGA Processor] Preparing mbfLUT buffer...\033[39m\n" << std::flush;

	const Monotonic<7>* mbfs = static_cast<const Monotonic<7>*>(voidMBFs);

	// Can't use MMAP here, memory mapped blocks don't mesh well with OpenCL buffer uploads
	//const uint64_t* mbfsUINT64 = readFlatBufferNoMMAP<uint64_t>(FileName::flatMBFsU64(7), FlatMBFStructure<7>::MBF_COUNT * 2);

	// Quick fix, apparently __m128 isn't stored as previously thought. Fix better later
	uint64_t* mbfsUINT64 = (uint64_t*) aligned_malloc(mbfCounts[7]*sizeof(Monotonic<7>), 64);
	for(size_t i = 0; i < mbfCounts[7]; i++) {
		Monotonic<7> mbf = mbfs[i];
		uint64_t upper = _mm_extract_epi64(mbf.bf.bitset.data, 1);
		uint64_t lower = _mm_extract_epi64(mbf.bf.bitset.data, 0);
		mbfsUINT64[i*2] = upper;
		mbfsUINT64[i*2+1] = lower;
	}

	double timeTaken = std::chrono::duration<double>(std::chrono::system_clock::now() - mbfBufPrepareStart).count();
	std::cout << "\033[31m[FPGA Processor] MBF LUT prepared successfully! Took " + std::to_string(timeTaken) << "s\033[39m\n" << std::flush;

	return mbfsUINT64;
}


void PCoeffMultiKernel::initBuffers(const void* mbfs) {
	const uint64_t* mbfLUT = initMBFLUT(mbfs);

	std::cout << "\033[31m[FPGA Processor] Start MBF LUT upload and initializing buffers...\033[39m\n" << std::flush;

	for(size_t deviceI = 0; deviceI < DEVICE_COUNT; deviceI++) {
		PCoeffKernelPart& d = this->kernelParts[deviceI];
		cl_event originalBufWritten;
		checkError(clEnqueueWriteBuffer(d.queue, d.mbfLUTA, false, 0, MBF_LUT_BYTE_SIZE, mbfLUT, 0, nullptr, &originalBufWritten), "Failed to enqueue write mbfLUTA");
		checkError(clEnqueueCopyBuffer(d.queue, d.mbfLUTA, d.mbfLUTB, 0, 0, MBF_LUT_BYTE_SIZE, 1, &originalBufWritten, nullptr), "Failed to enqueue copy to mbfLUTB");
	}

	uint32_t* dryBuf = generateDryBuf();

	std::cout << "\033[31m[FPGA Processor] Finished initializing buffers. Starting dryRun\033[39m\n" << std::flush;
	for(size_t deviceI = 0; deviceI < DEVICE_COUNT; deviceI++) {
		PCoeffKernelPart& d = this->kernelParts[deviceI];
		clFinish(d.queue); // Finish all uploads in this device
		d.launchWriteKernel(this->kernel, 0, DRY_BUF_SIZE, dryBuf);
	}

	aligned_free(const_cast<uint64_t*>(mbfLUT));

	for(size_t deviceI = 0; deviceI < DEVICE_COUNT; deviceI++) {
		PCoeffKernelPart& d = this->kernelParts[deviceI];
		clFinish(d.queue); // Finish all uploads in this device
	}

	clResetKernelsIntelFPGA(this->context, 2, deviceIDs);

	aligned_free(const_cast<uint32_t*>(dryBuf));

	std::cout << "\033[31m[FPGA Processor] Finished FPGA initialization\033[39m\n" << std::flush;
}

PCoeffMultiKernel::~PCoeffMultiKernel() {
	for(size_t deviceI = 0; deviceI < DEVICE_COUNT; deviceI++) {
		PCoeffKernelPart& p = this->kernelParts[deviceI];
		clReleaseMemObject(p.mbfLUTA);
		clReleaseMemObject(p.mbfLUTB);
		for(size_t i = 0; i < NUM_BUFFERS; i++) {
			clReleaseMemObject(p.inputMems[i]);
			clReleaseMemObject(p.resultMems[i]);
		}
		clReleaseCommandQueue(p.queue);
	}
	clReleaseEvent(doneEvent);
	clReleaseKernel(kernel);
	clReleaseProgram(program);
	clReleaseContext(context);
}
