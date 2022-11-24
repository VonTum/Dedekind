#include "flatPCoeffProcessing.h"

#include "resultCollection.h"
#include <string.h>
#include <optional>

#include "latch.h"
#include "pcoeffValidator.h"

constexpr size_t NUM_RESULT_VALIDATORS = 16;
constexpr int MAX_VALIDATOR_COUNT = 16;


uint8_t reverseBits(uint8_t index) {
	index = ((index & 0b00001111) << 4) | ((index & 0b11110000) >> 4); // swap 4,4
	index = ((index & 0b00110011) << 2) | ((index & 0b11001100) >> 2); // swap 2,2
	index = ((index & 0b01010101) << 1) | ((index & 0b10101010) >> 1); // swap 1,1
	return index;
}

// Deterministically shuffles the input bots to get a more uniform mix of bot difficulty
void shuffleBots(NodeIndex* bots, NodeIndex* botsEnd) {
	constexpr size_t CLUSTER_SIZE = 256;
	using IndexType = uint8_t;
	size_t numBots = botsEnd - bots;

	size_t numGroups = numBots / CLUSTER_SIZE;

	for(size_t groupI = 0; groupI < numGroups; groupI++) {
		NodeIndex itemsFromCluster[CLUSTER_SIZE];
		for(size_t itemWithinCluster = 0; itemWithinCluster < CLUSTER_SIZE; itemWithinCluster++) {
			itemsFromCluster[itemWithinCluster] = bots[numGroups * itemWithinCluster + groupI];
		}
		for(size_t itemWithinCluster = 0; itemWithinCluster < CLUSTER_SIZE; itemWithinCluster++) {
			IndexType sourceIndex = reverseBits(static_cast<IndexType>((itemWithinCluster + groupI / 16) % CLUSTER_SIZE)); // Try to keep indices somewhat consecutive for better memory pattern
			bots[numGroups * itemWithinCluster + groupI] = itemsFromCluster[sourceIndex];
		}
	}
}

std::vector<JobTopInfo> loadAllTops(unsigned int Variables) {
	std::vector<JobTopInfo> topsToProcess;
	const FlatNode* allNodes = readFlatBuffer<FlatNode>(FileName::flatNodes(Variables), mbfCounts[Variables]);
	for(NodeIndex i = 0; i < mbfCounts[Variables]; i++) {
		JobTopInfo newTopInfo;
		newTopInfo.top = i;
		newTopInfo.topDual = allNodes[i].dual;
		topsToProcess.push_back(newTopInfo);
	}
	freeFlatBuffer(allNodes, mbfCounts[Variables]);
	//shuffleBots(&topsToProcess[0], (&topsToProcess[0]) + topsToProcess.size());
	//free(allNodes);

	return topsToProcess;
}

std::vector<NodeIndex> generateRangeSample(unsigned int Variables, NodeIndex sampleCount) {
	std::vector<NodeIndex> resultingVector;
	for(NodeIndex i = 0; i < sampleCount; i++) {
		double expectedIndex = mbfCounts[Variables] * (i+0.5) / sampleCount;
		resultingVector.push_back(static_cast<NodeIndex>(expectedIndex));
	}
	return resultingVector;
}

