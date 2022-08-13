#include "flatPCoeffProcessing.h"

#include "aligned_alloc.h"

// Try for at least 2MB huge pages
// Also alignment is required for openCL buffer sending and receiving methods
constexpr size_t ALLOC_ALIGN = 1 << 21;

static size_t getAlignedBufferSize(unsigned int Variables) {
	return alignUpTo(getMaxDeduplicatedBufferSize(Variables)+20, size_t(32) * 16);
}

PCoeffProcessingContext::PCoeffProcessingContext(unsigned int Variables, size_t numberOfNUMANodes, size_t numberOfInputBuffersPerNUMANode, size_t numOutputBuffers) :
	inputQueue(numberOfNUMANodes, numberOfInputBuffersPerNUMANode),
	outputQueue(std::min(numberOfNUMANodes * numberOfInputBuffersPerNUMANode, numOutputBuffers)),
	inputBufferAllocator(getAlignedBufferSize(Variables), numberOfInputBuffersPerNUMANode, numberOfNUMANodes),
	outputBufferReturnQueue(numOutputBuffers) {
	
	std::cout 
		<< "Create PCoeffProcessingContext with " 
		<< Variables 
		<< " Variables, " 
		<< numberOfNUMANodes 
		<< " NUMA nodes, " 
		<< numberOfInputBuffersPerNUMANode
		<< " buffers / NUMA node, and "
		<< numOutputBuffers
		<< " output buffers" << std::endl;

	std::cout << "Allocating " << numOutputBuffers << " output buffers." << std::endl;
	for(size_t i = 0; i < numOutputBuffers; i++) {
		outputBufferReturnQueue.push(static_cast<ProcessedPCoeffSum*>(aligned_malloc(sizeof(ProcessedPCoeffSum) * MAX_BUFSIZE(Variables), ALLOC_ALIGN)));
	}
}

PCoeffProcessingContext::~PCoeffProcessingContext() {
	std::cout << "Deleting output buffers..." << std::endl;
	size_t numFreedOutputBuffers = 0;
	for(size_t i = 0; i < outputBufferReturnQueue.sz; i++) {
		aligned_free(outputBufferReturnQueue[i]);
	}
	std::cout << "Deleted " << outputBufferReturnQueue.sz << " output buffers. " << std::endl;
	std::cout << "Deleting input buffers..." << std::endl;
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



std::vector<BetaResult> pcoeffPipeline(unsigned int Variables, const std::vector<NodeIndex>& topIndices, void (*processorFunc)(PCoeffProcessingContext&)) {
	constexpr size_t BOTTOM_BUF_CREATOR_COUNT = 6;
	PCoeffProcessingContext context(Variables, (BOTTOM_BUF_CREATOR_COUNT+1) / 2, 50, 20);

	std::vector<BetaResult> results;
	results.reserve(topIndices.size());

	std::thread inputProducerThread([&]() {
		try {
			runBottomBufferCreator(Variables, topIndices, context.inputQueue, context.inputBufferAllocator, BOTTOM_BUF_CREATOR_COUNT);
		} catch(const char* errText) {
			std::cerr << "Error thrown in inputProducerThread: " << errText;
			exit(-1);
		}
	});

	std::thread resultProcessingThread([&]() {
		try {
			resultProcessor(Variables, context.outputQueue, context.inputBufferAllocator, context.outputBufferReturnQueue, results);
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

	inputProducerThread.join();
	//context.inputQueue.close();

	processorThread.join();
	context.outputQueue.close();

	resultProcessingThread.join();
	queueWatchdogThread.join();


	assert(results.size() == topIndices.size());

	return results;
}

