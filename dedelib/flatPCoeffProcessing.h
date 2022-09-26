#pragma once

#include <condition_variable>
#include <cstdint>
#include <vector>
#include <thread>
#include <queue>
#include <optional>
#include <future>

#include "knownData.h"
#include "flatMBFStructure.h"
#include "bitSet.h"
#include "u192.h"
#include "connectGraph.h"
#include "threadPool.h"
#include "threadUtils.h"
#include "numaMem.h"

#include "flatPCoeff.h"

#include "synchronizedQueue.h"

#include "bottomBufferCreator.h"

#include "processingContext.h"

// Deterministically shuffles the input bots to get a more uniform mix of bot difficulty
void shuffleBots(NodeIndex* bots, NodeIndex* botsEnd);

template<unsigned int Variables>
void cpuProcessor_SingleThread_MBF(PCoeffProcessingContext& context, const Monotonic<Variables>* mbfs) {
	std::cout << "SingleThread CPU Processor started.\n" << std::flush;
	size_t lastNUMANode = 0;
	for(std::optional<JobInfo> jobOpt; (jobOpt = context.inputQueue.pop_wait(lastNUMANode)).has_value(); ) {
		JobInfo& job = jobOpt.value();

		PCoeffProcessingContextEighth& subContext = context.getNUMAForBuf(job.bufStart);

		//shuffleBots(job.bufStart + 1, job.bufEnd);
		//std::cout << "Grabbed job of size " << job.bufferSize() << '\n' << std::flush;
		ProcessedPCoeffSum* countConnectedSumBuf = subContext.resultBufferAlloc.pop_wait();
		//std::cout << "Grabbed output buffer.\n" << std::flush;
		processBetasCPU_SingleThread(mbfs, job, countConnectedSumBuf);
		OutputBuffer result;
		result.originalInputData = job;
		result.outputBuf = countConnectedSumBuf;
		subContext.outputQueue.push(result);
		//std::cout << "Result pushed.\n" << std::flush;
	}
	std::cout << "SingleThread CPU Processor finished.\n" << std::flush;
}

template<unsigned int Variables>
void cpuProcessor_CoarseMultiThread_MBF(PCoeffProcessingContext& context, const Monotonic<Variables>* mbfs) {
	std::cout << "Coarse MultiThread CPU Processor started.\n" << std::flush;
	ThreadPool pool;
	pool.doInParallel([&]() {cpuProcessor_SingleThread_MBF(context, mbfs);});
	std::cout << "Coarse MultiThread CPU Processor finished.\n" << std::flush;
}

template<unsigned int Variables>
void cpuProcessor_FineMultiThread_MBF(PCoeffProcessingContext& context, const Monotonic<Variables>* mbfs) {
	std::cout << "Fine MultiThread CPU Processor started.\n" << std::flush;
	ThreadPool pool;
	size_t lastNUMANode = 0;
	for(std::optional<JobInfo> jobOpt; (jobOpt = context.inputQueue.pop_wait(lastNUMANode)).has_value(); ) {
		JobInfo& job = jobOpt.value();
		PCoeffProcessingContextEighth& subContext = context.getNUMAForBuf(job.bufStart);

		ProcessedPCoeffSum* countConnectedSumBuf = subContext.resultBufferAlloc.pop_wait();
		//shuffleBots(job.bufStart + 1, job.bufEnd);
		processBetasCPU_MultiThread(mbfs, job, countConnectedSumBuf, pool);
		OutputBuffer result;
		result.originalInputData = job;
		result.outputBuf = countConnectedSumBuf;
		subContext.outputQueue.push(result);
	}
	std::cout << "Fine MultiThread CPU Processor finished.\n" << std::flush;
}

