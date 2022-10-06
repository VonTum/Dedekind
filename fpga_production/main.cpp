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
#include "AOCLUtils/aocl_utils.h"

#include "../dedelib/toString.h"
#include "../dedelib/flatPCoeffProcessing.h"
#include "../dedelib/knownData.h"
#include "../dedelib/configure.h"
#include "../dedelib/pcoeffValidator.h"

#include "fpgaBoilerplate.h"
#include "fpgaProcessor.h"

using namespace aocl_utils;


// Runtime constants
static bool SHOW_NONZEROS = false;
static bool SHOW_ALL = false;
static bool ENABLE_SHUFFLE = false;
static bool ENABLE_COMPARE = false;
static bool ENABLE_STATISTICS = false;
static bool ONLY_KERNEL = false; // For debugging, only initializes the FPGA kernel
static cl_uint SHOW_FRONT = 0;
static cl_uint SHOW_TAIL = 0;
static bool USE_VALIDATOR = false;
static double ACTIVITY_MULTIPLIER = 10*60.0;
static bool THROUGHPUT_MODE = false;

static std::string kernelFile = "dedekindAccelerator";


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


struct FPGAData {
	ProcessedPCoeffSum* outBuf;
	PCoeffProcessingContext* queues;
	PCoeffKernel* kernel;
	cl_mem inputMem;
	cl_mem resultMem;
	JobInfo job;
	cl_event* done;
	size_t* curBufIdx;
};

void pushJobIntoKernel(FPGAData* data) {
	std::optional<JobInfo> jobOpt = data->queues->inputQueue.pop_wait(*(data->curBufIdx));
	if(!jobOpt.has_value()) {
		clSetUserEventStatus(*data->done, CL_COMPLETE);
		return;
	}
	data->job = std::move(jobOpt).value();
	JobInfo& job = data->job;
	cl_uint bufferSize = static_cast<cl_uint>(job.bufferSize());
	size_t numberOfBottoms = job.getNumberOfBottoms();
	NodeIndex topIdx = job.getTop();
	std::cout << "Grabbed job " + std::to_string(topIdx) + " with " + std::to_string(numberOfBottoms) + " bottoms\n" << std::flush;

	constexpr cl_uint JOB_SIZE_ALIGNMENT = 16*32; // Block Size 32, shuffle size 16
	assert(bufferSize % JOB_SIZE_ALIGNMENT == 0);
	
	//std::cout << "Resulting buffer size: " + std::to_string(bufferSize) + "\n" << std::flush;

	cl_event writeFinished = data->kernel->writeBuffer(data->inputMem, job.bufStart, bufferSize);
	cl_event kernelFinished = data->kernel->launchKernel(data->inputMem, data->resultMem, bufferSize, 1, &writeFinished);

	PCoeffProcessingContextEighth& numaQueue = data->queues->getNUMAForBuf(job.bufStart);
	//std::cout << "Grabbing output buffer..." << std::endl;
	ProcessedPCoeffSum* countConnectedSumBuf = numaQueue.resultBufferAlloc.pop_wait().value();
	//std::cout << "Grabbed output buffer" << std::endl;
	
	data->outBuf = countConnectedSumBuf;

	cl_event readFinished = data->kernel->readBuffer(data->resultMem, countConnectedSumBuf, bufferSize, 1, &kernelFinished);
	
	clSetEventCallback(readFinished, CL_COMPLETE, [](cl_event ev, cl_int evStatus, void* myData){
		if(evStatus != CL_COMPLETE) {
			std::cerr << "Incomplete event!\n" << std::flush;
			exit(-1);
		}

		FPGAData* data = static_cast<FPGAData*>(myData);

		//std::cout << "Finished job " + std::to_string(data->job.getTop()) + " with " + std::to_string(data->job.getNumberOfBottoms()) + " bottoms\n" << std::flush;

		OutputBuffer result;
		result.originalInputData = std::move(data->job);
		result.outputBuf = data->outBuf;
		PCoeffProcessingContextEighth& numaQueue = data->queues->getNUMAForBuf(result.originalInputData.bufStart);
		numaQueue.outputQueue.push(result);

		pushJobIntoKernel(data);
	}, data);

	//std::cout << "Job " + std::to_string(topIdx) + " submitted\n" << std::flush;
}

constexpr size_t DEVICE_COUNT = 2;

