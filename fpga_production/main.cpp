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
#include <fstream>
#include <optional>
#include <mutex>
#include "AOCLUtils/aocl_utils.h"

#include "../dedelib/toString.h"
#include "../dedelib/flatPCoeffProcessing.h"
#include "../dedelib/knownData.h"
#include "../dedelib/configure.h"
#include "../dedelib/pcoeffValidator.h"
#include "../dedelib/latch.h"
#include "../dedelib/supercomputerJobs.h"
#include "../dedelib/timeTracker.h"
#include "../dedelib/pcoeffValidator.h"

#include "fpgaBoilerplate.h"
#include "fpgaProcessor.h"

using namespace aocl_utils;

template<typename T>
void printBufferSample(cl_command_queue queue, cl_mem mem, size_t bufSize, std::string name) {
	constexpr size_t SAMPLE_SIZE = 32;
	constexpr size_t SAMPLE_COUNT = 5;
	T sample[SAMPLE_SIZE];

	std::string totalStr = name + " > \n";
	for(size_t chunkI = 0; chunkI < SAMPLE_COUNT; chunkI++) {
		size_t offset = (bufSize - SAMPLE_SIZE) * chunkI / (SAMPLE_COUNT - 1);
		clEnqueueReadBuffer(queue, mem, true, offset * sizeof(T), SAMPLE_SIZE * sizeof(T), sample, 0, nullptr, nullptr);

		totalStr.append("    " + std::to_string(offset) + ": ");
		for(size_t i = 0; i < SAMPLE_SIZE; i++) {
			totalStr.append(std::to_string(sample[i]) + ",");
		}
		totalStr[totalStr.size() - 1] = '\n';
	}
	std::cout << totalStr;
	std::cout << std::flush;
}




struct FPGAGlobalData;
struct FPGASlot {
	OutputBuffer totalJob;
	FPGAGlobalData* globalData;
};

struct FPGAGlobalData {
	PCoeffMultiKernel multiKernel;
	PCoeffProcessingContext* context;
	int activeSlots;
	int processedCounts[DEVICE_COUNT];
	std::default_random_engine randomGenerator;

	FPGASlot slots[DEVICE_COUNT * NUM_BUFFERS];

	FPGAGlobalData(PCoeffProcessingContext& context) : 
		multiKernel("dedekindAccelerator"),
		context(&context),
		activeSlots(DEVICE_COUNT * NUM_BUFFERS),
		processedCounts{} {
		for(FPGASlot& slot : slots) {
			slot.globalData = this;
		}
		for(size_t i = 0; i < DEVICE_COUNT; i++) {
			processedCounts[i] = 0;
		}
	}
};

static void pushJob(FPGAGlobalData& data, int deviceI, int slotI) {
	std::optional<JobInfo> jobOpt = data.context->inputQueue.pop_wait_prefer_random(deviceI, data.randomGenerator);

	if(!jobOpt.has_value()) {
		data.activeSlots--;
		if(data.activeSlots == 0) {
			checkError(clSetUserEventStatus(data.multiKernel.doneEvent, CL_COMPLETE), "Error setting final event status!");
		}
		return;
	}

	FPGASlot& slot = data.slots[deviceI * NUM_BUFFERS + slotI];
	slot.totalJob.originalInputData = jobOpt.value();
	std::cout << "FPGA " + std::to_string(deviceI) + ":" + std::to_string(slotI) + ": top " + std::to_string(slot.totalJob.originalInputData.getTop()) + " with " + std::to_string(slot.totalJob.originalInputData.getNumberOfBottoms() / 1000000.0) + "M bots\n" << std::flush;

	size_t thisCount = data.processedCounts[deviceI]++;
	if(thisCount % 50 == 0) {
		int temperature = getDeviceTemperature(deviceIDs[deviceI]);
		std::cout << "\033[31m[FPGA Processor " + std::to_string(deviceI) + "] Processed " + std::to_string(thisCount) + " jobs, device temperature: " + std::to_string(temperature) + "°C.\033[39m\n" << std::flush;
	}

	cl_uint bufSize = static_cast<cl_uint>(slot.totalJob.originalInputData.bufferSize());
	PCoeffKernelPart& subKernel = data.multiKernel.kernelParts[deviceI];
	NodeIndex* jobBuf = slot.totalJob.originalInputData.bufStart;

	cl_event kernelFinished = subKernel.launchWriteKernel(data.multiKernel.kernel, slotI, bufSize, jobBuf);

	PCoeffProcessingContextEighth& jobQueue = data.context->getNUMAForBuf(jobBuf);
	slot.totalJob.outputBuf = jobQueue.resultBufferAlloc.pop_wait().value();
	cl_event readFinished = subKernel.launchRead(slotI, bufSize, slot.totalJob.outputBuf, kernelFinished);

	checkError(clSetEventCallback(readFinished, CL_COMPLETE, [](cl_event, cl_int, void* voidSlot) {
		FPGASlot* slot = (FPGASlot*) voidSlot;

		PCoeffProcessingContextEighth& jobQueue = slot->globalData->context->getNUMAForBuf(slot->totalJob.originalInputData.bufStart);

		int slotID = slot - slot->globalData->slots;
		int deviceI = slotID / NUM_BUFFERS;
		int slotI = slotID % NUM_BUFFERS;

		//size_t bufSize = slot->totalJob.originalInputData.bufferSize();
		//std::cout << "\033[31m[FPGA Processor " + std::to_string(deviceI) + "] Finished slot " + std::to_string(slotI) + "\033[39m\n" << std::flush;

		//PCoeffKernelPart& kPart = slot->globalData->multiKernel.kernelParts[deviceI];
		//printBufferSample<uint32_t>(kPart.queue, kPart.inputMems[slotI], bufSize, "[" + std::to_string(deviceI) + "].inputMems[" + std::to_string(slotI) + "]");
		//printBufferSample<uint64_t>(kPart.queue, kPart.resultMems[slotI], bufSize, "[" + std::to_string(deviceI) + "].resultMems[" + std::to_string(slotI) + "]");

		jobQueue.outputQueue.push(std::move(slot->totalJob));

		pushJob(*slot->globalData, deviceI, slotI); // Push new job into freed slot
	}, &slot), "Error setting event callback");
}

