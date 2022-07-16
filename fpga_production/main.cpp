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
#include "AOCLUtils/aocl_utils.h"

#include "../dedelib/toString.h"
#include "../dedelib/flatPCoeffProcessing.h"
#include "../dedelib/knownData.h"
#include "../dedelib/configure.h"

#include "fpgaBoilerplate.h"
#include "fpgaProcessor.h"

using namespace aocl_utils;


// Runtime constants
static bool SHOW_NONZEROS = false;
static bool SHOW_ALL = false;
static bool ENABLE_SHUFFLE = false;
static bool ENABLE_COMPARE = false;
static bool ENABLE_STATISTICS = false;
static cl_uint SHOW_FRONT = 0;
static cl_uint SHOW_TAIL = 0;
static double ACTIVITY_MULTIPLIER = 10*60.0;
static bool THROUGHPUT_MODE = false;

static std::string kernelFile;


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
	bool done; // TODO implement better
};

bool allDone(FPGAData* datas, size_t numDatas) {
	for(size_t i = 0; i < numDatas; i++) {
		if(datas[i].done == false) return false;
	}
	return true;
}

void pushJobIntoKernel(FPGAData* data) {
	std::optional<JobInfo> jobOpt = data->queues->inputQueue.pop_wait();
	if(!jobOpt.has_value()) {
		data->done = true;
		return;
	}
	data->job = std::move(jobOpt).value();
	JobInfo& job = data->job;
	cl_uint bufferSize = static_cast<cl_uint>(job.size());
	size_t numberOfBottoms = job.getNumberOfBottoms();
	NodeIndex topIdx = job.getTop();
	std::cout << "Grabbed job " << topIdx << " with " << numberOfBottoms << " bottoms\n" << std::flush;

	constexpr cl_uint JOB_SIZE_ALIGNMENT = 16*32; // Block Size 32, shuffle size 16

	//std::cout << "Aligning buffer..." << std::endl;
	for(; (bufferSize+2) % JOB_SIZE_ALIGNMENT != 0; bufferSize++) {
		job.bufStart[bufferSize] = mbfCounts[7] - 1; // Fill with the global TOP mbf. AKA 0xFFFFFFFF.... to minimize wasted work
	}

	for(size_t i = 0; i < 2; i++) {
		job.bufStart[bufferSize++] = 0x80000000; // Tops at the end for stats collection
	}

	assert(bufferSize % JOB_SIZE_ALIGNMENT == 0);
	
	//std::cout << "Resulting buffer size: " << bufferSize << std::endl;

	cl_event writeFinished = data->kernel->writeBuffer(data->inputMem, job.bufStart, bufferSize);
	cl_event kernelFinished = data->kernel->launchKernel(data->inputMem, data->resultMem, bufferSize, 1, &writeFinished);

	//std::cout << "Grabbing output buffer..." << std::endl;
	ProcessedPCoeffSum* countConnectedSumBuf = data->queues->outputBufferReturnQueue.alloc_wait(bufferSize);
	//std::cout << "Grabbed output buffer" << std::endl;
	
	data->outBuf = countConnectedSumBuf;

	cl_event readFinished = data->kernel->readBuffer(data->resultMem, countConnectedSumBuf, bufferSize, 1, &kernelFinished);
	
	clSetEventCallback(readFinished, CL_COMPLETE, [](cl_event ev, cl_int evStatus, void* myData){
		if(evStatus != CL_COMPLETE) {
			std::cerr << "Incomplete event!\n" << std::flush;
			exit(-1);
		}

		FPGAData* data = static_cast<FPGAData*>(myData);

		std::cout << "Finished job " << data->job.getTop() << " with " << data->job.getNumberOfBottoms() << " bottoms\n" << std::flush;

		data->queues->outputBufferReturnQueue.free(data->outBuf); // temporary
		data->queues->inputBufferReturnQueue.push(data->job.bufStart);

		pushJobIntoKernel(data);
	}, data);

	std::cout << "Job " << topIdx << " submitted\n" << std::flush;
}