void fpgaProcessor_Throughput_OnDevices(PCoeffProcessingContext& queues, const uint64_t* mbfLUT) {
	cl_event dones[NUM_BUFFERS * DEVICE_COUNT];
	FPGAData storedData[NUM_BUFFERS * DEVICE_COUNT];

	PCoeffKernel kernels[DEVICE_COUNT];
	size_t queueIdxes[DEVICE_COUNT];

	std::cout << "\033[31m[FPGA Processor] Initializing Kernels.\033[39m\n" << std::flush;
	for(size_t deviceI = 0; deviceI < DEVICE_COUNT; deviceI++) {
		PCoeffKernel& k = kernels[deviceI];
		queueIdxes[deviceI] = 0;

		k.init(devices[deviceI], mbfLUT, kernelFile.c_str());
		k.createBuffers(NUM_BUFFERS, BUFFER_SIZE);

		for(size_t i = 0; i < NUM_BUFFERS; i++) {
			size_t doneI = deviceI * NUM_BUFFERS + i;
			cl_int status;
			dones[doneI] = clCreateUserEvent(k.context, &status);
			checkError(status, "clCreateUserEvent error");
			//clSetUserEventStatus(dones[doneI], CL_QUEUED);
			storedData[doneI].queues = &queues;
			storedData[doneI].kernel = &k;
			storedData[doneI].curBufIdx = &queueIdxes[deviceI];
			storedData[doneI].done = &dones[doneI];
			storedData[doneI].inputMem = k.inputMems[i];
			storedData[doneI].resultMem = k.resultMems[i];
		}
	}
	std::cout << "\033[31m[FPGA Processor] Waiting for intialization finish.\033[39m\n" << std::flush;
	for(size_t deviceI = 0; deviceI < DEVICE_COUNT; deviceI++) {
		kernels[deviceI].finish();
	}
	std::cout << "\033[31m[FPGA Processor] Performing Dry-Run.\033[39m\n" << std::flush;
	dryRunKernels(kernels, DEVICE_COUNT);

	std::cout << "\033[31m[FPGA Processor] FPGA Processor started.\033[39m\n" << std::flush;
	for(size_t bufI = 0; bufI < NUM_BUFFERS*DEVICE_COUNT; bufI++) {
		pushJobIntoKernel(&storedData[bufI]);
	}

	std::cout << "\033[31m[FPGA Processor] Initial jobs submitted!\033[39m\n" << std::flush;
	
	std::thread t0([&](){checkError(clWaitForEvents(NUM_BUFFERS * 1, dones), "clWaitForEvents error 0000000000000000000");});
	checkError(clWaitForEvents(NUM_BUFFERS * 1, dones + NUM_BUFFERS), "clWaitForEvents error 1111111111111111111");

	t0.join();
	//kernels[1].finish();
	//cl_int status = clWaitForEvents(NUM_BUFFERS * 1, dones);
	//checkError(status, "clWaitForEvents error");
	std::cout << "\033[31m[FPGA Processor] Exit!\033[39m\n" << std::flush;
}

void fpgaProcessor_Throughput(PCoeffProcessingContext& queues, const void* mbfs[2]) {
	initPlatform();
	const uint64_t* mbfLUT = initMBFLUT(mbfs[0]);

	std::cout << "\033[31m[FPGA Processor] Creating FPGA Processors for " + std::to_string(numDevices) + " devices!\033[39m\n" << std::flush;

	fpgaProcessor_Throughput_OnDevices(queues, mbfLUT);

	std::cout << "\033[31m[FPGA Processor] All FPGA Processors done!\033[39m\n" << std::flush;
}


#include "../dedelib/supercomputerJobs.h"
#include "../dedelib/timeTracker.h"

// Supercomputing entry point.
int main(int argc, char** argv) {
	TimeTracker timer("FPGA computation ");
	if(argc != 3) {
		std::cerr << "Usage: " << argv[0] << " <computeFolder> <jobID>\n" << std::flush;
		return -1;
	}
	std::string computeFolder(argv[1]);
	std::string jobID(argv[2]);

	processJob(7, computeFolder, jobID, "fpga", fpgaProcessor_Throughput);
	return 0;
}


