#include "flatPCoeffProcessing.h"

#include "aligned_alloc.h"

// Try for at least 2MB huge pages
// Also alignment is required for openCL buffer sending and receiving methods
constexpr size_t ALLOC_ALIGN = 1 << 21;

PCoeffProcessingContext::PCoeffProcessingContext(unsigned int Variables, size_t numberOfInputBuffers, size_t minOutputBuffers, size_t maxOutputBuffers) :
	inputQueue(numberOfInputBuffers),
	outputQueue(std::min(numberOfInputBuffers, maxOutputBuffers)),
	inputBufferReturnQueue(),
	outputBufferReturnQueue(minOutputBuffers * MAX_BUFSIZE(Variables), maxOutputBuffers, ALLOC_ALIGN) {
	std::cout << "Allocating " << numberOfInputBuffers << " input buffers." << std::endl;
	for(size_t i = 0; i < numberOfInputBuffers; i++) {
		inputBufferReturnQueue.push(static_cast<NodeIndex*>(aligned_malloc(sizeof(NodeIndex) * MAX_BUFSIZE(Variables), ALLOC_ALIGN)));
	}
	std::cout << "Allocated space for " << minOutputBuffers << ".." << maxOutputBuffers << " output buffers." << std::endl;
}
PCoeffProcessingContext::~PCoeffProcessingContext() {
	std::cout << "Deleting input buffers..." << std::endl;
	size_t numFreedInputBuffers = 0;
	auto& inputBufferReturnStackContainer = inputBufferReturnQueue.get();
	while(!inputBufferReturnStackContainer.empty()) {
		aligned_free(inputBufferReturnStackContainer.top());
		inputBufferReturnStackContainer.pop();
		numFreedInputBuffers++;
	}
	std::cout << "Deleted " << numFreedInputBuffers << " input buffers. " << std::endl;

	std::cout << "Deleting output buffers..." << std::endl;
	size_t numFreedOutputBuffers = 0;
	auto& outputBufferReturnStackContainer = outputBufferReturnQueue.get();
	if(outputBufferReturnStackContainer.getNumberOfChunksInUse() != 0) {
		std::cerr << "ERROR: " << outputBufferReturnStackContainer.getNumberOfChunksInUse() << " output buffers not returned!\n";
		exit(-1); 
	}
}

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



std::vector<BetaResult> pcoeffPipeline(unsigned int Variables, const std::vector<NodeIndex>& topIndices, void (*processorFunc)(PCoeffProcessingContext&), size_t numberOfInputBuffers, size_t minOutputBuffers, size_t maxOutputBuffers) {
	PCoeffProcessingContext context(Variables, numberOfInputBuffers, minOutputBuffers, maxOutputBuffers);

	std::vector<BetaResult> results;
	results.reserve(topIndices.size());

	std::thread inputProducerThread([&]() {
		try {
			runBottomBufferCreator(Variables, topIndices, context.inputQueue, context.inputBufferReturnQueue, 12);
		} catch(const char* errText) {
			std::cerr << "Error thrown in inputProducerThread: " << errText;
			exit(-1);
		}
	});

	std::thread resultProcessingThread([&]() {
		try {
			resultProcessor(Variables, context.outputQueue, context.inputBufferReturnQueue, context.outputBufferReturnQueue, results);
		} catch(const char* errText) {
			std::cerr << "Error thrown in resultProcessingThread: " << errText;
			exit(-1);
		}
	});
	std::thread processorThread([&]() {
		try {
			processorFunc(context);
		} catch(const char* errText) {
			std::cerr << "Error thrown in processorThread: " << errText;
			exit(-1);
		}
	});
	
	std::thread queueWatchdogThread([&](){
		while(!context.outputQueue.queueHasBeenClosed()) {
			std::cout << "\033[34m[Queues] (" 
				+ std::to_string(context.inputQueue.size()) + ") -> R(" 
				+ std::to_string(context.inputBufferReturnQueue.size()) + ") -> ("
				+ std::to_string(context.outputQueue.size()) + " / " 
				+ std::to_string(context.outputBufferReturnQueue.get().getNumberOfChunksInUse()) + ")\033[39m\n" << std::flush;
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	});

	inputProducerThread.join();
	context.inputQueue.close();

	processorThread.join();
	context.outputQueue.close();

	resultProcessingThread.join();
	queueWatchdogThread.join();


	assert(results.size() == topIndices.size());

	return results;
}

