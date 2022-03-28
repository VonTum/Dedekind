// Copyright (C) 2013-2021 Altera Corporation, San Jose, California, USA. All rights reserved.
// Permission is hereby granted, free of charge, to any person obtaining a copy of this
// software and associated documentation files (the "Software"), to deal in the Software
// without restriction, including without limitation the rights to use, copy, modify, merge,
// publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to
// whom the Software is furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in all copies or
// substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
//
// This agreement shall be governed in all respects by the laws of the State of California and
// by the laws of the United States of America.

#include <iostream>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <cstring>
#include <thread>
#include <CL/opencl.h>
#include <chrono>
#include <immintrin.h>
#include <algorithm>
#include <random>
#include "AOCLUtils/aocl_utils.h"

#include "../dedelib/toString.h"
#include "../dedelib/flatPCoeffProcessing.h"
#include "../dedelib/knownData.h"
#include "../dedelib/configure.h"

using namespace aocl_utils;

constexpr size_t STRING_BUFFER_LEN = 1024;
constexpr size_t INPUT_BUFFER_COUNT = 1;
constexpr size_t RESULT_BUFFER_COUNT = 1;


// Runtime constants
static cl_int BUFSIZE = 500;
static int NUM_ITERATIONS = 1;
static bool SHOW_NONZEROS = false;
static bool SHOW_ALL = false;
static bool ENABLE_PROFILING = false;
static bool ENABLE_SHUFFLE = false;
static bool ENABLE_COMPARE = false;
static int MAX_UP_TO = 500000000;
static int START_AT = 0;

// OpenCL runtime configuration
static cl_platform_id platform = NULL;
static cl_device_id device = NULL;
static cl_context context = NULL;
static cl_command_queue queue = NULL;
static cl_kernel fullPipelineKernel = NULL;
static cl_program program = NULL;
static cl_mem mbfLUTMem = NULL;
static cl_mem inputMem[INPUT_BUFFER_COUNT]{NULL};
static cl_mem resultMem[RESULT_BUFFER_COUNT]{NULL};
static cl_int N = 1;

// work group size, can be referred to from enqueueNDRangeKernel
static const size_t gSize = 1;
static const size_t lSize = 1;

// Control whether the emulator should be used.
static bool use_emulator = false;


// Function prototypes
bool init(const char* kernelFile);
void cleanup();
static void device_info_ulong( cl_device_id device, cl_device_info param, const char* name);
static void device_info_uint( cl_device_id device, cl_device_info param, const char* name);
static void device_info_bool( cl_device_id device, cl_device_info param, const char* name);
static void device_info_string( cl_device_id device, cl_device_info param, const char* name);
static void display_device_info( cl_device_id device );


// inclusive, simimar to verilog. getBitRange(v, 11, 3) == v[11:3]
uint64_t getBitRange(uint64_t v, int highestBit, int lowestBit) {
	uint64_t shiftedRight = v >> lowestBit;
	int bitWidth = highestBit - lowestBit + 1;
	uint64_t mask = (uint64_t(1) << bitWidth) - 1;
	return shiftedRight & mask;
}


/*void summarizeResultsBuffer(uint64_t* resultsOut, cl_int bufSize) {
	int corrects = 0;
	int nonZeroCorrects = 0;
	int totals = 0;
	int nonZeroTotals = 0;

	for(cl_int i = 0; i < bufSize; i++) {
		uint64_t resultConnectCount = getBitRange(resultsOut[i], 47, 0);
		uint64_t resultNumTerms = getBitRange(resultsOut[i], 13+48-1, 48);

		if(SHOW_ALL || allData[i].connectCount != 0 || resultConnectCount != 0) {
			if(SHOW_NONZEROS) {
				std::cout << "[PCoeff] CPU: " << allData[i].connectCount << " FPGA: " << resultConnectCount << ", \t";
				std::cout << "[NumTerms] CPU: " << allData[i].numTerms << " FPGA: " << resultNumTerms << std::endl;
			}
		}

		if(!allData[i].isTop) {
			if(resultConnectCount == allData[i].connectCount && resultNumTerms == allData[i].numTerms) {
				corrects++;
				if(resultConnectCount != 0) nonZeroCorrects++;
			}
			totals++;
			if(allData[i].connectCount != 0) nonZeroTotals++;
		}
	}

	std::cout << "Total tally: " << corrects << "/" << totals << std::endl;
	std::cout << "Nonzero tally: " << nonZeroCorrects << "/" << nonZeroTotals << std::endl;
}*/

