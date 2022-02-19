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

	SynchronizedQueue<NodeIndex*> inputBufferReturnQueue;
	SynchronizedQueue<ProcessedPCoeffSum*> outputBufferReturnQueue;

	PCoeffProcessingContext(unsigned int Variables, size_t numberOfInputBuffers, size_t numberOfOutputBuffers);
	~PCoeffProcessingContext();
};

template<unsigned int Variables, size_t BatchSize>
void inputProducer(const FlatMBFStructure<Variables>& allMBFData, PCoeffProcessingContext& processingContext, const std::vector<NodeIndex>& topsToProcess) {
	std::cout << "Input processor started. " << topsToProcess.size() << " tops to process!" << std::endl;
	JobBatch<Variables, BatchSize> jobBatch;
	SwapperLayers<Variables, BitSet<BatchSize>> swapper;

	for(size_t i = 0; i < topsToProcess.size(); i += BatchSize) {
		size_t numberInThisBatch = std::min(topsToProcess.size() - i, BatchSize);

		NodeIndex* poppedBuffers[BatchSize];
		processingContext.inputBufferReturnQueue.popN_wait_forever(poppedBuffers, numberInThisBatch);

		for(size_t j = 0; j < numberInThisBatch; j++) {
			jobBatch.jobs[j].bufStart = poppedBuffers[j];
		}

		NodeIndex topsForThisBatch[BatchSize];
		for(size_t j = 0; j < numberInThisBatch; j++) topsForThisBatch[j] = topsToProcess[i + j];

		buildJobBatch(allMBFData, topsForThisBatch, numberInThisBatch, jobBatch, swapper);

		processingContext.inputQueue.pushN(jobBatch.jobs, numberInThisBatch);
	}
	std::cout << "Input processor finished." << std::endl;
}

template<unsigned int Variables>
void resultProcessor(const FlatMBFStructure<Variables>& allMBFData, PCoeffProcessingContext& processingContext, std::vector<BetaResult>& finalResults) {
	std::cout << "Result processor started." << std::endl;
	for(std::optional<OutputBuffer> outputBuffer; (outputBuffer = processingContext.outputQueue.pop_wait()).has_value(); ) {
		OutputBuffer outBuf = outputBuffer.value();

		BetaResult curBetaResult;
		curBetaResult.topIndex = outBuf.originalInputData.top;
		curBetaResult.betaSum = produceBetaResult(allMBFData, outBuf.originalInputData, outBuf.outputBuf);

		processingContext.inputBufferReturnQueue.push(outBuf.originalInputData.bufStart);
		processingContext.outputBufferReturnQueue.push(outBuf.outputBuf);

		finalResults.push_back(curBetaResult);
		if(finalResults.size() % 1000 == 0) std::cout << finalResults.size() << std::endl;
	}
	std::cout << "Result processor finished." << std::endl;
}

template<unsigned int Variables>
void cpuProcessor_SingleThread(const FlatMBFStructure<Variables>& allMBFData, PCoeffProcessingContext& context) {
	std::cout << "SingleThread CPU Processor started." << std::endl;
	for(std::optional<JobInfo> jobOpt; (jobOpt = context.inputQueue.pop_wait()).has_value(); ) {
		JobInfo& job = jobOpt.value();
		ProcessedPCoeffSum* countConnectedSumBuf = context.outputBufferReturnQueue.pop_wait().value();
		processBetasCPU_SingleThread(allMBFData, job.top, job.bufStart, job.bufEnd, countConnectedSumBuf);
		OutputBuffer result;
		result.originalInputData = job;
		result.outputBuf = countConnectedSumBuf;
		context.outputQueue.push(result);
	}
	std::cout << "SingleThread CPU Processor finished." << std::endl;
}

template<unsigned int Variables>
void cpuProcessor_CoarseMultiThread(const FlatMBFStructure<Variables>& allMBFData, PCoeffProcessingContext& context) {
	std::cout << "CoarseMultiThread CPU Processor started." << std::endl;
	ThreadPool pool;
	pool.doInParallel([&]() {cpuProcessor_SingleThread(allMBFData, context);});
	std::cout << "CoarseMultiThread CPU Processor finished." << std::endl;
}

// Requires a Processor function of type void(const FlatMBFStructure<Variables>& allMBFData, PCoeffProcessingContext& context)
template<unsigned int Variables, size_t BatchSize, typename Processor>
std::vector<BetaResult> pcoeffPipeline(const FlatMBFStructure<Variables>& allMBFData, const std::vector<NodeIndex>& topIndices, const Processor& processorFunc, size_t numberOfInputBuffers, size_t numberOfOutputBuffers) {
	PCoeffProcessingContext context(Variables, numberOfInputBuffers, numberOfOutputBuffers);

	std::vector<BetaResult> results;
	results.reserve(topIndices.size());

	std::thread inputProducerThread([&]() {inputProducer<Variables, BatchSize>(allMBFData, context, topIndices);});
	std::thread processorThread([&]() {processorFunc(allMBFData, context);});
	std::thread resultProcessingThread([&]() {resultProcessor<Variables>(allMBFData, context, results);});

	inputProducerThread.join();
	context.inputQueue.close();

	processorThread.join();
	context.outputQueue.close();

	resultProcessingThread.join();
	context.inputBufferReturnQueue.close();
	context.outputBufferReturnQueue.close();

	return results;
}

// Requires a Processor function of type void(const FlatMBFStructure<Variables>& allMBFData, PCoeffProcessingContext& context)
template<unsigned int Variables, size_t BatchSize, typename Processor>
void processDedekindNumber(const Processor& processorFunc, size_t numberOfInputBuffers = 70, size_t numberOfOutputBuffers = 20) {
	std::vector<NodeIndex> topsToProcess;
	for(NodeIndex i = 0; i < mbfCounts[Variables]; i++) {
		topsToProcess.push_back(i);
	}
	const FlatMBFStructure<Variables> allMBFData = readFlatMBFStructure<Variables>();

	std::vector<BetaResult> betaResults = pcoeffPipeline<Variables, BatchSize, Processor>(allMBFData, topsToProcess, processorFunc, numberOfInputBuffers, numberOfOutputBuffers);

	u192 dedekindNumber = computeDedekindNumberFromBetaSums(allMBFData, betaResults);

	std::cout << "D(" << (Variables + 2) << ") = " << dedekindNumber << std::endl;
}
