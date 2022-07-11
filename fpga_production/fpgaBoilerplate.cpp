#include "fpgaBoilerplate.h"

#include <CL/cl_ext_intelfpga.h>
#include <stdio.h>
#include <chrono>
#include <iostream>
#include "AOCLUtils/aocl_utils.h"

#include "../dedelib/aligned_alloc.h"
#include "../dedelib/flatBufferManagement.h"

#include "../dedelib/flatPCoeff.h"

using namespace aocl_utils;


constexpr size_t STRING_BUFFER_LEN = 1024;

// OpenCL runtime configuration
cl_platform_id platform = NULL;
cl_device_id device = NULL;
cl_context context = NULL;
cl_command_queue queue = NULL;
cl_kernel fullPipelineKernel = NULL;
cl_program program = NULL;
cl_mem mbfLUTMemA = NULL;
cl_mem mbfLUTMemB = NULL;
cl_mem inputMem[INPUT_BUFFER_COUNT]{NULL};
cl_mem resultMem[RESULT_BUFFER_COUNT]{NULL};



// Function prototypes
static void device_info_ulong( cl_device_id device, cl_device_info param, const char* name);
static void device_info_uint( cl_device_id device, cl_device_info param, const char* name);
static void device_info_bool( cl_device_id device, cl_device_info param, const char* name);
static void device_info_string( cl_device_id device, cl_device_info param, const char* name);
static void display_device_info( cl_device_id device );





/////// HELPER FUNCTIONS ///////
static void initPlatform() {
	if(!setCwdToExeDir()) {
		std::cerr << "ERROR: setCwdToExeDir\n" << std::flush;
		exit(-1);
	}

	// Get the OpenCL platform.
	platform = findPlatform("Intel(R) FPGA SDK for OpenCL(TM)");
	
	if(platform == NULL) {
		printf("ERROR: Unable to find Intel(R) FPGA OpenCL platform.\n");
		exit(-1);
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

	// TODO ECC detection, VERY NICE
	//clSetDeviceExceptionCallbackIntelFPGA(num_devices, devices.get(), )

	// We'll just use the first device.
	device = devices[0];

	// Display some device information.
	display_device_info(device);
}

void initMBFLUT(const Monotonic<7>* mbfs) {
	auto mbfBufPrepareStart = std::chrono::system_clock::now();
	std::cout << "Preparing mbfLUT buffer..." << std::endl;

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

	std::cout << "MBF LUT prepared successfully! ";
	auto mbfPrepareEnd = std::chrono::system_clock::now();
	std::cout << "Took " << std::chrono::duration<double>(mbfPrepareEnd - mbfBufPrepareStart).count() << "s\n";

	std::cout << "Creating FPGA buffers...\n" << std::flush;

	cl_int status;
	// Create constant mbf Look Up Table data buffer
	mbfLUTMemA = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_CHANNEL_3_INTELFPGA, mbfCounts[7]*16 /*16 bytes per MBF*/, nullptr, &status);
	checkError(status, "Failed to create the mbfLUTMemA buffer");
	mbfLUTMemB = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_CHANNEL_4_INTELFPGA, mbfCounts[7]*16 /*16 bytes per MBF*/, nullptr, &status);
	checkError(status, "Failed to create the mbfLUTMemB buffer");

	// Create the input and output buffers
	for(size_t i = 0; i < INPUT_BUFFER_COUNT; i++) {
		inputMem[i] = clCreateBuffer(context, CL_MEM_READ_ONLY, MAX_BUFSIZE(7)*sizeof(uint32_t), nullptr, &status);
		checkError(status, "Failed to create the inputMem buffer");
	}
	for(size_t i = 0; i < RESULT_BUFFER_COUNT; i++) {
		resultMem[i] = clCreateBuffer(context, CL_MEM_WRITE_ONLY, MAX_BUFSIZE(7)*sizeof(uint64_t), nullptr, &status);
		checkError(status, "Failed to create the resultMem buffer");
	}

	std::cout << "Uploading MBF LUT...\n" << std::flush;

	status = clEnqueueWriteBuffer(queue,mbfLUTMemA,0,0,mbfCounts[7]*16 /*16 bytes per MBF*/,mbfsUINT64,0,0,0);
	checkError(status, "Failed to enqueue writing to mbfLUTMemA buffer");
	status = clEnqueueWriteBuffer(queue,mbfLUTMemB,0,0,mbfCounts[7]*16 /*16 bytes per MBF*/,mbfsUINT64,0,0,0);
	checkError(status, "Failed to enqueue writing to mbfLUTMemB buffer");

	status = clFinish(queue);
	checkError(status, "Error while uploading mbfLUT buffer!");

	aligned_free(mbfsUINT64);

	std::cout << "MBF LUT uploaded successfully! ";
	auto mbfUploadDone = std::chrono::system_clock::now();
	std::cout << "Took " << std::chrono::duration<double>(mbfUploadDone - mbfPrepareEnd).count() << "s\n";
}

void initContext() {
	std::cout << "Initializing context..." << std::endl;
	cl_int status;

	// Create the context.
	context = clCreateContext(NULL, 1, &device, &oclContextCallback, NULL, &status);
	checkError(status, "Failed to create context");

	// Create the command queue.
	//queue = clCreateCommandQueue(context, device, CL_QUEUE_PROFILING_ENABLE, &status);
	queue = clCreateCommandQueue(context, device, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, &status);
	checkError(status, "Failed to create command queue");
}