void fpgaProcessor_FullySerial(const FlatMBFStructure<7>& allMBFData, PCoeffProcessingContext& context) {
	auto mbfBufUploadStart = std::chrono::system_clock::now();
	std::cout << "Uploading mbfLUT buffer..." << std::endl;

	uint64_t* mbfsUINT64 = readFlatBuffer<uint64_t>(FileName::flatMBFs(Variables), FlatMBFStructure<Variables>::MBF_COUNT * 2);});

	// Quick fix, apparently __m128 isn't stored as previously thought. Fix better later
	/*uint64_t* mbfsUINT64 = (uint64_t*) aligned_malloc(mbfCounts[7]*sizeof(Monotonic<7>), 4096);
	for(size_t i = 0; i < mbfCounts[7]; i++) {
		Monotonic<7> mbf = allMBFData.mbfs[i];
		uint64_t upper = _mm_extract_epi64(mbf.bf.bitset.data, 1);
		uint64_t lower = _mm_extract_epi64(mbf.bf.bitset.data, 0);
		mbfsUINT64[i*2] = upper;
		mbfsUINT64[i*2+1] = lower;
	}*/

	cl_int status;
	 // This is allowed, mmap aligns buffers to the nearest page boundary. Usually something like 1024 or 4096 byte alignment > 64
	status = clEnqueueWriteBuffer(queue,mbfLUTMem,0,0,mbfCounts[7]*sizeof(cl_ulong2),mbfsUINT64,0,0,0);
	checkError(status, "Failed to enqueue writing to mbfLUTMem buffer");

	// Preset the kernel arguments to these constant buffers
	status = clSetKernelArg(fullPipelineKernel,0,sizeof(cl_mem),&mbfLUTMem);
	checkError(status, "Failed to set fullPipelineKernel mbfLUTMem");
	status = clFinish(queue);
	checkError(status, "Error while uploading mbfLUT buffer!");

	//aligned_free(mbfsUINT64);

	std::cout << "mbfLUT buffer uploaded! ";
	auto mbfLutBufUploadDone = std::chrono::system_clock::now();
	std::cout << "Took " << std::chrono::duration<double>(mbfLutBufUploadDone - mbfBufUploadStart).count() << "s" << std::endl;

	std::cout << "FPGA Processor started.\n" << std::flush;
	for(std::optional<JobInfo> jobOpt; (jobOpt = context.inputQueue.pop_wait()).has_value(); ) {
		JobInfo& job = jobOpt.value();
		cl_uint jobSize = static_cast<cl_int>(job.size());
		NodeIndex* botIndices = job.bufStart;
		std::cout << "Grabbed job " << botIndices[0] << " of size " << jobSize << std::endl;
		botIndices[0] |= 0x80000000; // Mark top
		ProcessedPCoeffSum* countConnectedSumBuf = context.outputBufferReturnQueue.pop_wait();

		for(int i = 0; i < START_AT; i++) botIndices[i+1] = botIndices[i+1+START_AT];
		jobSize -= START_AT;
		if(MAX_UP_TO < jobSize) jobSize = MAX_UP_TO;

		if(ENABLE_SHUFFLE) shuffleBots(botIndices + 1, botIndices + jobSize);
		botIndices[jobSize++] = 0x80000000; // Dummy top, to get a good occupation reading
		
		status = clEnqueueWriteBuffer(queue,inputMem[0],0,0,jobSize*sizeof(uint32_t),botIndices,0,0,0);
		checkError(status, "Failed to enqueue writing to inputMem buffer");

		status = clFinish(queue);
		checkError(status, "Failed to Finish writing inputMem buffer");

		// Set the kernel arguments for kernel
		// The 0th argument, mbfLUTMem is a constant buffer and remains unchanged throughout a run. 
		status = clSetKernelArg(fullPipelineKernel,1,sizeof(cl_mem),&inputMem[0]);
		checkError(status, "Failed to set fullPipelineKernel arg 2:inputMem");
		status = clSetKernelArg(fullPipelineKernel,2,sizeof(cl_mem),&resultMem[0]);
		checkError(status, "Failed to set fullPipelineKernel arg 3:resultMem");
		status = clSetKernelArg(fullPipelineKernel,3,sizeof(cl_uint),&jobSize);
		checkError(status, "Failed to set fullPipelineKernel arg 4:job size");

		auto kernelStart = std::chrono::system_clock::now();
		status = clEnqueueNDRangeKernel(queue, fullPipelineKernel, 1, NULL, &gSize, &lSize, 0, NULL, NULL);
		checkError(status, "Failed to launch fullPipelineKernel");
		std::cout << "Kernel launched for size " << jobSize << std::endl;

		status = clFinish(queue);
		checkError(status, "Failed to Finish kernel");
		auto kernelEnd = std::chrono::system_clock::now();
		double runtimeSeconds = std::chrono::duration<double>(kernelEnd - kernelStart).count();

		status = clEnqueueReadBuffer(queue, resultMem[0], 1, 0, jobSize*sizeof(uint64_t), countConnectedSumBuf, 0, 0, 0);
		checkError(status, "Failed to enqueue read buffer resultMem to countConnectedSumBuf");

		status = clFinish(queue);
		checkError(status, "Failed to Finish reading output buffer");

		std::cout << "Finished job " << botIndices[0] << " of size " << jobSize << " in " << runtimeSeconds << "s, at " << (double(jobSize)/runtimeSeconds/1000000.0) << "Mbots/s" << std::endl;

		// Use final dummy top to get proper occupation reading
		uint64_t analysisBot = countConnectedSumBuf[jobSize-1];
		uint64_t activityCounter = (analysisBot & 0x1FFFFFFF80000000) >> 31;
		uint64_t cycleCounter = analysisBot & 0x000000007FFFFFFF;

		std::cout << "Previous top took " << (cycleCounter<<10) << " cycles" << std::endl;
		std::cout << "Previous top had " << (activityCounter<<16) << " activity" << std::endl;
		std::cout << "Previous top occupation: " << (activityCounter << 6) / (40.0 * cycleCounter) * 100.0 << "%" << std::endl;

		if(ENABLE_COMPARE) {
			std::cout << "Starting MultiThread CPU comparison..." << std::endl;
			std::unique_ptr<uint64_t[]> countConnectedSumBufCPU = std::make_unique<uint64_t[]>(jobSize);
			ThreadPool pool;

			auto cpuStart = std::chrono::system_clock::now();
			processBetasCPU_MultiThread(allMBFData, job.bufStart, job.bufEnd, countConnectedSumBufCPU.get(), pool);
			auto cpuEnd = std::chrono::system_clock::now();
			double cpuRuntimeSeconds = std::chrono::duration<double>(cpuEnd - cpuStart).count();

			std::cout << "CPU processing completed. Took " << cpuRuntimeSeconds << "s, at " << (double(jobSize)/cpuRuntimeSeconds/1000000.0) << "Mbots/s" << std::endl;
			std::cout << "Kernel speedup of " << (cpuRuntimeSeconds / runtimeSeconds) << "x" << std::endl;

			for(size_t i = 1; i < jobSize - 1; i++) {
				if(countConnectedSumBuf[i] != countConnectedSumBufCPU[i]) {
					std::cout << "[!!!!!!!] Mistake found at position " << i << "/" << jobSize << ": ";
					std::cout << "FPGA: {" << getPCoeffSum(countConnectedSumBuf[i]) << "; " << getPCoeffCount(countConnectedSumBuf[i]) << "} ";
					std::cout << "CPU: {" << getPCoeffSum(countConnectedSumBufCPU[i]) << "; " << getPCoeffCount(countConnectedSumBufCPU[i]) << "} " << std::endl;
				}
			}
			std::cout << "All " << jobSize << " elements checked!" << std::endl;
		}

		OutputBuffer result;
		result.originalInputData = job;
		result.outputBuf = countConnectedSumBuf;
		context.outputQueue.push(result);
	}
	std::cout << "FPGA Processor finished.\n" << std::flush;
}

