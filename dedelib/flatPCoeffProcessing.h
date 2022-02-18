#pragma once

#include <condition_variable>
#include <cstdint>
#include <vector>
#include <thread>
#include <queue>

#include "knownData.h"
#include "flatMBFStructure.h"
#include "swapperLayers.h"
#include "bitSet.h"
#include "u192.h"
#include "connectGraph.h"
#include "threadPool.h"

#include "flatPCoeff.h"

#include "synchronizedQueue.h"

struct ResultBuffer {
	JobInfo originalInputData;
	ProcessedPCoeffSum* resultsBuf;
};

template<unsigned int Variables>
class PCoeffProcessingContext {
public:
	FlatMBFStructure<Variables> allMBFData;

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

	SynchronizedQueue<ResultBuffer> resultsQueue;

	SynchronizedQueue<NodeIndex*> inputBufferReturnQueue;
	SynchronizedQueue<ProcessedPCoeffSum*> resultBufferReturnQueue;



	PCoeffProcessingContext() : allMBFData(readFlatMBFStructure<Variables>()) {}
	

};

template<unsigned int Variables, size_t BatchSize>
class InputProducer {
	std::thread productionThread;

public:
	InputProducer(const std::vector<NodeIndex>& topsToProcess, PCoeffProcessingContext& processingContext, size_t numberOfInputBuffers) : 
		productionThread([this, &processingContext, &topsToProcess](){
			JobBatch<Variables, BatchSize> jobBatch;
			SwapperLayers<Variables, BitSet<BatchSize>> swapper;

			std::cout << "Allocating " << numberOfInputBuffers << " input buffers." << std::endl;
			for(size_t i = 0; i < numberOfInputBuffers; i++) {
				processingContext.inputBufferReturnQueue.push(static_cast<NodeIndex*>(malloc(sizeof(NodeIndex) * MAX_BUFSIZE(Variables))));
			}

			for(size_t i = 0; i < topsToProcess.size(); i += BatchSize) {
				size_t numberInThisBatch = std::min(topsToProcess.size() - i, BatchSize);

				for(size_t j = 0; j < numberInThisBatch; j++) {
					std::optional<NodeIndex*> inputBuffer = this->processingContext.inputBufferReturnQueue.pop_wait();
					assert(inputBuffer.has_value());
					jobBatch.jobs[j].bufStart = inputBuffer;
				}

				NodeIndex topsForThisBatch[BatchSize];
				for(size_t j = 0; j < numberInThisBatch; j++) topsForThisBatch[j] = topsToProcess[i + j];

				buildJobBatch(processingContext.allMBFData, topsForThisBatch, numberInThisBatch, jobBatch, swapper);

				for(size_t j = 0; j < numberInThisBatch; j++) {
					processingContext.inputQueue.push(jobBatch.jobs[j]);
				}
			}
			processingContext.inputQueue.close();
		}) {}

	~InputProducer() {
		std::cout << "Closing InputProducer thread..." << std::endl;
		productionThread.join();
		std::cout << "InputProducer thread closed. " << std::endl;
		std::cout << "Deleting input buffers..." << std::endl;
		size_t numFreedBuffers = 0;
		for(std::optional<NodeIndex*> buffer; (buffer = processingContext.pop_wait()).has_value();) {
			free(buffer.value());
			numFreedBuffers++;
		}
		std::cout << "Deleted " << numFreedBuffers << " input buffers. " << std::endl;
	}
};

struct ResultCollection {
	NodeIndex topIndex;
	BetaSum betaSum;
};

template<unsigned int Variables>
class ResultProcessor {
public:
	std::vector<ResultCollection> finalResults;
private:	
	std::thread collectionThread;
public:
	ResultProcessor(PCoeffProcessingContext& processingContext, size_t expectedNumberOfResults) : 
		collectionThread([this](){
			for(std::optional<ResultBuffer> resultBuffer; (resultBuffer = processingContext.resultsQueue.pop_wait()).has_value(); ) {
				ResultBuffer rb = resultBuffer.value();

				ResultCollection curBetaResult;
				curBetaResult.topIndex = rb.originalInputData.top;
				curBetaResult.betaSum = produceBetaResult(processingContext.allMBFData, rb.originalInputData, rb.resultsBuf);
				finalResults.push_back(curBetaResult);
			}
		}) {

		finalResults.reserve(expectedNumberOfResults);
	}

	~ResultProcessor() {
		std::cout << "Closing ResultProcessor thread..." << std::endl;
		collectionThread.join();
		std::cout << "ResultProcessor thread closed. " << std::endl;
	}
};
