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
	size_t inputQueueRotation = 0;
	for(std::optional<JobInfo> jobOpt; (jobOpt = context.inputQueue.pop_wait(inputQueueRotation)).has_value(); ) {
		JobInfo& job = jobOpt.value();

		//shuffleBots(job.bufStart + 1, job.bufEnd);
		//std::cout << "Grabbed job of size " << job.bufferSize() << '\n' << std::flush;
		ProcessedPCoeffSum* countConnectedSumBuf = context.outputBufferReturnQueue.pop_wait();
		//std::cout << "Grabbed output buffer.\n" << std::flush;
		processBetasCPU_SingleThread(mbfs, job, countConnectedSumBuf);
		OutputBuffer result;
		result.originalInputData = job;
		result.outputBuf = countConnectedSumBuf;
		context.outputQueue.push(result);
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
	size_t inputQueueRotation = 0;
	for(std::optional<JobInfo> jobOpt; (jobOpt = context.inputQueue.pop_wait(inputQueueRotation)).has_value(); ) {
		JobInfo& job = jobOpt.value();
		ProcessedPCoeffSum* countConnectedSumBuf = context.outputBufferReturnQueue.pop_wait();
		//shuffleBots(job.bufStart + 1, job.bufEnd);
		processBetasCPU_MultiThread(mbfs, job, countConnectedSumBuf, pool);
		OutputBuffer result;
		result.originalInputData = job;
		result.outputBuf = countConnectedSumBuf;
		context.outputQueue.push(result);
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
	};

	pthread_t complexMainThreads[CORE_COMPLEX_COUNT];
	ProcessorData procData[2];
	procData[0].context = &context;
	procData[0].mbfs = static_cast<const Monotonic<Variables>*>(mbfs[0]);
	procData[1].context = &context;
	procData[1].mbfs = static_cast<const Monotonic<Variables>*>(mbfs[1]);

	/*cpuProcessor_FineMultiThread_MBF<Variables>(context, procData[1].mbfs);
	return;*/

	auto processorFunc = [](void* voidData) -> void* {
		ThreadPool threadPool(CORES_PER_COMPLEX);

		ProcessorData* procData = (ProcessorData*) voidData;
		PCoeffProcessingContext& context = *procData->context;

		size_t inputQueueRotation = 0;
		for(std::optional<JobInfo> jobOpt; (jobOpt = context.inputQueue.pop_wait(inputQueueRotation)).has_value(); ) {
			JobInfo& job = jobOpt.value();
			ProcessedPCoeffSum* countConnectedSumBuf = context.outputBufferReturnQueue.pop_wait();
			//shuffleBots(job.bufStart + 1, job.bufEnd);
			processBetasCPU_MultiThread(procData->mbfs, job, countConnectedSumBuf, threadPool);
			OutputBuffer result;
			result.originalInputData = job;
			result.outputBuf = countConnectedSumBuf;
			context.outputQueue.push(result);
		}

		pthread_exit(nullptr);
		return nullptr;
	};

	for(int coreComplex = 0; coreComplex < CORE_COMPLEX_COUNT; coreComplex++) {
		int socketI = coreComplex / 8;
		complexMainThreads[coreComplex] = createCoreComplexPThread(coreComplex, processorFunc, &procData[0]);
	}

	for(int coreComplex = 0; coreComplex < CORE_COMPLEX_COUNT; coreComplex++) {
		pthread_join(complexMainThreads[coreComplex], nullptr);
	}
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

std::vector<BetaResult> pcoeffPipeline(unsigned int Variables, const std::vector<NodeIndex>& topIndices, void (*processorFunc)(PCoeffProcessingContext& context, const void* mbfs[2]), void(*validator)(const OutputBuffer&, const void*) = [](const OutputBuffer&, const void*){});

// Requires a Processor function of type void(PCoeffProcessingContext& context, const void* mbfsNUMA[2])
template<unsigned int Variables, typename Processor>
void processDedekindNumber(const Processor& processorFunc) {
	std::vector<NodeIndex> topsToProcess;
	for(NodeIndex i = 0; i < mbfCounts[Variables]; i++) {
		topsToProcess.push_back(i);
	}

	shuffleBots(&topsToProcess[0], (&topsToProcess[0]) + topsToProcess.size());

	std::cout << "Starting Computation..." << std::endl;
	std::vector<BetaResult> betaResults = pcoeffPipeline(Variables, topsToProcess, processorFunc);

	BetaResultCollector collector(Variables);
	collector.addBetaResults(betaResults);

	std::cout << "Reading FlatMBFStructure..." << std::endl;
	const FlatMBFStructure<Variables> allMBFData = readFlatMBFStructure<Variables>();
	std::cout << "FlatMBFStructure initialized." << std::endl;

	std::cout << "Computation finished." << std::endl;
	u192 dedekindNumber = computeDedekindNumberFromBetaSums(allMBFData, collector.getResultingSums());

	std::cout << "D(" << (Variables + 2) << ") = " << dedekindNumber << std::endl;
}

std::vector<NodeIndex> generateRangeSample(unsigned int Variables, NodeIndex sampleCount);