/*
	Parses "38431,31,13854,111,3333" to {38431,31,13854,111,3333}
*/
template<typename IntT>
std::vector<IntT> parseIntList(const std::string& strList) {
	std::stringstream strStream(strList);
	std::string segment;
	std::vector<IntT> resultingCounts;

	while(std::getline(strStream, segment, ',')) {
		resultingCounts.push_back(std::stoi(segment));
	}

	return resultingCounts;
}

// Entry point.
int main(int argc, char** argv) {
	try {
	ParsedArgs parsed(argc, argv);
	configure(parsed); // Configures memory mapping, and other settings
	const std::vector<std::string>& argsFromParsedArgs = parsed.args();
	std::vector<NodeIndex> topsToProcess{1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000};
	if(argsFromParsedArgs.size() >= 1) {
		topsToProcess = parseIntList<NodeIndex>(argsFromParsedArgs[0]);
	}

	Options options(argc, argv);

	bool reduce_data = false;

	// Optional argument to specify whether the emulator should be used.
	if(options.has("emulator")) {
		use_emulator = options.get<bool>("emulator");
	}

	if(options.has("bufsize")) {
		BUFSIZE = options.get<cl_int>("bufsize");
	}

	if(options.has("iters")) {
		NUM_ITERATIONS = options.get<cl_int>("iters");
	}

	if(options.has("MAX_UP_TO")) {
		MAX_UP_TO = options.get<cl_int>("MAX_UP_TO");
	}

	if(options.has("START_AT")) {
		START_AT = options.get<cl_int>("START_AT");
	}

	if(options.has("show")) {
		SHOW_NONZEROS = true;
	}

	if(options.has("showall")) {
		SHOW_ALL = true;
		SHOW_NONZEROS = true;
	}

	if(options.has("profile")) {
		ENABLE_PROFILING = true;
	}

	if(options.has("shuffle")) {
		ENABLE_SHUFFLE = true;
		std::cout << "Enabled shuffling" << std::endl;
	}

	if(options.has("compare")) {
		ENABLE_COMPARE = true;
		std::cout << "Enabled comparing with MultiThread CPU implementation" << std::endl;
	}

	std::string kernelFile = "fullPipelineKernel";

	if(options.has("kernelFile")) {
		kernelFile = options.get<std::string>("kernelFile");
		std::cout << "Set Kernel file to: " << kernelFile << "[.aocx]" << std::endl;
	}

	cl_int status;

	std::cout << "Loading FlatMBFStructure..." << std::endl;
	auto flatMBFLoadStart = std::chrono::system_clock::now();
	constexpr size_t Variables = 7;
	const FlatMBFStructure<Variables> allMBFData = readFlatMBFStructure<Variables>();
	std::cout << "FlatMBFStructure ready. ";
	auto flatMBFLoadDone = std::chrono::system_clock::now();
	std::cout << "Took " << std::chrono::duration<double>(flatMBFLoadDone - flatMBFLoadStart).count() << "s" << std::endl;

	std::cout << "Initializing kernel..." << std::endl;
	if(!init(kernelFile.c_str())) {
		return -1;
	}
	std::cout << "Kernel initialized successfully! ";
	auto kernelInitializedDone = std::chrono::system_clock::now();
	std::cout << "Took " << std::chrono::duration<double>(kernelInitializedDone - flatMBFLoadDone).count() << "s" << std::endl;

	std::cout << "Pipelining computation for " << topsToProcess.size() << " tops..." << std::endl;
	std::vector<BetaResult> result = pcoeffPipeline<7, 32>(allMBFData, topsToProcess, fpgaProcessor_FullySerial, 70, 10);
	std::cout << "Computation Finished!" << std::endl;

	for(const BetaResult& r : result) {
		std::cout << r.topIndex << ": " << r.betaSum.betaSum << ", " << r.betaSum.countedIntervalSizeDown << std::endl;
	}
	
	

	// Free the resources allocated
	std::cout << "Cleaning up" << std::endl;
	cleanup();
	std::cout << "Cleanup finished" << std::endl;

	}catch(const char* text) {
		std::cerr << "ERROR:" << text << std::endl;
	}
	return 0;
}

