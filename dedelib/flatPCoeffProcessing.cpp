#include "flatPCoeffProcessing.h"

#include "resultCollection.h"
#include <string.h>
#include <optional>

#include "latch.h"

constexpr size_t BOTTOM_BUF_CREATOR_COUNT = 16;
constexpr size_t NUM_RESULT_VALIDATORS = 16;
constexpr int MAX_VALIDATOR_COUNT = 16;
constexpr size_t NUM_VALIDATOR_THREADS_PER_COMPLEX = 5;


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

std::vector<NodeIndex> generateRangeSample(unsigned int Variables, NodeIndex sampleCount) {
	std::vector<NodeIndex> resultingVector;
	for(NodeIndex i = 0; i < sampleCount; i++) {
		double expectedIndex = mbfCounts[Variables] * (i+0.5) / sampleCount;
		resultingVector.push_back(static_cast<NodeIndex>(expectedIndex));
	}
	return resultingVector;
}

// Expects mbfs0 to be stored in memory of socket0, and mbfs1 to have memory of socket1
static void validatorThread(
	void(*validator)(const OutputBuffer&, const void*, ThreadPool&), 
	PCoeffProcessingContextEighth& context,
	const void* validatorData
) {
	ThreadPool pool(NUM_VALIDATOR_THREADS_PER_COMPLEX);
	for(std::optional<OutputBuffer> outputBuffer; (outputBuffer = context.validationQueue.pop_wait()).has_value(); ) {
		OutputBuffer outBuf = outputBuffer.value();

		validator(outBuf, validatorData, pool);

		context.inputBufferAlloc.push(outBuf.originalInputData.bufStart);
		context.resultBufferAlloc.push(outBuf.outputBuf);
	}
}

// Expects mbfs0 to be stored in memory of socket0, and mbfs1 to have memory of socket1
static void noValidatorThread(
	PCoeffProcessingContextEighth& context
) {
	for(std::optional<OutputBuffer> outputBuffer; (outputBuffer = context.validationQueue.pop_wait()).has_value(); ) {
		OutputBuffer outBuf = outputBuffer.value();

		context.inputBufferAlloc.push(outBuf.originalInputData.bufStart);
		context.resultBufferAlloc.push(outBuf.outputBuf);
	}
}

void loadNUMA_MBFs(unsigned int Variables, const void* mbfs[2]) {
	void* numaMBFBuffers[2];
	size_t mbfSize = (1 << (Variables > 3 ? Variables-3 : 0)); // sizeof(Monotonic<Variables>)
	size_t mbfBufSize = mbfSize * mbfCounts[Variables];
	allocSocketBuffers(mbfBufSize, numaMBFBuffers);
	readFlatVoidBufferNoMMAP(FileName::flatMBFs(Variables), mbfBufSize, numaMBFBuffers[1]);
	memcpy(numaMBFBuffers[0], numaMBFBuffers[1], mbfBufSize);

	mbfs[0] = numaMBFBuffers[0];
	mbfs[1] = numaMBFBuffers[1];
}

ResultProcessorOutput pcoeffPipeline(unsigned int Variables, const std::vector<NodeIndex>& topIndices, void (*processorFunc)(PCoeffProcessingContext&, const void*[2]), void(*validator)(const OutputBuffer&, const void*, ThreadPool&)) {
	//setThreadName("Main Thread");
	PCoeffProcessingContext context(Variables);

	Latch processingHasStarted(1); // BottomBufferCreator and (not) processor

	std::thread inputProducerThread([&]() {
		setThreadName("InputProducer");
		try {
			runBottomBufferCreator(Variables, topIndices, context, BOTTOM_BUF_CREATOR_COUNT, &processingHasStarted);
			context.inputQueue.close();
		} catch(const char* errText) {
			std::cerr << "Error thrown in inputProducerThread: " << errText;
			exit(-1);
		}
	});
	

	// load the MBF buffers for processor and result verifier
	
	const void* mbfs[2];
	loadNUMA_MBFs(Variables, mbfs);

	std::thread processorThread([&]() {
		setThreadName("Processor");
		try {
			processorFunc(context, mbfs);
			for(int numaNode = 0; numaNode < 8; numaNode++) {
				context.numaQueues[numaNode]->outputQueue.close();
			}
		} catch(const char* errText) {
			std::cerr << "Error thrown in processorThread: " << errText;
			exit(-1);
		}
	});
	
	processingHasStarted.wait();
	std::cout << "Basic initialization has started, begin processing!\n" << std::flush;

	std::thread queueWatchdogThread([&](){
		setThreadName("Queue Watchdog");
		while(!context.inputQueue.isClosed) {
			std::string totalString = "\033[34m[Queues]:";
			for(int numaNode = 0; numaNode < 8; numaNode++) {
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

	struct ValidatorThreadData{
		void(*validator)(const OutputBuffer&, const void*, ThreadPool&);
		PCoeffProcessingContextEighth* context;
		const void* mbfs;
		int numaNode;
	};
	ValidatorThreadData validatorDatas[8];
	for(int i = 0; i < 8; i++) {
		validatorDatas[i].validator = validator;
		validatorDatas[i].context = context.numaQueues[i].ptr;
		validatorDatas[i].mbfs = mbfs[i / 4];
		validatorDatas[i].numaNode = i;
	}

	PThreadsSpread validatorThreads;
	if(validator != nullptr) {
		validatorThreads = PThreadsSpread(16, CPUAffinityType::COMPLEX, validatorDatas, [](void* data) -> void* {
			ValidatorThreadData* validatorData = (ValidatorThreadData*) data;
			setThreadName(("Validator " + std::to_string(validatorData->numaNode)).c_str());
			validatorThread(validatorData->validator, *validatorData->context, validatorData->mbfs);
			pthread_exit(nullptr);
			return nullptr;
		}, 2);
	} else {
		std::cout << "***** No validation selected! ******\n" << std::endl;
		validatorThreads = PThreadsSpread(8, CPUAffinityType::NUMA_DOMAIN, validatorDatas, [](void* data) -> void* {
			ValidatorThreadData* validatorData = (ValidatorThreadData*) data;
			setThreadName(("NoValidator " + std::to_string(validatorData->numaNode)).c_str());
			noValidatorThread(*validatorData->context);
			pthread_exit(nullptr);
			return nullptr;
		});
	}


	ResultProcessorOutput results = NUMAResultProcessor(Variables, context, topIndices.size());

	inputProducerThread.join();
	processorThread.join();
	queueWatchdogThread.join();
	validatorThreads.join();

	assert(results.results.size() == topIndices.size());

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