void fpgaProcessor_Throughput(PCoeffProcessingContext& queues) {
	initPlatform();
	const uint64_t* mbfLUT = initMBFLUT();

	PCoeffKernel kernel;

	kernel.init(devices[0], mbfLUT, kernelFile.c_str());
	
	cl_mem inputMems[NUM_BUFFERS];
	cl_mem resultMems[NUM_BUFFERS];
	kernel.createBuffers(inputMems, resultMems, NUM_BUFFERS, BUFFER_SIZE);

	FPGAData storedData[NUM_BUFFERS];
	for(size_t i = 0; i < NUM_BUFFERS; i++) {
		storedData[i].queues = &queues;
		storedData[i].kernel = &kernel;
		storedData[i].done = false;
		storedData[i].inputMem = inputMems[i];
		storedData[i].resultMem = resultMems[i];
	}

	std::cout << "FPGA Processor started.\n" << std::flush;
	for(size_t bufI = 0; bufI < NUM_BUFFERS; bufI++) {
		std::cout << "\033[31m[FPGA Processor] Initial!\033[39m\n" << std::flush;
		pushJobIntoKernel(&storedData[bufI]);
	}

	std::cout << "\033[31m[FPGA Processor] Initial jobs submitted!\033[39m\n" << std::flush;
	while(!allDone(storedData, NUM_BUFFERS)) { 
		kernel.finish();
	}
	std::cout << "\033[31m[FPGA Processor] Exit!\033[39m\n" << std::flush;
}