ResultProcessorOutput pcoeffPipeline(unsigned int Variables, const std::function<std::vector<JobTopInfo>()>& topLoader, void (*processorFunc)(PCoeffProcessingContext&), void*(*validator)(void*), const std::function<void(const OutputBuffer&, const char*)>& errorBufFunc) {
	setNUMANodeAffinity(0); // Fopr buffer loading, use the ethernet socket on node 0, to save bandwidth for big flatLinksBuffer on socket 4. 
	setThreadName("Main Thread");
	// Alloc on node 3, because that's where the FPGA processor is located too. We want as low latency from it to the context
	unique_numa_ptr<PCoeffProcessingContext> contextPtr = unique_numa_ptr<PCoeffProcessingContext>::alloc_onnode(3, Variables);
	PCoeffProcessingContext& context = *contextPtr;

	// Set it on second socket, because that is where 2 ethernet links are for faster buffer download
	pthread_t inputProducerThread = createNUMANodePThread(4, [](void* voidContext) -> void* {
		PCoeffProcessingContext* typedContext = (PCoeffProcessingContext*) voidContext;
		setThreadName("InputProducer");
		runBottomBufferCreator(typedContext->Variables, *typedContext);
		pthread_exit(nullptr);
		return nullptr;
	}, &context);
	
	struct ProcData {
		PCoeffProcessingContext* context;
		void (*processorFunc)(PCoeffProcessingContext&);
	};
	ProcData procData;
	procData.context = &context;
	procData.processorFunc = processorFunc;

	// FPGAs are on nodes 3 and 7, context is on 3, so start main thread on node 3
	pthread_t processorThread = createNUMANodePThread(3, [](void* voidProcData) -> void* {
		setThreadName("Processor");
		ProcData* procData = (ProcData*) voidProcData;

		procData->processorFunc(*procData->context);
		for(int numaNode = 0; numaNode < NUMA_SLICE_COUNT; numaNode++) {
			procData->context->numaQueues[numaNode]->outputQueue.close();
		}

		pthread_exit(nullptr);
		return nullptr;
	}, &procData);
	
	std::thread queueWatchdogThread([&](){
		setThreadName("Queue Watchdog");
		while(!context.inputQueue.isClosed) {
			std::string totalString = "\033[34m[Queues]:";
			for(int numaNode = 0; numaNode < NUMA_SLICE_COUNT; numaNode++) {
				PCoeffProcessingContextEighth& subContext = *context.numaQueues[numaNode];

				totalString += "\n" + std::to_string(numaNode)
				 + "> i:" + std::to_string(context.inputQueue.queues[numaNode].size())
				 + " o:" + std::to_string(subContext.outputQueue.size())
				 + " v:" + std::to_string(subContext.validationQueue.size())
				 + " / i:" + std::to_string(subContext.inputBufferAlloc.size())
				 + " r:" + std::to_string(subContext.resultBufferAlloc.size());
			}
			totalString += "\033[39m\n";

			std::cout << totalString << std::flush;
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	});

	std::cout << "\033[32m[Result Processor] Started loading MBFS...\033[39m\n" << std::flush;
	context.initMBFS();
	std::cout << "\033[32m[Result Processor] Finished loading MBFS...\n[Result Processor] Started loading job tops...\033[39m\n" << std::flush;
	context.initTops(topLoader());
	std::cout << "\033[32m[Result Processor] Finished loading job tops...\n[Result Processor] Started loading ClassInfos...\033[39m\n" << std::flush;

	ValidatorThreadData validatorDatas[16];
	PThreadBundle validatorThreads;
	if(validator != nullptr) {
		for(int i = 0; i < 16; i++) {
			validatorDatas[i].context = context.numaQueues[i / 8].ptr;
			validatorDatas[i].mbfs = context.mbfs[i / 8];
			validatorDatas[i].complexI = i;
			validatorDatas[i].errorBufFunc = &errorBufFunc;
		}
		validatorThreads = spreadThreads(16, CPUAffinityType::COMPLEX, validatorDatas, validator);
	} else {
		std::cout << "***** No validation selected! ******\n" << std::endl;

		for(int i = 0; i < NUMA_SLICE_COUNT; i++) {
			validatorDatas[i].context = context.numaQueues[i].ptr;
			validatorDatas[i].mbfs = context.mbfs[i];
			validatorDatas[i].complexI = i;
			validatorDatas[i].errorBufFunc = &errorBufFunc;
		}
		validatorThreads = spreadThreads(NUMA_SLICE_COUNT, CPUAffinityType::SOCKET, validatorDatas, noValidatorPThread);
	}

	ResultProcessorOutput results = NUMAResultProcessor(Variables, context, errorBufFunc);
	assert(results.results.size() == context.tops.size());

	pthread_join(inputProducerThread, nullptr);
	pthread_join(processorThread, nullptr);
	queueWatchdogThread.join();
	validatorThreads.join();

	return results;
}

std::unique_ptr<u128[]> mergeResultsAndValidationForFinalBuffer(unsigned int Variables, const std::vector<BetaSumPair>& betaSums, const ValidationData* validationBuf) {
	const FlatNode* allNodes = readFlatBuffer<FlatNode>(FileName::flatNodes(Variables), mbfCounts[Variables]);
	const ClassInfo* allClassInfos = readFlatBuffer<ClassInfo>(FileName::flatClassInfo(Variables), mbfCounts[Variables]);

	
	std::unique_ptr<u128[]> finalResults(new u128[mbfCounts[Variables]]);

	size_t validationBufferNonZeroSize = VALIDATION_BUFFER_SIZE(Variables);

	for(size_t i = 0; i < mbfCounts[Variables]; i++) {
		uint32_t dualNode = allNodes[i].dual;
		ValidationData validationTerm;
		if(dualNode < validationBufferNonZeroSize) {
			validationTerm = validationBuf[dualNode];
		} else {
			validationTerm.dualBetaSum.betaSum = 0;
			validationTerm.dualBetaSum.countedIntervalSizeDown = 0;
		}

		/*std::cout << i
		 << ": (" << validationTerm.dualBetaSum.betaSum
		 << " ; " << validationTerm.dualBetaSum.countedIntervalSizeDown
		 << ") : (" << betaSums[i].betaSum.betaSum
		 << " ; " << betaSums[i].betaSum.countedIntervalSizeDown
		 << ") : (" << betaSums[i].betaSumDualDedup.betaSum
		 << " ; " << betaSums[i].betaSumDualDedup.countedIntervalSizeDown
		 << ")\n" << std::flush;*/

		BetaSum totalSumForThisTop = betaSums[i].getBetaSumPlusValidationTerm(Variables, validationTerm.dualBetaSum);

		if(totalSumForThisTop.countedIntervalSizeDown != allClassInfos[i].intervalSizeDown) {
			std::cerr << "Incorrect total interval size down for top " << i << " should be " << allClassInfos[i].intervalSizeDown << " but found " << totalSumForThisTop.countedIntervalSizeDown << "! Aborting!" << std::endl;
			std::abort();
		}

		finalResults[i] = totalSumForThisTop.betaSum;
	}

	std::cout << "\033[35m[Validation] All interval sizes checked, no errors found!\033[39m\n" << std::flush;

	freeFlatBuffer<FlatNode>(allNodes, mbfCounts[Variables]);
	freeFlatBuffer<ClassInfo>(allClassInfos, mbfCounts[Variables]);

	return finalResults;
}

void computeFinalDedekindNumberFromGatheredResults(unsigned int Variables, const std::vector<BetaSumPair>& sortedBetaSumPairs, const ValidationData* validationBuffer) {
	std::cout << "Computation finished." << std::endl;
	u192 dedekindNumber = computeDedekindNumberFromBetaSums(Variables, sortedBetaSumPairs);
	std::cout << "D(" << (Variables + 2) << ") = " << toString(dedekindNumber) << std::endl;

	std::unique_ptr<u128[]> perTopSubResult = mergeResultsAndValidationForFinalBuffer(Variables, sortedBetaSumPairs, validationBuffer);
	
	u192 dedekindNumberFromValidator = computeDedekindNumberFromStandardBetaTopSums(Variables, perTopSubResult.get());
	std::cout << "D(" << (Variables + 2) << ") (validator) = " << toString(dedekindNumberFromValidator) << std::endl;
}

void processDedekindNumber(unsigned int Variables, void (*processorFunc)(PCoeffProcessingContext& context), void*(*validator)(void*)) {
	std::cout << "Starting Computation..." << std::endl;
	ResultProcessorOutput betaResults = pcoeffPipeline(Variables, [Variables]() -> std::vector<JobTopInfo> {return loadAllTops(Variables);}, processorFunc, validator, 
		[](const OutputBuffer& outBuf, const char* name){
			std::cerr << "Error from " + std::string(name) + " of top " + std::to_string(outBuf.originalInputData.getTop()) + "\n" << std::flush;
			std::abort();
		}
	);

	BetaResultCollector collector(Variables);
	collector.addBetaResults(betaResults.results);

	computeFinalDedekindNumberFromGatheredResults(Variables, collector.getResultingSums(), betaResults.validationBuffer);

	numa_free(betaResults.validationBuffer, VALIDATION_BUFFER_SIZE(Variables) * sizeof(ValidationData));
}
