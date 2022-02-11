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
#include "AOCLUtils/aocl_utils.h"

#include "../dedelib/toString.h"

#include "dataFormat.h"

using namespace aocl_utils;

#define STRING_BUFFER_LEN 1024


// Runtime constants
static cl_int BUFSIZE = 500;
static int NUM_ITERATIONS = 1;
static bool SHOW_NONZEROS = false;
static bool SHOW_ALL = false;
static bool ENABLE_PROFILING = false;

// OpenCL runtime configuration
static cl_platform_id platform = NULL;
static cl_device_id device = NULL;
static cl_context context = NULL;
static cl_command_queue queue = NULL;
static cl_kernel fullPipelineKernel = NULL;
static cl_program program = NULL;
static cl_mem lowerBotsMem = NULL;
static cl_mem upperBotsMem = NULL;
#define RESULTS_BUFFER_COUNT 1
static cl_mem resultsMem[RESULTS_BUFFER_COUNT]{NULL};
static cl_int N = 1;

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


void summarizeResultsBuffer(uint64_t* resultsOut, cl_int bufSize) {
  int corrects = 0;
  int nonZeroCorrects = 0;
  int totals = 0;
  int nonZeroTotals = 0;

  for(size_t i = 0; i < bufSize; i++) {
    uint64_t resultConnectCount = getBitRange(resultsOut[i], 47, 0);
    uint64_t resultNumTerms = getBitRange(resultsOut[i], 12+48-1, 48);

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
}




// Entry point.
int main(int argc, char** argv) {
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

  cl_int status;

  std::cout << "Loading data..." << std::endl;
  uint64_t* upperBotsIn = (uint64_t*) aocl_utils::alignedMalloc(BUFSIZE*sizeof(uint64_t));
  uint64_t* lowerBotsIn = (uint64_t*) aocl_utils::alignedMalloc(BUFSIZE*sizeof(uint64_t));
  uint64_t* resultsOut = (uint64_t*) aocl_utils::alignedMalloc(BUFSIZE*sizeof(uint64_t));

  for(size_t i = 0; i < BUFSIZE; i++) {
    upperBotsIn[i] = allData[i].botOrTopUpper;
    lowerBotsIn[i] = allData[i].botOrTopLower;

    //std::cout << upperBotsIn[i] << " ";
    //std::cout << lowerBotsIn[i] << std::endl;
  }

  std::string kernelFile = "fullPipelineKernel";

   if(options.has("kernelFile")) {
     kernelFile = options.get<std::string>("kernelFile");
     std::cout << "Set Kernel file to: " << kernelFile << "[.aocx]" << std::endl;
   }

  if(!init(kernelFile.c_str())) {
    return -1;
  }

  status = clEnqueueWriteBuffer(queue,upperBotsMem,0,0,BUFSIZE*sizeof(uint64_t),upperBotsIn,0,0,0);
  checkError(status, "Failed to enqueue writing to upperBotsMem buffer");
  status = clEnqueueWriteBuffer(queue,lowerBotsMem,0,0,BUFSIZE*sizeof(uint64_t),lowerBotsIn,0,0,0);
  checkError(status, "Failed to enqueue writing to lowerBotsMem buffer");

  status = clFinish(queue);
  checkError(status, "Failed to Finish writing buffers");
  
  // Set the kernel arguments for kernel
  status = clSetKernelArg(fullPipelineKernel,0,sizeof(cl_mem),&upperBotsMem);
  checkError(status, "Failed to set fullPipelineKernel arg 0");
  status = clSetKernelArg(fullPipelineKernel,1,sizeof(cl_mem),&lowerBotsMem);
  checkError(status, "Failed to set fullPipelineKernel arg 1");
  status = clSetKernelArg(fullPipelineKernel,3,sizeof(cl_int),&BUFSIZE);
  checkError(status, "Failed to set fullPipelineKernel arg 3");
  
  // Configure work set over which the kernel will execute
  size_t gSize = 1;
  size_t lSize = 1; 

  // Launch the kernels
  printf("Enqueueing fullPipelineKernel %d times with global size %d\n", NUM_ITERATIONS, (int) gSize);

  // Launch fullPipelineKernel
  double startTime = aocl_utils::getCurrentTimestamp();
  
  for(size_t i = 0; i < NUM_ITERATIONS; i++) {
    for(size_t j = 0; j < RESULTS_BUFFER_COUNT; j++) {
      status = clSetKernelArg(fullPipelineKernel,2,sizeof(cl_mem),&resultsMem[j]);
      checkError(status, "Failed to set fullPipelineKernel arg 2");
      status = clEnqueueNDRangeKernel(queue, fullPipelineKernel, 1, NULL, &gSize, &lSize, 0, NULL, NULL);
      checkError(status, "Failed to launch fullPipelineKernel");
      std::cout << "Kernel launched!" << std::endl;
    }
  }

  status = clFinish(queue);
  checkError(status, "Failed to finish");
  //std::this_thread::sleep_for(std::chrono::milliseconds(4000));

  double stopTime = aocl_utils::getCurrentTimestamp();
  printf ("Kernel computation took %g seconds\n", stopTime - startTime);

  // Reading results to buffer
  printf("Reading results to buffers...\n");
  for(size_t i = 0; i < RESULTS_BUFFER_COUNT; i++) {
    status = clEnqueueReadBuffer(queue, resultsMem[i], 1, 0, BUFSIZE*sizeof(uint64_t), resultsOut, 0, 0, 0);
    checkError(status, "Failed to enqueue read buffer resultsMem to resultsOut");
    status = clFinish(queue);
    checkError(status, "Failed to read buffer resultsMem to resultsOut");
    
    summarizeResultsBuffer(resultsOut, BUFSIZE);
  }

  // Free the resources allocated
  cleanup();
  aocl_utils::alignedFree(lowerBotsIn);
  aocl_utils::alignedFree(upperBotsIn);
  aocl_utils::alignedFree(resultsOut);


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

  // Create the input and output buffers
  lowerBotsMem = clCreateBuffer(context, CL_MEM_READ_ONLY, BUFSIZE*sizeof(uint64_t), 0, &status);
  checkError(status, "Failed to create the lowerBotsMem buffer");
  upperBotsMem = clCreateBuffer(context, CL_MEM_READ_ONLY, BUFSIZE*sizeof(uint64_t), 0, &status);
  checkError(status, "Failed to create the upperBotsMem buffer");
  for(size_t i = 0; i < RESULTS_BUFFER_COUNT; i++) {
    resultsMem[i] = clCreateBuffer(context, CL_MEM_WRITE_ONLY, BUFSIZE*sizeof(uint64_t), 0, &status);
    checkError(status, "Failed to create the resultsMem buffer");
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
  if(lowerBotsMem){
    clReleaseMemObject(lowerBotsMem);
  }
  if(upperBotsMem){
    clReleaseMemObject(upperBotsMem);
  }
  for(size_t i = 0; i < RESULTS_BUFFER_COUNT; i++) {
    if(resultsMem[i]){
      clReleaseMemObject(resultsMem[i]);
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

