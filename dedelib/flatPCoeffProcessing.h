#pragma once

#include <condition_variable>
#include <cstdint>
#include <vector>
#include <thread>
#include <queue>
#include <optional>

#include "knownData.h"
#include "flatMBFStructure.h"
#include "swapperLayers.h"
#include "bitSet.h"
#include "u192.h"
#include "connectGraph.h"
#include "threadPool.h"

#include "flatPCoeff.h"

#include "synchronizedQueue.h"

#include "bottomBufferCreator.h"

struct OutputBuffer {
	JobInfo originalInputData;
	ProcessedPCoeffSum* outputBuf;
};

class PCoeffProcessingContext {
public:
	/*
		This is a closed-loop buffer circulation system. 
		Buffers are allocated once at the start of the program, 
		and are then passed from module to module. 
		There are two classes of buffers in circulation:
			- NodeIndex* buffers: These contain the MBF indices 
				                  of the bottoms that correspond
								  to the top, which is the first
								  element. 
			- ProcessedPCoeffSum* buffers:  These contain the results 
								            of processPCoeffSum on
								            each of the inputs. 
	*/
	SynchronizedQueue<JobInfo> inputQueue;

	SynchronizedQueue<OutputBuffer> outputQueue;

	// Return queues are implemented as stacks, to try and reuse recently retired buffers more often, to improve cache coherency. 
	SynchronizedStack<NodeIndex*> inputBufferReturnQueue;
	SynchronizedStack<ProcessedPCoeffSum*> outputBufferReturnQueue;

	PCoeffProcessingContext(unsigned int Variables, size_t numberOfInputBuffers, size_t numberOfOutputBuffers);
	~PCoeffProcessingContext();
};

template<unsigned int Variables, size_t BatchSize>
void inputProducer(const FlatMBFStructure<Variables>& allMBFData, PCoeffProcessingContext& processingContext, const std::vector<NodeIndex>& topsToProcess) {
	std::cout << "Input processor started. " << topsToProcess.size() << " tops to process!\n" << std::flush;
	JobBatch<Variables, BatchSize> jobBatch;
	SwapperLayers<Variables, BitSet<BatchSize>> swapper;

	for(size_t i = 0; i < topsToProcess.size(); i += BatchSize) {
		size_t numberInThisBatch = std::min(topsToProcess.size() - i, BatchSize);

		NodeIndex* poppedBuffers[BatchSize];
		processingContext.inputBufferReturnQueue.popN_wait(poppedBuffers, numberInThisBatch);

		for(size_t j = 0; j < numberInThisBatch; j++) {
			jobBatch.jobs[j].bufStart = poppedBuffers[j];
		}

		NodeIndex topsForThisBatch[BatchSize];
		for(size_t j = 0; j < numberInThisBatch; j++) topsForThisBatch[j] = topsToProcess[i + j];

		buildJobBatch(allMBFData, topsForThisBatch, numberInThisBatch, jobBatch, swapper);

		processingContext.inputQueue.pushN(jobBatch.jobs, numberInThisBatch);
	}
	std::cout << "Input processor finished.\n" << std::flush;
}

template<unsigned int Variables>
void resultProcessor(const FlatMBFStructure<Variables>& allMBFData, PCoeffProcessingContext& processingContext, std::vector<BetaResult>& finalResults) {
	std::cout << "Result processor started.\n" << std::flush;
	for(std::optional<OutputBuffer> outputBuffer; (outputBuffer = processingContext.outputQueue.pop_wait()).has_value(); ) {
		OutputBuffer outBuf = outputBuffer.value();

		BetaResult curBetaResult;
		//if constexpr(Variables == 7) std::cout << "Results for job " << outBuf.originalInputData.getTop() << std::endl;
		curBetaResult.topIndex = outBuf.originalInputData.getTop();
		curBetaResult.betaSum = produceBetaResult(allMBFData, outBuf.originalInputData, outBuf.outputBuf);

		processingContext.inputBufferReturnQueue.push(outBuf.originalInputData.bufStart);
		processingContext.outputBufferReturnQueue.push(outBuf.outputBuf);

		finalResults.push_back(curBetaResult);
		if constexpr(Variables == 7) std::cout << '.' << std::flush;
		if(finalResults.size() % 1000 == 0) std::cout << std::endl << finalResults.size() << std::endl;
	}
	std::cout << "Result processor finished.\n" << std::flush;
}

// Deterministically shuffles the input bots to get a more uniform mix of bot difficulty
void shuffleBots(NodeIndex* bots, NodeIndex* botsEnd);

template<unsigned int Variables>
void cpuProcessor_SingleThread(const FlatMBFStructure<Variables>& allMBFData, PCoeffProcessingContext& context) {
	std::cout << "SingleThread CPU Processor started.\n" << std::flush;
	for(std::optional<JobInfo> jobOpt; (jobOpt = context.inputQueue.pop_wait()).has_value(); ) {
		JobInfo& job = jobOpt.value();

		//shuffleBots(job.bufStart + 1, job.bufEnd);
		//std::cout << "Grabbed job of size " << job.size() << '\n' << std::flush;
		ProcessedPCoeffSum* countConnectedSumBuf = context.outputBufferReturnQueue.pop_wait();
		//std::cout << "Grabbed output buffer.\n" << std::flush;
		processBetasCPU_SingleThread(allMBFData, job, countConnectedSumBuf);
		OutputBuffer result;
		result.originalInputData = job;
		result.outputBuf = countConnectedSumBuf;
		context.outputQueue.push(result);
		//std::cout << "Result pushed.\n" << std::flush;
	}
	std::cout << "SingleThread CPU Processor finished.\n" << std::flush;
}

