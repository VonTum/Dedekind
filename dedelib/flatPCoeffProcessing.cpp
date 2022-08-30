#include "flatPCoeffProcessing.h"

#include "resultCollection.h"

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



ResultProcessorOutput pcoeffPipeline(unsigned int Variables, const std::vector<NodeIndex>& topIndices, void (*processorFunc)(PCoeffProcessingContext&, const void*[2]), void(*validator)(const OutputBuffer&, const void*, ThreadPool&)) {
	constexpr size_t BOTTOM_BUF_CREATOR_COUNT = 16;
	constexpr size_t NUM_RESULT_VALIDATORS = 16;
	PCoeffProcessingContext context(Variables, (BOTTOM_BUF_CREATOR_COUNT+1) / 2, 50, Variables >= 7 ? 60 : 200);

	std::thread inputProducerThread([&]() {
		try {
			runBottomBufferCreator(Variables, topIndices, context.inputQueue, context.inputBufferAllocator, BOTTOM_BUF_CREATOR_COUNT);
			context.inputQueue.close();
		} catch(const char* errText) {
			std::cerr << "Error thrown in inputProducerThread: " << errText;
			exit(-1);
		}
	});

	// load the MBF buffers for processor and result verifier
	void* numaMBFBuffers[2];
	size_t mbfSize = (1 << (Variables > 3 ? Variables-3 : 0)); // sizeof(Monotonic<Variables>)
	size_t mbfBufSize = mbfSize * mbfCounts[Variables];
	allocSocketBuffers(mbfBufSize, numaMBFBuffers);
	readFlatVoidBufferNoMMAP(FileName::flatMBFs(Variables), mbfBufSize, numaMBFBuffers[1]);
	memcpy(numaMBFBuffers[0], numaMBFBuffers[1], mbfBufSize);

	const void* constMBFs[2]{numaMBFBuffers[0], numaMBFBuffers[1]};

	std::thread processorThread([&]() {
		try {
			processorFunc(context, constMBFs);
			context.outputQueue.close();
		} catch(const char* errText) {
			std::cerr << "Error thrown in processorThread: " << errText;
			exit(-1);
		}
	});
	
	std::thread queueWatchdogThread([&](){
		while(!context.outputQueue.queueHasBeenClosed()) {
			std::string totalString = 
				"\033[34m[Queues] Return(" 
				+ std::to_string(context.outputQueue.size()) 
				+ " / " + std::to_string(context.outputBufferReturnQueue.capacity() - context.outputBufferReturnQueue.size()) + ") Input(";
			for(size_t queueI = 0; queueI < context.inputBufferAllocator.slabs.size(); queueI++) {
				totalString += std::to_string(context.inputQueue.queues[queueI].size()) + " / " 
				+ std::to_string(context.inputBufferAllocator.queues[queueI].capacity() - context.inputBufferAllocator.queues[queueI].size()) + ", ";
			}
			totalString += ")\033[39m\n";
			std::cout << totalString << std::flush;
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	});

	//std::vector<BetaResult> results = resultProcessor(Variables, context, topIndices.size());
	ResultProcessorOutput results = NUMAResultProcessorWithValidator(Variables, context, topIndices.size(), NUM_RESULT_VALIDATORS, validator, constMBFs);

	inputProducerThread.join();
	processorThread.join();
	queueWatchdogThread.join();


	assert(results.results.size() == topIndices.size());

	return results;
}
