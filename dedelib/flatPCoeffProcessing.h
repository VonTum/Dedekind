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
		ProcessedPCoeffSum* countConnectedSumBuf = subContext.resultBufferAlloc.pop_wait().value();
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

		ProcessedPCoeffSum* countConnectedSumBuf = subContext.resultBufferAlloc.pop_wait().value();
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
		int socket;
	};

	ProcessorData procData[NUMA_SLICE_COUNT];
	for(int i = 0; i < NUMA_SLICE_COUNT; i++) {
		procData[i].context = &context;
		procData[i].mbfs = static_cast<const Monotonic<Variables>*>(mbfs[i]);
		procData[i].socket = i;
	}

	auto processorFunc = [](void* voidData) -> void* {
		ProcessorData* procData = (ProcessorData*) voidData;

		setThreadName(("Processor " + std::to_string(procData->socket)).c_str());
		ThreadPool threadPool(CORES_PER_COMPLEX);

		PCoeffProcessingContext& context = *procData->context;

		int socket = procData->socket;
		PCoeffProcessingContextEighth& numaQueue = *context.numaQueues[socket];

		constexpr size_t BATCHER_SIZE = Variables < 7 ? 8 : 1;
		PopBatcher<JobInfo, BATCHER_SIZE> batcher;
		auto batchPopFunc = [&](JobInfo* buf) -> size_t {
			return context.inputQueue.popN_specific_wait(buf, BATCHER_SIZE, socket);
		};
		for(std::optional<JobInfo> jobOpt; (jobOpt = batcher.pop_wait(batchPopFunc)).has_value(); ) {
			JobInfo& job = jobOpt.value();
			ProcessedPCoeffSum* countConnectedSumBuf = numaQueue.resultBufferAlloc.pop_wait().value();
			auto startTime = std::chrono::high_resolution_clock::now();
			//shuffleBots(job.bufStart + 1, job.bufEnd);
			processBetasCPU_MultiThread(procData->mbfs, job, countConnectedSumBuf, threadPool);
			std::chrono::nanoseconds deltaTime = std::chrono::high_resolution_clock::now() - startTime;
			std::cout << 
				"Processed job " + std::to_string(job.getTop())
				 + " of " + std::to_string(job.getNumberOfBottoms() / 1000000.0)
				 + "M bottoms in " + std::to_string(std::chrono::duration_cast<std::chrono::seconds>(deltaTime).count()) + "s\n" << std::flush;
			OutputBuffer result;
			result.originalInputData = job;
			result.outputBuf = countConnectedSumBuf;
			numaQueue.outputQueue.push(result);
		}

		pthread_exit(nullptr);
		return nullptr;
	};

	PThreadsSpread coreComplexThreads(CORE_COMPLEX_COUNT, CPUAffinityType::COMPLEX, procData, processorFunc, 8);
	coreComplexThreads.join();
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

void loadNUMA_MBFs(unsigned int Variables, const void* mbfs[2]);

ResultProcessorOutput pcoeffPipeline(unsigned int Variables, std::future<std::vector<JobTopInfo>>& topIndices, void (*processorFunc)(PCoeffProcessingContext& context, const void* mbfs[2]), void(*validator)(const OutputBuffer&, const void*, ThreadPool&) = nullptr);

std::unique_ptr<u128[]> mergeResultsAndValidationForFinalBuffer(unsigned int Variables, const FlatNode* allMBFNodes, const ClassInfo* allClassInfos, const std::vector<BetaSumPair>& betaSums, const ValidationData* validationBuf);

template<unsigned int Variables>
void computeFinalDedekindNumberFromGatheredResults(const std::vector<BetaSumPair>& sortedBetaSumPairs, const ValidationData* validationBuffer) {
	std::cout << "Reading FlatMBFStructure..." << std::endl;
	const FlatMBFStructure<Variables> allMBFData = readFlatMBFStructure<Variables>();
	std::cout << "FlatMBFStructure initialized." << std::endl;

	std::cout << "Computation finished." << std::endl;
	u192 dedekindNumber = computeDedekindNumberFromBetaSums(allMBFData, sortedBetaSumPairs);
	std::cout << "D(" << (Variables + 2) << ") = " << dedekindNumber << std::endl;

	std::unique_ptr<u128[]> perTopSubResult = mergeResultsAndValidationForFinalBuffer(Variables, allMBFData.allNodes, allMBFData.allClassInfos, sortedBetaSumPairs, validationBuffer);
	
	u192 dedekindNumberFromValidator = computeDedekindNumberFromStandardBetaTopSums(allMBFData, perTopSubResult.get());
	std::cout << "D(" << (Variables + 2) << ") (validator) = " << dedekindNumberFromValidator << std::endl;
}

std::vector<JobTopInfo> loadAllTops(unsigned int Variables);

template<unsigned int Variables>
void processDedekindNumber(void (*processorFunc)(PCoeffProcessingContext& context, const void* mbfs[2]), void(*validator)(const OutputBuffer&, const void*, ThreadPool&) = nullptr) {
	std::future<std::vector<JobTopInfo>> allTopsFuture = std::async(loadAllTops, Variables);
	
	std::cout << "Starting Computation..." << std::endl;
	ResultProcessorOutput betaResults = pcoeffPipeline(Variables, allTopsFuture, processorFunc, validator);

	BetaResultCollector collector(Variables);
	collector.addBetaResults(betaResults.results);

	computeFinalDedekindNumberFromGatheredResults<Variables>(collector.getResultingSums(), betaResults.validationBuffer);
}

std::vector<NodeIndex> generateRangeSample(unsigned int Variables, NodeIndex sampleCount);