void initKernel(const char* kernelFile) {
	auto startKernelInitialization = std::chrono::system_clock::now();
	std::cout << "Initializing kernel..." << std::endl;

	// Create the program.
	std::cout << "Using kernel file " << kernelFile << "[.aocx]" << std::endl;
	std::string binary_file = getBoardBinaryFile(kernelFile, device);
	printf("Using AOCX: %s\n", binary_file.c_str());
	program = createProgramFromBinary(context, binary_file.c_str(), &device, 1);

	cl_int status;
	// Build the program that was just created.
	status = clBuildProgram(program, 0, NULL, "", NULL, NULL);
	checkError(status, "Failed to build program");

	// Create the kernel - name passed in here must match kernel name in the
	// original CL file, that was compiled into an AOCX file using the AOC tool
	fullPipelineKernel = clCreateKernel(program, "dedekindAccelerator", &status);
	checkError(status, "Failed to create dedekindAccelerator");


	// Preset the kernel arguments to these constant buffers
	status = clSetKernelArg(fullPipelineKernel,0,sizeof(cl_mem),&mbfLUTMemA);
	checkError(status, "Failed to set fullPipelineKernel mbfLUTMemA");
	status = clSetKernelArg(fullPipelineKernel,1,sizeof(cl_mem),&mbfLUTMemB);
	checkError(status, "Failed to set fullPipelineKernel mbfLUTMemB");

	std::cout << "Kernel initialized successfully! ";
	auto kernelInitializedDone = std::chrono::system_clock::now();
	std::cout << "Took " << std::chrono::duration<double>(kernelInitializedDone - startKernelInitialization).count() << "s\n";
}


static void cleanupBuffers() {
	if(mbfLUTMemA) {
		clReleaseMemObject(mbfLUTMemA);
	}
	if(mbfLUTMemB) {
		clReleaseMemObject(mbfLUTMemB);
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

static void cleanupKernel() {
	if(fullPipelineKernel) {
		clReleaseKernel(fullPipelineKernel);
	}
	if(program) {
		clReleaseProgram(program);
	}
}

static void cleanupContext() {
	if(queue) {
		clReleaseCommandQueue(queue);
	}
	if(context) {
		clReleaseContext(context);
	}
}

// Free the resources allocated during initialization
void cleanup() {
	cleanupBuffers();
	cleanupKernel();
	cleanupContext();
}

// work group size, can be referred to from enqueueNDRangeKernel
static const size_t gSize = 1;
static const size_t lSize = 1;

void launchKernel(cl_mem* input, cl_mem* output, cl_uint bufferSize) {
	// Set the kernel arguments for kernel
	// The 0th and 1st argument, mbfLUTMemA/B is a constant buffer and remains unchanged throughout a run. 
	cl_int status = clSetKernelArg(fullPipelineKernel,2,sizeof(cl_mem),input);
	checkError(status, "Failed to set fullPipelineKernel arg 2:inputMem");
	status = clSetKernelArg(fullPipelineKernel,3,sizeof(cl_mem),output);
	checkError(status, "Failed to set fullPipelineKernel arg 3:resultMem");
	status = clSetKernelArg(fullPipelineKernel,4,sizeof(cl_uint),&bufferSize);
	checkError(status, "Failed to set fullPipelineKernel arg 4:job size");

	status = clEnqueueNDRangeKernel(queue, fullPipelineKernel, 1, NULL, &gSize, &lSize, 0, NULL, NULL);
	checkError(status, "Failed to launch fullPipelineKernel");
	std::cout << "Kernel launched for size " << bufferSize << std::endl;
}

void init(const char* kernelFile, const Monotonic<7>* mbfs) {
	initPlatform();
	initContext();
	initMBFLUT(mbfs);
	initKernel(kernelFile);

	// Double initialize
	constexpr int TESTBUF_SIZE = 1 << 15; // ~32000 bottoms
	uint32_t* testBuf = aligned_mallocT<uint32_t>(TESTBUF_SIZE, 64);
	testBuf[0] = (TESTBUF_SIZE - 2) | 0x80000000;
	testBuf[1] = testBuf[0];
	for(int i = 0; i < TESTBUF_SIZE - 2; i++) {
		testBuf[i+2] = i;
	}


	cl_int status = clEnqueueWriteBuffer(queue,inputMem[0],0,0,TESTBUF_SIZE*sizeof(uint32_t),testBuf,0,0,0);
	checkError(status, "Failed to enqueue writing to testBuf buffer");

	status = clFinish(queue);
	checkError(status, "Failed to Finish writing testBuf buffer");

	launchKernel(&inputMem[0], &resultMem[0], TESTBUF_SIZE);

	aligned_free(testBuf);

	status = clFinish(queue);
	checkError(status, "Failed to Finish the test kernel");

	cleanupKernel();
	initKernel(kernelFile);
}


// Helper functions to display parameters returned by OpenCL queries
static void device_info_ulong( cl_device_id device, cl_device_info param, const char* name) {
	cl_ulong a;
	clGetDeviceInfo(device, param, sizeof(cl_ulong), &a, NULL);
	printf("%-40s = %lu\n", name, a);
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

	device_info_uint(device, CL_DEVICE_CORE_TEMPERATURE_INTELFPGA, "CL_DEVICE_CORE_TEMPERATURE_INTELFPGA");

	{
		cl_command_queue_properties ccp;
		clGetDeviceInfo(device, CL_DEVICE_QUEUE_PROPERTIES, sizeof(cl_command_queue_properties), &ccp, NULL);
		printf("%-40s = %s\n", "Command queue out of order? ", ((ccp & CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE)?"true":"false"));
		printf("%-40s = %s\n", "Command queue profiling enabled? ", ((ccp & CL_QUEUE_PROFILING_ENABLE)?"true":"false"));
	}
}