/*
// Parses "38431,31,13854,111,3333" to {38431,31,13854,111,3333}
template<typename IntT>
static std::vector<IntT> parseIntList(const std::string& strList) {
	std::stringstream strStream(strList);
	std::string segment;
	std::vector<IntT> resultingCounts;

	while(std::getline(strStream, segment, ',')) {
		resultingCounts.push_back(std::stoi(segment));
	}

	return resultingCounts;
}

static std::vector<NodeIndex> parseArgs(int argc, char** argv) {
	ParsedArgs parsed(argc, argv);
	configure(parsed); // Configures memory mapping, and other settings
	
	Options options(argc, argv);

	if(options.has("kernel")) {
		kernelFile = options.get<std::string>("kernel");
		std::cout << "Set Kernel file to: " << kernelFile << "[.aocx]" << std::endl;
	}

	if(options.has("show")) {
		SHOW_NONZEROS = true;
	}

	if(options.has("showall")) {
		SHOW_ALL = true;
		SHOW_NONZEROS = true;
	}

	if(options.has("shuffle")) {
		ENABLE_SHUFFLE = true;
		std::cout << "Enabled shuffling" << std::endl;
	}

	if(options.has("compare")) {
		ENABLE_COMPARE = true;
		std::cout << "Enabled comparing with MultiThread CPU implementation" << std::endl;
	}

	if(options.has("stats")) {
		ENABLE_STATISTICS = true;
		std::cout << "Enabled statistics!" << std::endl;
	}

	if(options.has("front")) {
		SHOW_FRONT = options.get<int>("front");
		std::cout << "Set front showing to " << SHOW_FRONT << std::endl;
	}

	if(options.has("tail")) {
		SHOW_TAIL = options.get<int>("tail");
		std::cout << "Set tail showing to " << SHOW_TAIL << std::endl;
	}

	if(options.has("validate")) {
		USE_VALIDATOR = true;
		std::cout << "Enabled Validator" << std::endl;
	}

	if(options.has("throughput")) {
		THROUGHPUT_MODE = true;
	}

	if(options.has("onlyKernel")) {
		ONLY_KERNEL = true;
	}

	const std::vector<std::string>& argsFromParsedArgs = parsed.args();
	std::vector<NodeIndex> topsToProcess;
	if(argsFromParsedArgs.size() >= 1) {
		// Given set of tops
		topsToProcess = parseIntList<NodeIndex>(argsFromParsedArgs[0]);
	} else {
		// Sample tops
		int SAMPLE_COUNT = 10;

		if(options.has("sample")) {
			SAMPLE_COUNT = options.get<int>("sample");
			std::cout << "Set number of samples to N=" << SAMPLE_COUNT << std::endl;
		}

		for(int i = 0; i < SAMPLE_COUNT; i++) {
			double expectedIndex = mbfCounts[7] * (i+0.5) / SAMPLE_COUNT;
			topsToProcess.push_back(static_cast<NodeIndex>(expectedIndex));
		}
		std::default_random_engine generator;
		std::shuffle(topsToProcess.begin(), topsToProcess.end(), generator);
		std::vector<NodeIndex> duplicatedTopsToProcess;
		duplicatedTopsToProcess.reserve(8*topsToProcess.size());
		for(NodeIndex item : topsToProcess) {
			for(int i = 0; i < 8; i++) {
				duplicatedTopsToProcess.push_back(item + i);
			}
		}
		topsToProcess = std::move(duplicatedTopsToProcess);
	}

	return topsToProcess;
}

void justProcessorInitializationMain() {
	PCoeffProcessingContext context(7);

	const void* mbfs[2];
	loadNUMA_MBFs(7, mbfs);

	fpgaProcessor_Throughput(context, mbfs);
}

// Testing entry point.
int main(int argc, char** argv) {
	std::vector<NodeIndex> topsToProcess = parseArgs(argc, argv);
	if(ONLY_KERNEL) {
		justProcessorInitializationMain();
		return 0;
	}
	auto validatorFunc = NO_VALIDATOR;
	if(USE_VALIDATOR) {
		std::cout << "Using Validator!" << std::endl;
		validatorFunc = threadPoolBufferValidator<7>;
	}
	try {
		std::future<std::vector<JobTopInfo>> topsToProcessFuture = std::async([&]() -> std::vector<JobTopInfo> {
			std::vector<JobTopInfo> jobTopInfos;
			std::cout << "Processing tops: ";
			const FlatNode* flatNodes = readFlatBuffer<FlatNode>(FileName::flatNodes(7), mbfCounts[7]);
			for(NodeIndex i : topsToProcess) {
				JobTopInfo newInfo;
				newInfo.top = i;
				newInfo.topDual = flatNodes[i].dual;
				jobTopInfos.push_back(newInfo);
				std::cout << i << ',';
			}
			std::cout << std::endl;
			delete[] flatNodes;
			return jobTopInfos;
		});
		std::cout << "Pipelining computation for " << topsToProcess.size() << " tops..." << std::endl;
		
		ResultProcessorOutput result = pcoeffPipeline(7, topsToProcessFuture, fpgaProcessor_Throughput, validatorFunc);
		
		//ResultProcessorOutput result = pcoeffPipeline(7, topsToProcess, fpgaProcessor_FullySerial);
		std::cout << "Computation Finished!" << std::endl;

		for(const BetaResult& r : result.results) {
			BetaSumPair bsp = r.dataForThisTop;
			std::cout << r.topIndex << ": " << bsp.betaSum.betaSum << ", " << bsp.betaSum.countedIntervalSizeDown << std::endl;
		}
		
		// Free the resources allocated
		std::cout << "Cleaning up" << std::endl;
		cleanup();
		std::cout << "Cleanup finished" << std::endl;

	} catch(const char* text) {
		std::cerr << "ERROR:" << text << std::endl;
	}
	return 0;
}*/