/*void fpgaProcessor_FullySerial(PCoeffProcessingContext& queues) {
	init(kernelFile.c_str());

	std::optional<std::ofstream> statsFile = {};
	if(ENABLE_STATISTICS) {
		statsFile = std::ofstream(FileName::dataPath + "statsFile.csv");
		std::ofstream& statsf = statsFile.value();

		statsf << "TopIdx,BotCount,Total Permutations,Permutations Per Bot,Runtime,Total Cycles,Occupation,Bots/s";

		for(int i = 0; i <= 5040; i++) {
			statsf << ',' << i;
		}
	}

	std::cout << "FPGA Processor started.\n" << std::flush;
	for(std::optional<JobInfo> jobOpt; (jobOpt = queues.inputQueue.pop_wait()).has_value(); ) {
		JobInfo& job = jobOpt.value();
		cl_uint bufferSize = static_cast<cl_int>(job.size());
		size_t numberOfBottoms = job.getNumberOfBottoms();
		NodeIndex topIdx = job.getTop();
		std::cout << "Grabbed job " << topIdx << " with " << numberOfBottoms << " bottoms\n" << std::flush;

		if(ENABLE_SHUFFLE) shuffleBots(job.begin(), job.end());

		constexpr cl_uint JOB_SIZE_ALIGNMENT = 16*32; // Block Size 32, shuffle size 16

		std::cout << "Aligning buffer..." << std::endl;
		for(; (bufferSize+2) % JOB_SIZE_ALIGNMENT != 0; bufferSize++) {
			job.bufStart[bufferSize] = mbfCounts[7] - 1; // Fill with the global TOP mbf. AKA 0xFFFFFFFF.... to minimize wasted work
		}

		for(size_t i = 0; i < 2; i++) {
			job.bufStart[bufferSize++] = 0x80000000; // Tops at the end for stats collection
		}

		assert(bufferSize % JOB_SIZE_ALIGNMENT == 0);
		
		std::cout << "Resulting buffer size: " << bufferSize << std::endl;

		if(SHOW_TAIL != 0) {
			std::cout << "Tail: " << std::endl;
			for(cl_uint i = std::max(bufferSize - SHOW_TAIL, cl_uint(0)); i < bufferSize + 5; i++) { // Look a bit past the end of the job, for fuller picture
				std::cout << job.bufStart[i] << ',';
			}
			std::cout << std::endl;
		}

		if(SHOW_FRONT != 0) {
			std::cout << "Front: " << std::endl;
			for(cl_uint i = 0; i < SHOW_FRONT; i++) {
				std::cout << job.bufStart[i] << ',';
			}
			std::cout << std::endl;
		}

		cl_int status = clEnqueueWriteBuffer(queue,inputMem[0],0,0,bufferSize*sizeof(uint32_t),job.bufStart,0,0,0);
		checkError(status, "Failed to enqueue writing to inputMem buffer");
		status = clFinish(queue);

		auto kernelStart = std::chrono::system_clock::now();
		launchKernel(&inputMem[0], &resultMem[0], bufferSize, 0, 0, 0);
		status = clFinish(queue);

		auto kernelEnd = std::chrono::system_clock::now();
		double runtimeSeconds = std::chrono::duration<double>(kernelEnd - kernelStart).count();

		std::cout << "Grabbing output buffer..." << std::endl;
		ProcessedPCoeffSum* countConnectedSumBuf = queues.outputBufferReturnQueue.alloc_wait(bufferSize);
		std::cout << "Grabbed output buffer" << std::endl;

		status = clEnqueueReadBuffer(queue, resultMem[0], 0, 0, bufferSize*sizeof(uint64_t), countConnectedSumBuf, 0, 0, 0);
		checkError(status, "Failed to enqueue read buffer resultMem to countConnectedSumBuf");

		status = clFinish(queue);
		checkError(status, "Failed to Finish reading output buffer");

		std::cout << "Finished job " << topIdx << " with " << numberOfBottoms << " bottoms in " << runtimeSeconds << "s, at " << (double(numberOfBottoms)/runtimeSeconds/1000000.0) << "Mbots/s" << std::endl;

		// Use final dummy top to get proper occupation reading
		uint64_t debugA = countConnectedSumBuf[bufferSize-2];
		uint64_t debugB = countConnectedSumBuf[bufferSize-1];
		uint64_t activityData = getBitField(debugB, 32, 31);
		uint64_t cycleData = getBitField(debugB, 0, 32);

		uint64_t realCycles = cycleData << 9;
		uint64_t realActivity = activityData << 15;

		uint64_t ivalidCycles = getBitField(debugA, 48, 16) << 16;
		uint64_t ireadyCycles = getBitField(debugA, 32, 16) << 16;
		uint64_t ovalidCycles = getBitField(debugA, 16, 16) << 16;
		uint64_t oreadyCycles = getBitField(debugA, 0, 16) << 16;

		double occupation = realActivity / (ACTIVITY_MULTIPLIER * realCycles);

		std::cout << "Took " << realCycles << " cycles at " << (realCycles / runtimeSeconds / 1000000.0) << " Mcycles/s" << std::endl;
		std::cout << realActivity << " activity" << std::endl;
		std::cout << "Occupation: " << occupation * 100.0 << "%" << std::endl;
		std::cout << "Port fractions: ";
		std::cout << "ivalid: " << ivalidCycles * 100.0 / realCycles << "% ";
		std::cout << "iready: " << ireadyCycles * 100.0 / realCycles << "% ";
		std::cout << "ovalid: " << ovalidCycles * 100.0 / realCycles << "% ";
		std::cout << "oready: " << oreadyCycles * 100.0 / realCycles << "% " << std::endl;

		if(ENABLE_COMPARE) {
			std::cout << "Starting MultiThread CPU comparison..." << std::endl;
			std::unique_ptr<uint64_t[]> countConnectedSumBufCPU = std::make_unique<uint64_t[]>(job.size());
			ThreadPool pool;

			auto cpuStart = std::chrono::system_clock::now();
			processBetasCPU_MultiThread(mbfs, job, countConnectedSumBufCPU.get(), pool);
			auto cpuEnd = std::chrono::system_clock::now();
			double cpuRuntimeSeconds = std::chrono::duration<double>(cpuEnd - cpuStart).count();

			std::cout << "CPU processing completed. Took " << cpuRuntimeSeconds << "s, at " << (double(numberOfBottoms)/cpuRuntimeSeconds/1000000.0) << "Mbots/s" << std::endl;
			std::cout << "Kernel speedup of " << (cpuRuntimeSeconds / runtimeSeconds) << "x" << std::endl;

			for(const NodeIndex* curBot = job.begin(); curBot != job.end(); curBot++) {
				size_t index = job.indexOf(curBot);
				if((countConnectedSumBuf[index] & 0xE000000000000000) != 0) std::cout << "[ECC ECC]" << std::flush;
				if(countConnectedSumBuf[index] != countConnectedSumBufCPU[index]) {
					std::cout << "[!!!!!!!] Mistake found at position " << index << "/" << index << ": ";
					std::cout << "FPGA: {" << getPCoeffSum(countConnectedSumBuf[index]) << "; " << getPCoeffCount(countConnectedSumBuf[index]) << "} ";
					std::cout << "CPU: {" << getPCoeffSum(countConnectedSumBufCPU[index]) << "; " << getPCoeffCount(countConnectedSumBufCPU[index]) << "} " << std::endl;
				}
			}
			std::cout << "All " << numberOfBottoms << " elements checked!" << std::endl;
		}

		if(ENABLE_STATISTICS) {
			std::ofstream& statsf = statsFile.value();

			// statsf << "TopIdx,BotCount,Total Permutations,Permutations Per Bot,Runtime,Total Cycles,Occupation,Bots/s";

			uint64_t permutationBreakdown[5041];
			for(uint64_t& item : permutationBreakdown) item = 0;

			for(const NodeIndex* cur = job.begin(); cur != job.end(); cur++) {
				uint64_t pcoeffCount = getPCoeffCount(countConnectedSumBuf[job.indexOf(cur)]);

				permutationBreakdown[pcoeffCount]++;
			}
			uint64_t totalPermutations = 0;
			for(size_t i = 0; i <= 5040; i++) {
				totalPermutations += i * permutationBreakdown[i];
			}

			statsf << topIdx << ',';
			statsf << numberOfBottoms << ',';
			statsf << totalPermutations << ',';
			statsf << double(totalPermutations) / numberOfBottoms << ',';
			statsf << runtimeSeconds << ',';
			statsf << realCycles << ',';
			statsf << occupation << ',';
			statsf << double(numberOfBottoms)/runtimeSeconds;

			for(int i = 0; i <= 5040; i++) {
				statsf << ',' << permutationBreakdown[i]++;
			}

			statsf << std::endl;
		}

		// Comment out, result processor is *really* slow for some godforsaken reason. IT'S JUST MULTIPLIES!
		
		//OutputBuffer result;
		//result.originalInputData = job;
		//result.outputBuf = countConnectedSumBuf;
		//queues.outputQueue.push(result);
		
		queues.outputBufferReturnQueue.free(countConnectedSumBuf); // Temporary
		queues.inputBufferReturnQueue.push(job.bufStart);
	}
	std::cout << "FPGA Processor finished.\n" << std::flush;
}*/