/////// HELPER FUNCTIONS ///////

bool init(const char* kernelFile) {
	cl_int status;

	if(!setCwdToExeDir()) {
		return false;
	}

	// Get the OpenCL platform.
	if (use_emulator) {
		platform = findPlatform("Intel(R) FPGA Emulation Platform for OpenCL(TM)");
	} else {
		platform = findPlatform("Intel(R) FPGA SDK for OpenCL(TM)");
	}
	if(platform == NULL) {
		printf("ERROR: Unable to find Intel(R) FPGA OpenCL platform.\n");
		return false;
	}

	// User-visible output - Platform information
	{
		char char_buffer[STRING_BUFFER_LEN];
		printf("Querying platform for info:\n");
		printf("==========================\n");
		clGetPlatformInfo(platform, CL_PLATFORM_NAME, STRING_BUFFER_LEN, char_buffer, NULL);
		printf("%-40s = %s\n", "CL_PLATFORM_NAME", char_buffer);
		clGetPlatformInfo(platform, CL_PLATFORM_VENDOR, STRING_BUFFER_LEN, char_buffer, NULL);
		printf("%-40s = %s\n", "CL_PLATFORM_VENDOR ", char_buffer);
		clGetPlatformInfo(platform, CL_PLATFORM_VERSION, STRING_BUFFER_LEN, char_buffer, NULL);
		printf("%-40s = %s\n\n", "CL_PLATFORM_VERSION ", char_buffer);
	}

	// Query the available OpenCL devices.
	scoped_array<cl_device_id> devices;
	cl_uint num_devices;

	devices.reset(getDevices(platform, CL_DEVICE_TYPE_ALL, &num_devices));

	// We'll just use the first device.
	device = devices[0];

	// Display some device information.
	display_device_info(device);

	// Create the context.
	context = clCreateContext(NULL, 1, &device, &oclContextCallback, NULL, &status);
	checkError(status, "Failed to create context");

	// Create the command queue.
	if(ENABLE_PROFILING) {
		queue = clCreateCommandQueue(context, device, CL_QUEUE_PROFILING_ENABLE, &status);
	} else {
		queue = clCreateCommandQueue(context, device, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, &status);
	}
	checkError(status, "Failed to create command queue");

	// Create the program.
	std::cout << "Using kernel file " << kernelFile << "[.aocx]" << std::endl;
	std::string binary_file = getBoardBinaryFile(kernelFile, device);
	printf("Using AOCX: %s\n", binary_file.c_str());
	program = createProgramFromBinary(context, binary_file.c_str(), &device, 1);

	// Build the program that was just created.
	status = clBuildProgram(program, 0, NULL, "", NULL, NULL);
	checkError(status, "Failed to build program");

	// Create the kernel - name passed in here must match kernel name in the
	// original CL file, that was compiled into an AOCX file using the AOC tool
	fullPipelineKernel = clCreateKernel(program, "fullPipelineKernel", &status);
	checkError(status, "Failed to create fullPipelineKernel");

	// Create constant mbf Look Up Table data buffer
	mbfLUTMem = clCreateBuffer(context, CL_MEM_READ_ONLY, mbfCounts[7]*sizeof(cl_ulong2), 0, &status);
	checkError(status, "Failed to create the mbfLUTMem buffer");

	// Create the input and output buffers
	for(size_t i = 0; i < INPUT_BUFFER_COUNT; i++) {
		inputMem[i] = clCreateBuffer(context, CL_MEM_READ_ONLY, MAX_BUFSIZE(7)*sizeof(uint32_t), 0, &status);
		checkError(status, "Failed to create the inputMem buffer");
	}
	for(size_t i = 0; i < RESULT_BUFFER_COUNT; i++) {
		resultMem[i] = clCreateBuffer(context, CL_MEM_WRITE_ONLY, MAX_BUFSIZE(7)*sizeof(uint64_t), 0, &status);
		checkError(status, "Failed to create the resultMem buffer");
	}
	return true;
}