template<unsigned int Variables>
void cpuProcessor_SuperMultiThread(PCoeffProcessingContext& context, const void* mbfs[2]) {
	// Assume 16 core complexes of 8 cores each
	constexpr int CORE_COMPLEX_COUNT = 16;
	constexpr int CORES_PER_COMPLEX = 8;

	struct ProcessorData {
		PCoeffProcessingContext* context;
		const Monotonic<Variables>* mbfs;
		int numaNode;
	};

	ProcessorData procData[8];
	for(int i = 0; i < 8; i++) {
		procData[i].context = &context;
		procData[i].mbfs = static_cast<const Monotonic<Variables>*>(mbfs[i / 4]);
		procData[i].numaNode = i;
	}

	auto processorFunc = [](void* voidData) -> void* {
		ThreadPool threadPool(CORES_PER_COMPLEX);

		ProcessorData* procData = (ProcessorData*) voidData;
		PCoeffProcessingContext& context = *procData->context;

		int numaNode = procData->numaNode;
		PCoeffProcessingContextEighth& numaQueue = *context.numaQueues[numaNode];
		for(std::optional<JobInfo> jobOpt; (jobOpt = context.inputQueue.pop_specific_wait(numaNode)).has_value(); ) {
			JobInfo& job = jobOpt.value();
			ProcessedPCoeffSum* countConnectedSumBuf = numaQueue.resultBufferAlloc.pop_wait();
			//shuffleBots(job.bufStart + 1, job.bufEnd);
			processBetasCPU_MultiThread(procData->mbfs, job, countConnectedSumBuf, threadPool);
			OutputBuffer result;
			result.originalInputData = job;
			result.outputBuf = countConnectedSumBuf;
			numaQueue.outputQueue.push(result);
		}

		pthread_exit(nullptr);
		return nullptr;
	};

	PThreadsSpread coreComplexThreads(CORE_COMPLEX_COUNT, CPUAffinityType::COMPLEX, procData, processorFunc, 2);
}

template<unsigned int Variables>
void cpuProcessor_SingleThread(PCoeffProcessingContext& context, const void* mbfs[2]) {
	cpuProcessor_SingleThread_MBF(context, static_cast<const Monotonic<Variables>*>(mbfs[0]));
}
template<unsigned int Variables>
void cpuProcessor_CoarseMultiThread(PCoeffProcessingContext& context, const void* mbfs[2]) {
	cpuProcessor_CoarseMultiThread_MBF(context, static_cast<const Monotonic<Variables>*>(mbfs[0]));
}
template<unsigned int Variables>
void cpuProcessor_FineMultiThread(PCoeffProcessingContext& context, const void* mbfs[2]) {
	cpuProcessor_FineMultiThread_MBF(context, static_cast<const Monotonic<Variables>*>(mbfs[0]));
}

ResultProcessorOutput pcoeffPipeline(unsigned int Variables, const std::vector<NodeIndex>& topIndices, void (*processorFunc)(PCoeffProcessingContext& context, const void* mbfs[2]), void(*validator)(const OutputBuffer&, const void*, ThreadPool&) = [](const OutputBuffer&, const void*, ThreadPool&){});

std::unique_ptr<u128[]> mergeResultsAndValidationForFinalBuffer(unsigned int Variables, const FlatNode* allMBFNodes, const ClassInfo* allClassInfos, const std::vector<BetaSumPair>& betaSums, const ValidationData* validationBuf);

template<unsigned int Variables>
void processDedekindNumber(void (*processorFunc)(PCoeffProcessingContext& context, const void* mbfs[2]), void(*validator)(const OutputBuffer&, const void*, ThreadPool&) = [](const OutputBuffer&, const void*, ThreadPool&){}) {
	std::vector<NodeIndex> topsToProcess;
	for(NodeIndex i = 0; i < mbfCounts[Variables]; i++) {
		topsToProcess.push_back(i);
	}

	shuffleBots(&topsToProcess[0], (&topsToProcess[0]) + topsToProcess.size());

	std::cout << "Starting Computation..." << std::endl;
	ResultProcessorOutput betaResults = pcoeffPipeline(Variables, topsToProcess, processorFunc, validator);

	BetaResultCollector collector(Variables);
	collector.addBetaResults(betaResults.results);

	std::cout << "Reading FlatMBFStructure..." << std::endl;
	const FlatMBFStructure<Variables> allMBFData = readFlatMBFStructure<Variables>();
	std::cout << "FlatMBFStructure initialized." << std::endl;

	std::cout << "Computation finished." << std::endl;
	std::vector<BetaSumPair> sortedBetaSumPairs = collector.getResultingSums();
	u192 dedekindNumber = computeDedekindNumberFromBetaSums(allMBFData, sortedBetaSumPairs);
	std::cout << "D(" << (Variables + 2) << ") = " << dedekindNumber << std::endl;

	std::unique_ptr<u128[]> perTopSubResult = mergeResultsAndValidationForFinalBuffer(Variables, allMBFData.allNodes, allMBFData.allClassInfos, sortedBetaSumPairs, betaResults.validationBuffer);
	

	u192 dedekindNumberFromValidator = computeDedekindNumberFromStandardBetaTopSums(allMBFData, perTopSubResult.get());
	std::cout << "D(" << (Variables + 2) << ") (validator) = " << dedekindNumberFromValidator << std::endl;
}

std::vector<NodeIndex> generateRangeSample(unsigned int Variables, NodeIndex sampleCount);