/*
	Parses "38431,31,13854,111,3333" to {38431,31,13854,111,3333}
*/
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

	kernelFile = "dedekindAccelerator";

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

	if(options.has("throughput")) {
		THROUGHPUT_MODE = true;
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

// Entry point.
int main(int argc, char** argv) {
	std::vector<NodeIndex> topsToProcess = parseArgs(argc, argv);
	try {
		std::cout << "Processing tops: ";
		for(NodeIndex i : topsToProcess) std::cout << i << ',';
		std::cout << std::endl;

		std::cout << "Pipelining computation for " << topsToProcess.size() << " tops..." << std::endl;
		std::vector<BetaResult> result = pcoeffPipeline(7, topsToProcess, fpgaProcessor_Throughput, 200, 10, 100);
		//std::vector<BetaResult> result = pcoeffPipeline(7, topsToProcess, fpgaProcessor_FullySerial, 200, 10, 100);
		std::cout << "Computation Finished!" << std::endl;

		for(const BetaResult& r : result) {
			std::cout << r.topIndex << ": " << r.betaSum.betaSum << ", " << r.betaSum.countedIntervalSizeDown << std::endl;
		}
		
		// Free the resources allocated
		std::cout << "Cleaning up" << std::endl;
		cleanup();
		std::cout << "Cleanup finished" << std::endl;

	} catch(const char* text) {
		std::cerr << "ERROR:" << text << std::endl;
	}
	return 0;
}