// Free the resources allocated during initialization
void cleanup() {
	if(fullPipelineKernel) {
		clReleaseKernel(fullPipelineKernel);
	}
	if(program) {
		clReleaseProgram(program);
	}
	if(queue) {
		clReleaseCommandQueue(queue);
	}
	if(context) {
		clReleaseContext(context);
	}
	if(mbfLUTMem) {
		clReleaseMemObject(mbfLUTMem);
	}

	for(size_t i = 0; i < INPUT_BUFFER_COUNT; i++) {
		if(inputMem[i]){
			clReleaseMemObject(inputMem[i]);
		}
	}
	for(size_t i = 0; i < RESULT_BUFFER_COUNT; i++) {
		if(resultMem[i]){
			clReleaseMemObject(resultMem[i]);
		}
	}
}

// Helper functions to display parameters returned by OpenCL queries
static void device_info_ulong( cl_device_id device, cl_device_info param, const char* name) {
	cl_ulong a;
	clGetDeviceInfo(device, param, sizeof(cl_ulong), &a, NULL);
	printf("%-40s = %llu\n", name, a);
}
static void device_info_uint( cl_device_id device, cl_device_info param, const char* name) {
	cl_uint a;
	clGetDeviceInfo(device, param, sizeof(cl_uint), &a, NULL);
	printf("%-40s = %u\n", name, a);
}
static void device_info_bool( cl_device_id device, cl_device_info param, const char* name) {
	cl_bool a;
	clGetDeviceInfo(device, param, sizeof(cl_bool), &a, NULL);
	printf("%-40s = %s\n", name, (a?"true":"false"));
}
static void device_info_string( cl_device_id device, cl_device_info param, const char* name) {
	char a[STRING_BUFFER_LEN];
	clGetDeviceInfo(device, param, STRING_BUFFER_LEN, &a, NULL);
	printf("%-40s = %s\n", name, a);
}