template<unsigned int Variables>
void cpuProcessor_CoarseMultiThread(const FlatMBFStructure<Variables>& allMBFData, PCoeffProcessingContext& context) {
	std::cout << "Coarse MultiThread CPU Processor started.\n" << std::flush;
	ThreadPool pool;
	pool.doInParallel([&]() {cpuProcessor_SingleThread(allMBFData, context);});
	std::cout << "Coarse MultiThread CPU Processor finished.\n" << std::flush;
}

template<unsigned int Variables>
void cpuProcessor_FineMultiThread(const FlatMBFStructure<Variables>& allMBFData, PCoeffProcessingContext& context) {
	std::cout << "Fine MultiThread CPU Processor started.\n" << std::flush;
	ThreadPool pool;
	for(std::optional<JobInfo> jobOpt; (jobOpt = context.inputQueue.pop_wait()).has_value(); ) {
		JobInfo& job = jobOpt.value();
		ProcessedPCoeffSum* countConnectedSumBuf = context.outputBufferReturnQueue.pop_wait();
		//shuffleBots(job.bufStart + 1, job.bufEnd);
		processBetasCPU_MultiThread(allMBFData, job, countConnectedSumBuf, pool);
		OutputBuffer result;
		result.originalInputData = job;
		result.outputBuf = countConnectedSumBuf;
		context.outputQueue.push(result);
	}
	std::cout << "Fine MultiThread CPU Processor finished.\n" << std::flush;
}

std::vector<JobTopInfo> convertTopInfos(const FlatNode* flatNodes, const std::vector<NodeIndex>& topIndices);

// Requires a Processor function of type void(const FlatMBFStructure<Variables>& allMBFData, PCoeffProcessingContext& context)
template<unsigned int Variables, typename Processor>
std::vector<BetaResult> pcoeffPipeline(const FlatMBFStructure<Variables>& allMBFData, const std::vector<NodeIndex>& topIndices, const Processor& processorFunc, size_t numberOfInputBuffers, size_t numberOfOutputBuffers) {
	if(numberOfInputBuffers < 64) {
		std::cerr << "Too few input buffers!" << std::endl;
		std::abort();
	}
	
	PCoeffProcessingContext context(Variables, numberOfInputBuffers, numberOfOutputBuffers);

	std::vector<BetaResult> results;
	results.reserve(topIndices.size());

	std::vector<JobTopInfo> topInfos = convertTopInfos(allMBFData.allNodes, topIndices);

	std::thread inputProducerThread([&]() {
		try {
			runBottomBufferCreator(Variables, topInfos, context.inputQueue, context.inputBufferReturnQueue);
		} catch(const char* errText) {
			std::cerr << "Error thrown in inputProducerThread: " << errText;
			exit(-1);
		}
	});
	std::thread processorThread([&]() {
		try {
			processorFunc(allMBFData, context);
		} catch(const char* errText) {
			std::cerr << "Error thrown in processorThread: " << errText;
			exit(-1);
		}
	});
	std::thread resultProcessingThread([&]() {
		try {
			resultProcessor<Variables>(allMBFData, context, results);
		} catch(const char* errText) {
			std::cerr << "Error thrown in resultProcessingThread: " << errText;
			exit(-1);
		}
	});

	inputProducerThread.join();
	context.inputQueue.close();

	processorThread.join();
	context.outputQueue.close();

	resultProcessingThread.join();

	assert(results.size() == topIndices.size());

	return results;
}

// Requires a Processor function of type void(const FlatMBFStructure<Variables>& allMBFData, PCoeffProcessingContext& context)
template<unsigned int Variables, typename Processor>
void processDedekindNumber(const Processor& processorFunc, size_t numberOfInputBuffers = 200, size_t numberOfOutputBuffers = 20) {
	std::vector<NodeIndex> topsToProcess;
	for(NodeIndex i = 0; i < mbfCounts[Variables]; i++) {
		topsToProcess.push_back(i);
	}

	std::cout << "Reading FlatMBFStructure..." << std::endl;
	const FlatMBFStructure<Variables> allMBFData = readFlatMBFStructure<Variables>();
	std::cout << "FlatMBFStructure initialized." << std::endl;

	std::cout << "Starting Computation..." << std::endl;
	std::vector<BetaResult> betaResults = pcoeffPipeline<Variables, Processor>(allMBFData, topsToProcess, processorFunc, numberOfInputBuffers, numberOfOutputBuffers);

	BetaResultCollector collector(Variables);
	collector.addBetaResults(betaResults);

	std::cout << "Computation finished." << std::endl;
	u192 dedekindNumber = computeDedekindNumberFromBetaSums(allMBFData, collector.getResultingSums());

	std::cout << "D(" << (Variables + 2) << ") = " << dedekindNumber << std::endl;
}

std::vector<NodeIndex> generateRangeSample(unsigned int Variables, NodeIndex sampleCount);