void fpgaProcessor_Throughput_SingleThread(PCoeffProcessingContext& context) {
	std::cout << "\033[31m[FPGA Processor] Initializing platform...\033[39m\n" << std::flush;
	initPlatform();
	std::cout << "\033[31m[FPGA Processor] Creating PCoeffMultiKernel for " + std::to_string(numDevices) + " devices...\033[39m\n" << std::flush;
	FPGAGlobalData globalData(context);
	std::cout << "\033[31m[FPGA Processor] PCoeffMultiKernel created, waiting for MBF LUT...\033[39m\n" << std::flush;
	context.mbfs0Ready.wait();
	std::cout << "\033[31m[FPGA Processor] MBF LUT acquired, initializing and dry-running...\033[39m\n" << std::flush;
	globalData.multiKernel.initBuffers(context.mbfs[0]);

	/*printBufferSample<uint64_t>(globalData.multiKernel.kernelParts[0].queue, globalData.multiKernel.kernelParts[0].mbfLUTA, mbfCounts[7], "[0].mbfLUTA");
	printBufferSample<uint64_t>(globalData.multiKernel.kernelParts[0].queue, globalData.multiKernel.kernelParts[0].mbfLUTB, mbfCounts[7], "[0].mbfLUTB");
	printBufferSample<uint64_t>(globalData.multiKernel.kernelParts[1].queue, globalData.multiKernel.kernelParts[1].mbfLUTA, mbfCounts[7], "[1].mbfLUTA");
	printBufferSample<uint64_t>(globalData.multiKernel.kernelParts[1].queue, globalData.multiKernel.kernelParts[1].mbfLUTB, mbfCounts[7], "[1].mbfLUTB");*/

	std::cout << "\033[31m[FPGA Processor] FPGA processor started, waiting for initial jobs\033[39m\n" << std::flush;
	for(int di = 0; di < DEVICE_COUNT; di++) {
		for(int si = 0; si < NUM_BUFFERS; si++) {
			pushJob(globalData, di, si);
		}
	}
	std::cout << "\033[31m[FPGA Processor] Initial jobs submitted.\033[39m\n" << std::flush;
	checkError(clWaitForEvents(1, &globalData.multiKernel.doneEvent), "Error attempting to wait for events");
	std::cout << "\033[31m[FPGA Processor] Exit.\033[39m\n" << std::flush;
}

// Supercomputing entry point.
int main(int argc, char** argv) {
	TimeTracker timer("FPGA computation ");
	if(argc != 3) {
		std::cerr << "Usage: " << argv[0] << " <computeFolder> <jobID>\n" << std::flush;
		std::abort();
	}
	std::string computeFolder(argv[1]);
	std::string jobID(argv[2]);

	//processJob(7, computeFolder, jobID, "fpga", fpgaProcessor_Throughput, continuousValidatorPThread<7>);
	processJob(7, computeFolder, jobID, "fpga", fpgaProcessor_Throughput_SingleThread, continuousValidatorPThread<7>);

	/*PCoeffProcessingContext emptyContext(7);
	emptyContext.initMBFS();
	fpgaProcessor_Throughput_SingleThread(emptyContext);*/

	return 0;
}