// Query and display OpenCL information on device and runtime environment
static void display_device_info( cl_device_id device ) {

	printf("Querying device for info:\n");
	printf("========================\n");
	device_info_string(device, CL_DEVICE_NAME, "CL_DEVICE_NAME");
	device_info_string(device, CL_DEVICE_VENDOR, "CL_DEVICE_VENDOR");
	device_info_uint(device, CL_DEVICE_VENDOR_ID, "CL_DEVICE_VENDOR_ID");
	device_info_string(device, CL_DEVICE_VERSION, "CL_DEVICE_VERSION");
	device_info_string(device, CL_DRIVER_VERSION, "CL_DRIVER_VERSION");
	device_info_uint(device, CL_DEVICE_ADDRESS_BITS, "CL_DEVICE_ADDRESS_BITS");
	device_info_bool(device, CL_DEVICE_AVAILABLE, "CL_DEVICE_AVAILABLE");
	device_info_bool(device, CL_DEVICE_ENDIAN_LITTLE, "CL_DEVICE_ENDIAN_LITTLE");
	device_info_ulong(device, CL_DEVICE_GLOBAL_MEM_CACHE_SIZE, "CL_DEVICE_GLOBAL_MEM_CACHE_SIZE");
	device_info_ulong(device, CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE, "CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE");
	device_info_ulong(device, CL_DEVICE_GLOBAL_MEM_SIZE, "CL_DEVICE_GLOBAL_MEM_SIZE");
	device_info_bool(device, CL_DEVICE_IMAGE_SUPPORT, "CL_DEVICE_IMAGE_SUPPORT");
	device_info_ulong(device, CL_DEVICE_LOCAL_MEM_SIZE, "CL_DEVICE_LOCAL_MEM_SIZE");
	device_info_ulong(device, CL_DEVICE_MAX_CLOCK_FREQUENCY, "CL_DEVICE_MAX_CLOCK_FREQUENCY");
	device_info_ulong(device, CL_DEVICE_MAX_COMPUTE_UNITS, "CL_DEVICE_MAX_COMPUTE_UNITS");
	device_info_ulong(device, CL_DEVICE_MAX_CONSTANT_ARGS, "CL_DEVICE_MAX_CONSTANT_ARGS");
	device_info_ulong(device, CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE, "CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE");
	device_info_uint(device, CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS, "CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS");
	device_info_uint(device, CL_DEVICE_MEM_BASE_ADDR_ALIGN, "CL_DEVICE_MEM_BASE_ADDR_ALIGN");
	device_info_uint(device, CL_DEVICE_MIN_DATA_TYPE_ALIGN_SIZE, "CL_DEVICE_MIN_DATA_TYPE_ALIGN_SIZE");
	device_info_uint(device, CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR, "CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR");
	device_info_uint(device, CL_DEVICE_PREFERRED_VECTOR_WIDTH_SHORT, "CL_DEVICE_PREFERRED_VECTOR_WIDTH_SHORT");
	device_info_uint(device, CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT, "CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT");
	device_info_uint(device, CL_DEVICE_PREFERRED_VECTOR_WIDTH_LONG, "CL_DEVICE_PREFERRED_VECTOR_WIDTH_LONG");
	device_info_uint(device, CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT, "CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT");
	device_info_uint(device, CL_DEVICE_PREFERRED_VECTOR_WIDTH_DOUBLE, "CL_DEVICE_PREFERRED_VECTOR_WIDTH_DOUBLE");

	{
		cl_command_queue_properties ccp;
		clGetDeviceInfo(device, CL_DEVICE_QUEUE_PROPERTIES, sizeof(cl_command_queue_properties), &ccp, NULL);
		printf("%-40s = %s\n", "Command queue out of order? ", ((ccp & CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE)?"true":"false"));
		printf("%-40s = %s\n", "Command queue profiling enabled? ", ((ccp & CL_QUEUE_PROFILING_ENABLE)?"true":"false"));
	}
}

