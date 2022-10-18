#include "flatPCoeffProcessing.h"

#include "resultCollection.h"
#include <string.h>
#include <optional>

#include "latch.h"
#include "pcoeffValidator.h"

constexpr size_t BOTTOM_BUF_CREATOR_COUNT = 16;
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

static void loadNUMA_MBFs(unsigned int Variables, const void* mbfs[2]) {
	void* numaMBFBuffers[2];
	size_t mbfSize = (1 << (Variables > 3 ? Variables-3 : 0)); // sizeof(Monotonic<Variables>)
	size_t mbfBufSize = mbfSize * mbfCounts[Variables];
	allocSocketBuffers(mbfBufSize, numaMBFBuffers);
	readFlatVoidBufferNoMMAP(FileName::flatMBFs(Variables), mbfBufSize, numaMBFBuffers[1]);
	memcpy(numaMBFBuffers[0], numaMBFBuffers[1], mbfBufSize);

	mbfs[0] = numaMBFBuffers[0];
	mbfs[1] = numaMBFBuffers[1];
}
static void freeNUMA_MBFs(unsigned int Variables, const void* mbfs[2]) {
	size_t mbfSize = (1 << (Variables > 3 ? Variables-3 : 0)); // sizeof(Monotonic<Variables>)
	size_t mbfBufSize = mbfSize * mbfCounts[Variables];

	numa_free(const_cast<void*>(mbfs[0]), mbfBufSize);
	numa_free(const_cast<void*>(mbfs[1]), mbfBufSize);
}

ResultProcessorOutput pcoeffPipeline(unsigned int Variables, const std::function<std::vector<JobTopInfo>()>& topLoader, void (*processorFunc)(PCoeffProcessingContext&, const void*[2]), void*(*validator)(void*)) {
	//setThreadName("Main Thread");
	PCoeffProcessingContext context(Variables);

	std::thread inputProducerThread([&]() {
		setThreadName("InputProducer");
		runBottomBufferCreator(Variables, context, BOTTOM_BUF_CREATOR_COUNT);
	});
	

	// load the MBF buffers for processor and result verifier
	
	const void* mbfs[2];
	loadNUMA_MBFs(Variables, mbfs);

	std::thread processorThread([&]() {
		setThreadName("Processor");
		processorFunc(context, mbfs);
		for(int numaNode = 0; numaNode < NUMA_SLICE_COUNT; numaNode++) {
			context.numaQueues[numaNode]->outputQueue.close();
		}
	});
	
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

	ValidatorThreadData validatorDatas[16];

	PThreadBundle validatorThreads;
	if(validator != nullptr) {
		for(int i = 0; i < 16; i++) {
			validatorDatas[i].context = context.numaQueues[i / 8].ptr;
			validatorDatas[i].mbfs = mbfs[i / 8];
			validatorDatas[i].complexI = i;
		}
		validatorThreads = spreadThreads(16, CPUAffinityType::COMPLEX, validatorDatas, validator);
	} else {
		std::cout << "***** No validation selected! ******\n" << std::endl;

		for(int i = 0; i < NUMA_SLICE_COUNT; i++) {
			validatorDatas[i].context = context.numaQueues[i].ptr;
			validatorDatas[i].mbfs = mbfs[i];
			validatorDatas[i].complexI = i;
		}
		validatorThreads = spreadThreads(NUMA_SLICE_COUNT, CPUAffinityType::SOCKET, validatorDatas, noValidatorPThread);
	}

	ResultProcessorOutput results = NUMAResultProcessor(Variables, context, topLoader);
	assert(results.results.size() == context.tops.size());

	inputProducerThread.join();
	processorThread.join();
	queueWatchdogThread.join();
	validatorThreads.join();

	freeNUMA_MBFs(Variables, mbfs);

	return results;
}

std::unique_ptr<u128[]> mergeResultsAndValidationForFinalBuffer(unsigned int Variables, const FlatNode* allMBFNodes, const ClassInfo* allClassInfos, const std::vector<BetaSumPair>& betaSums, const ValidationData* validationBuf) {
	std::unique_ptr<u128[]> finalResults(new u128[mbfCounts[Variables]]);

	size_t validationBufferNonZeroSize = VALIDATION_BUFFER_SIZE(Variables);

	for(size_t i = 0; i < mbfCounts[Variables]; i++) {
		uint32_t dualNode = allMBFNodes[i].dual;
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

	return finalResults;
}
