#pragma once

#include <condition_variable>
#include <cstdint>
#include <vector>
#include <thread>
#include <queue>
#include <optional>
#include <functional>

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
void cpuProcessor_SingleThread_MBF(PCoeffProcessingContext& context, const Monotonic<Variables>* mbfs, int preferredNode = 0) {
	std::cout << "SingleThread CPU Processor started.\n" << std::flush;
	for(std::optional<JobInfo> jobOpt; (jobOpt = context.inputQueue.pop_wait_prefer(preferredNode)).has_value(); ) {
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
	struct ThreadData {
		PCoeffProcessingContext* context;
		const Monotonic<Variables>* mbfs;
		int numaNode;
	};

	ThreadData datas[2]{{&context, mbfs, 0}, {&context, mbfs, 1}};

	PThreadBundle threads = spreadThreads(std::thread::hardware_concurrency(), CPUAffinityType::CORE, datas, [](void* voidData) -> void* {
		ThreadData* data = (ThreadData*) voidData;
		cpuProcessor_SingleThread_MBF(*data->context, data->mbfs, data->numaNode);
		pthread_exit(nullptr);
		return nullptr;
	}, std::thread::hardware_concurrency() / 2);

	threads.join();
	std::cout << "Coarse MultiThread CPU Processor finished.\n" << std::flush;
}

template<unsigned int Variables>
void cpuProcessor_FineMultiThread_MBF(PCoeffProcessingContext& context, const Monotonic<Variables>* mbfs) {
	std::cout << "Fine MultiThread CPU Processor started.\n" << std::flush;
	ThreadPool pool;
	size_t lastNUMANode = 0;
	for(std::optional<JobInfo> jobOpt; (jobOpt = context.inputQueue.pop_wait_rotate(lastNUMANode)).has_value(); ) {
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
void cpuProcessor_SuperMultiThread(PCoeffProcessingContext& context) {
	// Assume 16 core complexes of 8 cores each
	constexpr int CORE_COMPLEX_COUNT = 16;
	constexpr int CORES_PER_COMPLEX = 8;
	constexpr int COMPLEXES_PER_SOCKET = CORE_COMPLEX_COUNT / NUMA_SLICE_COUNT;

	struct ProcessorData {
		PCoeffProcessingContext* context;
		const Monotonic<Variables>* mbfs;
		int coreComplex;
	};

	context.mbfsBothReady.wait();
	ProcessorData procData[CORE_COMPLEX_COUNT];
	for(int i = 0; i < CORE_COMPLEX_COUNT; i++) {
		procData[i].context = &context;
		procData[i].mbfs = static_cast<const Monotonic<Variables>*>(context.mbfs[i / COMPLEXES_PER_SOCKET]);
		procData[i].coreComplex = i;
	}

	auto processorFunc = [](void* voidData) -> void* {
		ProcessorData* procData = (ProcessorData*) voidData;

		setThreadName(("CPU " + std::to_string(procData->coreComplex)).c_str());
		ThreadPool threadPool(CORES_PER_COMPLEX);

		PCoeffProcessingContext& context = *procData->context;

		int socket = procData->coreComplex / COMPLEXES_PER_SOCKET;
		PCoeffProcessingContextEighth& numaQueue = *context.numaQueues[socket];

		for(std::optional<JobInfo> jobOpt; (jobOpt = context.inputQueue.pop_wait_prefer(socket)).has_value(); ) {
			JobInfo& job = jobOpt.value();
			ProcessedPCoeffSum* countConnectedSumBuf = numaQueue.resultBufferAlloc.pop_wait().value();
			auto startTime = std::chrono::high_resolution_clock::now();
			//shuffleBots(job.bufStart + 1, job.bufEnd);
			//processBetasCPU_SingleThread(procData->mbfs, job, countConnectedSumBuf);
			processBetasCPU_MultiThread(procData->mbfs, job, countConnectedSumBuf, threadPool);
			std::chrono::nanoseconds deltaTime = std::chrono::high_resolution_clock::now() - startTime;
			std::cout << 
				"CPU " + std::to_string(procData->coreComplex) + ": Processed job " + std::to_string(job.getTop())
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

	PThreadBundle coreComplexThreads = spreadThreads(CORE_COMPLEX_COUNT, CPUAffinityType::COMPLEX, procData, processorFunc);
	coreComplexThreads.join();
}

template<unsigned int Variables>
void cpuProcessor_SingleThread(PCoeffProcessingContext& context) {
	context.mbfs0Ready.wait();
	cpuProcessor_SingleThread_MBF(context, static_cast<const Monotonic<Variables>*>(context.mbfs[0]));
}
template<unsigned int Variables>
void cpuProcessor_CoarseMultiThread(PCoeffProcessingContext& context) {
	context.mbfs0Ready.wait();
	cpuProcessor_CoarseMultiThread_MBF(context, static_cast<const Monotonic<Variables>*>(context.mbfs[0]));
}
template<unsigned int Variables>
void cpuProcessor_FineMultiThread(PCoeffProcessingContext& context) {
	context.mbfs0Ready.wait();
	cpuProcessor_FineMultiThread_MBF(context, static_cast<const Monotonic<Variables>*>(context.mbfs[0]));
}

ResultProcessorOutput pcoeffPipeline(unsigned int Variables, const std::function<std::vector<JobTopInfo>()>& topLoader, void (*processorFunc)(PCoeffProcessingContext& context), void*(*validator)(void*) = nullptr);

std::unique_ptr<u128[]> mergeResultsAndValidationForFinalBuffer(unsigned int Variables, const std::vector<BetaSumPair>& betaSums, const ValidationData* validationBuf);

std::vector<JobTopInfo> loadAllTops(unsigned int Variables);

std::vector<NodeIndex> generateRangeSample(unsigned int Variables, NodeIndex sampleCount);

void computeFinalDedekindNumberFromGatheredResults(unsigned int Variables, const std::vector<BetaSumPair>& sortedBetaSumPairs, const ValidationData* validationBuffer);
void processDedekindNumber(unsigned int Variables, void (*processorFunc)(PCoeffProcessingContext& context), void*(*validator)(void*) = nullptr);
