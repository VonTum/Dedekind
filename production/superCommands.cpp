#include "commands.h"

#include "../dedelib/supercomputerJobs.h"
#include "../dedelib/pcoeffValidator.h"
#include "../dedelib/singleTopVerification.h"

#include <sstream>
#include <filesystem>

template<unsigned int Variables>
static void processSuperComputingJob_FromArgs(const std::vector<std::string>& args, const char* name, void (*processorFunc)(PCoeffProcessingContext&)) {
	std::string projectFolderPath = args[0];
	std::string jobID = args[1];
	void*(*validator)(void*) = nullptr;
	if(args.size() >= 3) {
		if(args[2] == "validate_basic") {
			validator = basicValidatorPThread<Variables>;
		} else if(args[2] == "validate_adv") {
			validator = continuousValidatorPThread<Variables>;
		}
	}
	bool success = processJob(Variables, projectFolderPath, jobID, name, processorFunc, validator);

	if(!success) std::abort();
}

template<unsigned int Variables>
static void processSuperComputingJob_ST(const std::vector<std::string>& args) {
	processSuperComputingJob_FromArgs<Variables>(args, "cpuST", cpuProcessor_SingleThread<Variables>);
}

template<unsigned int Variables>
static void processSuperComputingJob_FMT(const std::vector<std::string>& args) {
	processSuperComputingJob_FromArgs<Variables>(args, "cpuFMT", cpuProcessor_FineMultiThread<Variables>);
}

template<unsigned int Variables>
static void processSuperComputingJob_SMT(const std::vector<std::string>& args) {
	processSuperComputingJob_FromArgs<Variables>(args, "cpuSMT", cpuProcessor_SuperMultiThread<Variables>);
}

template<unsigned int Variables>
void checkErrorBuffer(const std::vector<std::string>& args) {
	const std::string& fileName = args[0];

	NodeIndex* idxBuf = aligned_mallocT<NodeIndex>(mbfCounts[Variables], 4096);
	ProcessedPCoeffSum* resultBuf = aligned_mallocT<ProcessedPCoeffSum>(mbfCounts[Variables], 4096);
	ProcessedPCoeffSum* correctResultBuf = aligned_mallocT<ProcessedPCoeffSum>(mbfCounts[Variables], 4096);
	uint64_t bufSize = readProcessingBufferPairFromFile(fileName.c_str(), idxBuf, resultBuf);

	const Monotonic<Variables>* mbfs = readFlatBufferNoMMAP<Monotonic<Variables>>(FileName::flatMBFs(Variables), mbfCounts[Variables]);
	ThreadPool threadPool;
	JobInfo job;
	job.bufStart = idxBuf;
	job.bufEnd = idxBuf + bufSize;
	// Don't need job.blockEnd
	processBetasCPU_MultiThread(mbfs, job, correctResultBuf, threadPool);

	size_t firstErrorIdx;
	for(size_t i = 2; i < bufSize; i++) {
		if(resultBuf[i] != correctResultBuf[i]) {
			firstErrorIdx = i;
			goto errorFound;
		}
	}
	std::cout << std::to_string(bufSize) + " elements checked. All correct!" << std::flush;
	return;
	errorFound:
	size_t lastErrorIdx = firstErrorIdx;
	size_t numberOfErrors = 1;
	for(size_t i = firstErrorIdx+1; i < bufSize; i++) {
		if(resultBuf[i] != correctResultBuf[i]) {
			lastErrorIdx = i;
			numberOfErrors++;
		}
	}

	for(size_t i = firstErrorIdx-5; i <= lastErrorIdx+5; i++) {
		ProcessedPCoeffSum found = resultBuf[i];
		ProcessedPCoeffSum correct = correctResultBuf[i];
		NodeIndex idx = idxBuf[i];
		const char* alignColor = (i % 32 >= 16) ? "\n\033[37m" : "\n\033[39m";
		std::cout << alignColor + std::to_string(idx) + "> \033[39m";

		if(found != correct) {
			std::cout << 
				"sum: \033[31m" + std::to_string(getPCoeffSum(found)) + "\033[39m / \033[32m" + std::to_string(getPCoeffSum(correct))
				+ "\033[39m \tcount: \033[31m" + std::to_string(getPCoeffCount(found)) + "\033[39m / \033[32m" + std::to_string(getPCoeffCount(correct)) + "\033[39m";
		} else {
			std::cout << "\033[32mCorrect!\033[39m";
		}
	}

	std::cout << std::flush;

	size_t errorRangeSize = lastErrorIdx - firstErrorIdx + 1;
	std::cout << "\nFirst Error: " + std::to_string(firstErrorIdx) + " (bot " + std::to_string(idxBuf[firstErrorIdx]) + ")";
	std::cout << "\nLast Error: " + std::to_string(lastErrorIdx) + " (bot " + std::to_string(idxBuf[lastErrorIdx]) + ")";
	std::cout << "\nNumber of Errors: " + std::to_string(numberOfErrors) + " = " + std::to_string(numberOfErrors%512) + " mod 512 / " + std::to_string(bufSize);
	std::cout << "\nError index range size: " + std::to_string(errorRangeSize) + " = " + std::to_string(errorRangeSize%512) + " mod 512 --> (" + std::to_string(numberOfErrors * 100.0 / errorRangeSize) + "%)";
	std::cout << "\n" << std::flush;

	bool longStreakFound = false;
	for(size_t chunkOffset = 0; chunkOffset < errorRangeSize; chunkOffset += 512) {
		size_t longestErrorStreak = 0;
		size_t longestErrorStart = 0;

		for(size_t i = 0; i < bufSize; i++) {
			for(size_t streamLength = 0; streamLength < errorRangeSize - chunkOffset; streamLength++) {
				if(__builtin_expect(resultBuf[firstErrorIdx + chunkOffset + streamLength] != correctResultBuf[i], 1)) {
					if(longestErrorStreak < streamLength) {
						longestErrorStreak = streamLength;
						longestErrorStart = i;
					}
					break;
				}
			}
		}

		if(longestErrorStreak > 8) {
			longStreakFound = true;
			std::cout << "Significant Streak found elsewhere: Chunk offset " + std::to_string(chunkOffset) + ":\n" << std::flush;
			std::cout << "    Streak found at: " + std::to_string(longestErrorStart) + " = " + std::to_string(longestErrorStart%512)
			+ " mod 512\n    Streak length: " + std::to_string(longestErrorStreak) + " = " + std::to_string(longestErrorStreak%512) + " mod 512\n" << std::flush;
		}
	}

	if(!longStreakFound) {
		std::cout << "Error streak found nowhere else" << std::endl;
	}
}

template<unsigned int Variables>
void checkAllErrorBuffers(const std::vector<std::string>& args) {
	const std::string& errorsFolder = args[0];

	for(std::filesystem::directory_entry errFile : std::filesystem::directory_iterator(errorsFolder)) {
		std::string errFilePath = errFile.path().string();
		std::cout << "Error file " + errFilePath << std::endl;
		std::vector<std::string> argVec{errFilePath};
		checkErrorBuffer<Variables>(argVec);
	}
}

template<typename IntType>
std::string toHex(IntType v) {
	constexpr char chars[16]{'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
	char outStr[sizeof(IntType)*2];
	for(size_t i = 0; i < sizeof(IntType) * 2; i++) {
		IntType quartet = (v >> (sizeof(IntType) * 8 - 4 * i - 4)) & 0b00001111;
		outStr[i] = chars[quartet];
	}
	return std::string(outStr, sizeof(IntType) * 2);
}

void printErrorBuffer(const std::vector<std::string>& args) {
	std::string fileName = args[0];

	NodeIndex* idxBuf = aligned_mallocT<NodeIndex>(mbfCounts[7], 4096);
	ProcessedPCoeffSum* resultsBuf = aligned_mallocT<ProcessedPCoeffSum>(mbfCounts[7], 4096);
	uint64_t bufSize = readProcessingBufferPairFromFile(fileName.c_str(), idxBuf, resultsBuf);

	uint64_t from = 0;
	uint64_t to;
	if(args.size() > 1) {
		from = std::stoll(args[1]);
		to = std::stoll(args[2]);
		if(bufSize < to) to = bufSize;
	} else {
		to = bufSize;
	}

	std::cout << "Buffer for top " + std::to_string(idxBuf[0] & 0x7FFFFFFF) + " of size " + std::to_string(bufSize) + "\n";

	std::cout << "Start at idx " + std::to_string(from) << ":\n";
	for(uint64_t i = from; i < to; i++) {
		ProcessedPCoeffSum ps = resultsBuf[i];
		const char* selectedColor = (i % 32 >= 16) ? "\033[37m" : "\033[39m";
		if(i % 512 == 0) selectedColor = "\033[34m";
		std::string hexText = toHex(ps);
		if(hexText[0] == 'e') {
			hexText = "\033[31m" + hexText + selectedColor;
		}
		if(hexText[0] == 'f') {
			hexText = "\033[32m" + hexText + selectedColor;
		}
		std::cout << selectedColor + std::to_string(idxBuf[i]) + "> " + hexText + "  sum: " + std::to_string(getPCoeffSum(ps)) + " count: " + std::to_string(getPCoeffCount(ps)) + "\033[39m\n";
	}
	std::cout << "Up to idx " + std::to_string(to) << "\n";
}

template<unsigned int Variables>
void checkSingleTopResult(BetaResult originalResult) {
	std::cout << "Checking top " + std::to_string(originalResult.topIndex) + "..." << std::endl;

	std::cout << "Original result: " + toString(originalResult) << std::endl;
	SingleTopResult realSol = computeSingleTopWithAllCores<Variables, true>(originalResult.topIndex);

	std::cout << "Original result: " + toString(originalResult) << std::endl;
	std::cout << "Correct result:  result: " + toString(realSol.resultSum) + ", dual: " + toString(realSol.dualSum) << std::endl;

	if(originalResult.dataForThisTop.betaSum == realSol.resultSum && originalResult.dataForThisTop.betaSumDualDedup == realSol.dualSum) {
		std::cout << "\033[32mCorrect!\033[39m\n" << std::flush;
	} else {
		std::cout << "\033[31mBad Result!\033[39m\n" << std::flush;
		std::abort();
	}
}

template<unsigned int Variables>
void checkResultsFileSamples(const std::vector<std::string>& args) {
	const std::string& filePath = args[0];

	ValidationData unused_checkSum;
	std::vector<BetaResult> results = readResultsFile(Variables, filePath.c_str(), unused_checkSum);

	if(args.size() > 1) {
		for(size_t i = 1; i < args.size(); i++) {
			size_t elemIndex = std::stoi(args[i]);
			std::cout << "Checking result idx " + std::to_string(elemIndex) + " / " + std::to_string(results.size()) << std::endl;
			if(elemIndex >= results.size()) {
				std::cerr << "ERROR Result index out of bounds! " + std::to_string(elemIndex) + " / " + std::to_string(results.size()) << std::endl;
				std::abort();
			}

			checkSingleTopResult<Variables>(results[elemIndex]);
		}
	} else {
		std::default_random_engine generator;
		std::uniform_int_distribution<size_t> distr(0, results.size() - 1);
		size_t selectedResult = distr(generator);
		std::cout << "Checking result idx " + std::to_string(selectedResult) + " / " + std::to_string(results.size()) << std::endl;
		checkSingleTopResult<Variables>(results[selectedResult]);
	}
}

template<unsigned int Variables>
void produceBasicResultsList(const std::vector<std::string>& args) {
	std::string newResultsPath = args[0];

	std::vector<BetaSumPair> fullResultsList(mbfCounts[Variables]);
	for(NodeIndex i = 0; i < mbfCounts[Variables]; i++) {
		SingleTopResult r = computeSingleTopWithAllCores<Variables, true>(i);
		fullResultsList[i] = BetaSumPair{r.resultSum, r.dualSum};
	}

	{
		std::ofstream allResultsFileCorrected(newResultsPath);
		allResultsFileCorrected.write(reinterpret_cast<const char*>(&fullResultsList[0]), sizeof(BetaSumPair) * mbfCounts[Variables]);
	}
}

template<unsigned int Variables>
uint64_t countNumberOfMonotonicDownFrom(const Monotonic<Variables>& top, size_t levelsToGo) {
	struct StackElem {
		Monotonic<Variables> mbf;
		uint8_t bits[MAX_EXPANSION];
		int curBit;
		int numBits;
	} stack[1 << Variables];
	size_t stackSize = 0;

	uint64_t total = 0;

	Monotonic<Variables> cur = top;

	//Add new mbf to stack
	total++;
	AntiChain<Variables> newBits = cur.asAntiChain();
	StackElem* stackTop = &stack[stackSize++];
	newBits.forEachOne([stackTop](size_t bit) {
		stackTop->bits[stackTop->numBits++] = static_cast<uint8_t>(bit);
	});
	stackTop->curBit = 0;
	stackTop->mbf = cur;

	if(stackTop->curBit != stackTop->numBits) {

	}
}

template<unsigned int Variables>
void getAllLayerSizes(const BooleanFunction<Variables>& bf, int layerSizeBuf[Variables+1]) {
	for(int l = 0; l < Variables + 1; l++) {
		layerSizeBuf[l] = bf.getLayer(l).size();
	}
}

template<unsigned int Variables>
int getHighestFullLayer(const BooleanFunction<Variables>& bf) {
	for(int l = 0; l < Variables + 1; l++) {
		BitSet<(1 << Variables)> missingBitsInLayer = andnot(BooleanFunction<Variables>::layerMask(l), bf.bitset);
		if(!missingBitsInLayer.isEmpty()) {
			return l - 1;
		}
	}
	return Variables + 1;
}

template<unsigned int Variables>
bool couldHavePermutationSubset(const BooleanFunction<Variables>& bf, const int layerSizeBuf[Variables+1]) {
	for(int l = 0; l < Variables + 1; l++) {
		int thisLayerSize = bf.getLayer(l).size();

		if(thisLayerSize > layerSizeBuf[l]) {
			return false;
		}
	}
	return true;
}

static void check0AndModuloVariables(unsigned int Variables, const std::vector<BetaSumPair>& fullResultsList) {
	std::cout << "Checking for 0 and " << factorial(Variables) << " divisibility..." << std::endl;
	for(NodeIndex i = 0; i < mbfCounts[Variables]; i++) {
		const BetaSumPair& curResult = fullResultsList[i];
		if(
			curResult.betaSum.countedIntervalSizeDown == 0 ||
			curResult.betaSum.betaSum % factorial(Variables) != 0 ||
			curResult.betaSum.countedIntervalSizeDown % factorial(Variables) != 0 ||
			curResult.betaSumDualDedup.betaSum % factorial(Variables) != 0 ||
			curResult.betaSumDualDedup.countedIntervalSizeDown % factorial(Variables) != 0
		) {
			std::cout << "Top " + std::to_string(i) + " is certainly wrong!" << std::endl;
		}
	}
	std::cout << "0 and " << factorial(Variables) << " divisibility check done." << std::endl;
}

template<unsigned int Variables>
static void checkTopsTail(const std::vector<BetaSumPair>& fullResultsList, const FlatNode* flatNodes, const ClassInfo* classInfos, const Monotonic<Variables>* allMBFs) {
	std::cout << "Checking tops tail" << std::endl;

	struct TopToCheck {
		NodeIndex top;
		NodeIndex topDual;
		int highestFullLayer;
	};

	// Start halfway, because lower tops are trivial to check
	NodeIndex startAt = flatNodeLayerOffsets[Variables][(1 << Variables) / 2 + 1];
	std::vector<TopToCheck> topToCheckVector;
	topToCheckVector.reserve(mbfCounts[Variables] - startAt);
	for(NodeIndex top = startAt; top < mbfCounts[Variables]; top++) {
		Monotonic<Variables> topMBF = allMBFs[top];
		TopToCheck newElem;
		newElem.top = top;
		newElem.topDual = static_cast<NodeIndex>(flatNodes[top].dual);
		newElem.highestFullLayer = getHighestFullLayer(topMBF.bf);
		topToCheckVector.push_back(std::move(newElem));
	}

	// Do easy tops first
	std::sort(topToCheckVector.begin(), topToCheckVector.end(), [](TopToCheck a, TopToCheck b) -> bool {
		if(a.highestFullLayer != b.highestFullLayer) {
			return a.highestFullLayer > b.highestFullLayer; // Higher first full layer is easier
		} else {
			return a.topDual < b.topDual; // Lower dual is easier
		}
	});



	int lastHighest = 0;
	BooleanFunction<Variables> upToLayerMask;
	for(size_t idx = 0; idx < topToCheckVector.size(); idx++) {
		if(idx % 10000 == 0) {
			std::cout << idx << std::endl;
		}

		TopToCheck ttc = topToCheckVector[idx];
		if(ttc.highestFullLayer != lastHighest) {
			lastHighest = ttc.highestFullLayer;
			std::cout << "highestFullLayer: " << lastHighest << std::endl;
			upToLayerMask = BooleanFunction<Variables>::empty();
			for(int l = 0; l <= lastHighest; l++) {
				upToLayerMask.bitset |= BooleanFunction<Variables>::layerMask(l);
			}
		}

		uint64_t totalCountForThisTop = 0;
		Monotonic<Variables> topMBF = allMBFs[ttc.top];

		Monotonic<Variables> topMBFPermutes[factorial(Variables)];
		{
			size_t j = 0;
			topMBF.forEachPermutation([&](Monotonic<Variables> permut){
				topMBFPermutes[j++] = permut;
			});
		}

		for(NodeIndex bot = 0; bot < ttc.topDual; bot++) {
			Monotonic<Variables> botMBF = allMBFs[bot];
			if(botMBF.bf.isSubSetOf(upToLayerMask)) {
				// All permutations are valid
				totalCountForThisTop += factorial(Variables) * classInfos[bot].classSize;
			} else {
				// Check permutations individually
				for(size_t j = 0; j < factorial(Variables); j++) {
					if(botMBF <= topMBFPermutes[j]) {
						totalCountForThisTop += classInfos[bot].classSize;
					}
				}
			}
		}
		if(totalCountForThisTop != fullResultsList[ttc.top].betaSum.countedIntervalSizeDown) {
			std::cout << "Top " + std::to_string(ttc.top) + " is certainly wrong! Bad intervalSizeDown: (should be: " << classInfos[ttc.top].intervalSizeDown << ", found: " << totalCountForThisTop << std::endl;
		}
	}
}

#include "../dedelib/bitSet.h"
template<size_t Width>
struct MBFSwapper {
	alignas(4096) BitSet<Width>* upper;
	alignas(4096) BitSet<Width>* lower;
	size_t byteSize;
	
	MBFSwapper(unsigned int Variables, int numaNode) {
		byteSize = sizeof(BitSet<Width>) * getMaxLayerSize(Variables);
		upper = (BitSet<Width>*) numa_alloc_onnode(byteSize, numaNode);
		lower = (BitSet<Width>*) numa_alloc_onnode(byteSize, numaNode);
	}
	~MBFSwapper() {
		numa_free(upper, byteSize);
		numa_free(lower, byteSize);
	}
	MBFSwapper& operator=(const MBFSwapper&&) = delete;
	MBFSwapper(const MBFSwapper&&) = delete;
	MBFSwapper& operator=(const MBFSwapper&) = delete;
	MBFSwapper(const MBFSwapper&) = delete;

	template<typename IntT>
	void init(const IntT* indices, size_t numIndices, size_t initialLayerSize) {
		for(size_t i = 0; i < initialLayerSize; i++) {
			upper[i] = BitSet<Width>::empty();
		}
		for(size_t i = 0; i < numIndices; i++) {
			upper[indices[i]].set(i);
		}
	}

	void initRange(size_t startAt, size_t numIndices, size_t initialLayerSize) {
		for(size_t i = 0; i < initialLayerSize; i++) {
			upper[i] = BitSet<Width>::empty();
		}
		for(size_t i = 0; i < numIndices; i++) {
			upper[startAt + i].set(i);
		}
	}

	void followLinksToNextLayer(const uint32_t* links, size_t numNodesInNextLayer, size_t numLinksInThisTransition) {
		assert((links[-1] & uint32_t(0x80000000)) != 0);
		const uint32_t* curLink = links;
		for(size_t nodeIndex = 0; nodeIndex < numNodesInNextLayer; nodeIndex++) {
			uint32_t fromNode = *curLink++;
			BitSet<Width> total = upper[fromNode & 0x7FFFFFFF];
			while((fromNode & uint32_t(0x80000000)) == 0) {
				fromNode = *curLink++;
				total |= upper[fromNode & 0x7FFFFFFF];
			}
			lower[nodeIndex] = total;
		}
		
		auto* tmp = upper;
		upper = lower;
		lower = tmp;

		size_t linksCounted = curLink - links;
		assert(linksCounted == numLinksInThisTransition);
	}

	// Expects a function of the form void(size_t localNodeIndex, size_t idxInBitset)
	template<typename Func>
	void forEachOne(const Func& func, size_t currentLayerWidth) {
		for(size_t localNodeIndex = 0; localNodeIndex < currentLayerWidth; localNodeIndex++) {
			upper[localNodeIndex].forEachOne([&func, localNodeIndex](size_t idx){
				func(localNodeIndex, idx);
			});
		}
	}
};

static void checkTopsFirstHalf(unsigned int Variables, const std::vector<BetaSumPair>& fullResultsList, const FlatNode* flatNodes, const ClassInfo* classInfos) {
	size_t upTo = flatNodeLayerOffsets[Variables][(1 << Variables) / 2 + 1];
	std::cout << "Checking intervalSizeDown FIRST HALF... (0 - " << upTo << ")" << std::endl;

	for(NodeIndex i = 0; i < upTo; i++) {
		//if(i % 1000000 == 0) std::cout << i << "..." << std::endl;
		NodeIndex dual = static_cast<NodeIndex>(flatNodes[i].dual);

		uint64_t foundIntervalSize = fullResultsList[i].betaSum.countedIntervalSizeDown + fullResultsList[i].betaSumDualDedup.countedIntervalSizeDown;

		//if(i >= flatNodeLayerOffsets[Variables][(1 << Variables) / 2 + 1]) {
		if(i > dual) {
			foundIntervalSize += factorial(Variables);
		}

		foundIntervalSize /= factorial(Variables);
		if(foundIntervalSize != classInfos[i].intervalSizeDown) {
			std::cout << "Top " + std::to_string(i) + " is certainly wrong! Bad intervalSizeDown: (should be: " << classInfos[i].intervalSizeDown << ", found: " << foundIntervalSize << ") // classSize=" << classInfos[i].classSize << ", bs_dual=" << fullResultsList[i].betaSumDualDedup.countedIntervalSizeDown << std::endl;
		}
	}
	std::cout << "FIRST HALF check done" << std::endl;
}

template<unsigned int Variables>
static void checkTopsSecondHalfNaive(const std::vector<BetaSumPair>& fullResultsList, const FlatNode* flatNodes, const ClassInfo* classInfos, const Monotonic<Variables>* allMBFs) {
	std::cout << "Checking intervalSizeDown SECOND HALF... (" << flatNodeLayerOffsets[Variables][(1 << Variables) / 2 + 1] << " - " << mbfCounts[Variables] << ")" << std::endl;
	size_t lowestLayer = 0;
	for(NodeIndex i = flatNodeLayerOffsets[Variables][(1 << Variables) / 2 + 1]; i < mbfCounts[Variables]; i++) {
		
		//if(i % 1000000 == 0) std::cout << i << "..." << std::endl;
		NodeIndex dual = static_cast<NodeIndex>(flatNodes[i].dual);

		uint64_t foundIntervalSize = fullResultsList[i].betaSum.countedIntervalSizeDown + fullResultsList[i].betaSumDualDedup.countedIntervalSizeDown;

		Monotonic<Variables> mbfI = allMBFs[i];

		Monotonic<Variables> mbfIPermutes[factorial(Variables)];
		{
			size_t j = 0;
			mbfI.forEachPermutation([&](Monotonic<Variables> permut){
				mbfIPermutes[j++] = permut;
			});
			assert(j == factorial(Variables));
		}
		size_t mbfLayer = mbfI.size();
		if(mbfLayer != lowestLayer) {
			std::cout << "Layer " << mbfLayer << std::endl;
			lowestLayer = mbfLayer;
		}
		// Skip i's layer, contains only i, and is very wide. 
		//size_t layerIStart = flatNodeLayerOffsets[Variables][mbfLayer];
		/*if(dual + 1 <= i) {
			foundIntervalSize += factorial(Variables);
		}*/

		int mbfILayerSizes[Variables+1];
		getAllLayerSizes(mbfI.bf, mbfILayerSizes);
		std::cout << i << ": ";
		for(NodeIndex inbetween = dual; inbetween <= i; inbetween++) {
			Monotonic<Variables> inbetweenMBF = allMBFs[inbetween];
			//if(couldHavePermutationSubset(inbetweenMBF.bf, mbfILayerSizes)) { // Optimization. Early exit for mbfs that can't be below
				uint64_t countForThisBot = 0;
				for(size_t j = 0; j < factorial(Variables); j++) {
					if(inbetweenMBF <= mbfIPermutes[j]) {
						countForThisBot++;
					}
				}
				if(countForThisBot != 0) std::cout << inbetween << " ";
				assert(countForThisBot * classInfos[inbetween].classSize % factorial(Variables) == 0);
				foundIntervalSize += countForThisBot * classInfos[inbetween].classSize;
			//}
		}

		std::cout << std::endl;
		assert(foundIntervalSize % factorial(Variables) == 0);
		foundIntervalSize /= factorial(Variables);
		if(foundIntervalSize != classInfos[i].intervalSizeDown) {
			std::cout << "Top " + std::to_string(i) + " is certainly wrong! Bad intervalSizeDown: (should be: " << classInfos[i].intervalSizeDown << ", found: " << foundIntervalSize
			 << ") // classSize=" << classInfos[i].classSize
			  << ", bs_dual=" << fullResultsList[i].betaSumDualDedup.countedIntervalSizeDown
			  << ", original interval size=" << fullResultsList[i].betaSum.countedIntervalSizeDown / factorial(Variables)
			  << std::endl;
		}
	}
}

template<unsigned int Variables, size_t MaxBlockSize>
static void checkResultsBlock(NodeIndex startAt, size_t numInThisBlock, MBFSwapper<MaxBlockSize>& swapper, const std::vector<BetaSumPair>& fullResultsList, const FlatNode* flatNodes, const ClassInfo* classInfos, const Monotonic<Variables>* allMBFs, const uint32_t* flatLinks) {
	int blockLayer = getFlatLayerOfIndex(Variables, startAt);

	if(blockLayer <= (1 << Variables) / 2) {
		std::cerr << "Assertion Error. This function should only be run for tops in the upper half. Top " << startAt << " has layer " << blockLayer << std::endl;
		exit(-1);
	}
	int dualsLayer = (1 << Variables) - blockLayer;
	swapper.initRange(startAt - flatNodeLayerOffsets[Variables][blockLayer], numInThisBlock, layerSizes[Variables][blockLayer]);

	uint64_t totalIntervalSizesDown[MaxBlockSize];
	for(size_t i = 0; i < numInThisBlock; i++) {
		totalIntervalSizesDown[i] = fullResultsList[startAt + i].betaSum.countedIntervalSizeDown
			 + fullResultsList[startAt + i].betaSumDualDedup.countedIntervalSizeDown
			 + factorial(Variables); // For the top itself, we don't include it in the swapper run
	}

	for(int layer = blockLayer-1; layer >= dualsLayer; layer--) {
		std::cout << "Iter layer " << layer << std::endl;

		NodeIndex thisLayerSize = layerSizes[Variables][layer];
		NodeIndex thisLayerOffset = flatNodeLayerOffsets[Variables][layer];
		size_t numLinksBetweenLayers = linkCounts[Variables][layer];

		swapper.followLinksToNextLayer(flatLinks + flatLinkOffsets[Variables][(1 << Variables) - layer-1], thisLayerSize, numLinksBetweenLayers); // mbfStructure is ordered in referse, seems I thought that optimization worth the confusion

		for(NodeIndex localBottom = 0; localBottom < thisLayerSize; localBottom++) {
			NodeIndex realBottom = thisLayerOffset + localBottom;
			
			NodeIndex localTopsForThisBottom[MaxBlockSize];
			size_t numLocalTopsForThisBottom = 0;

			swapper.upper[localBottom].forEachOne([&](size_t localTop){
				size_t realTop = startAt + localTop;
				size_t realTopDual = flatNodes[realTop].dual;
				if(realBottom >= realTopDual) {
					localTopsForThisBottom[numLocalTopsForThisBottom++] = localTop;
				}
			});
			if(numLocalTopsForThisBottom >= 1) {
				Monotonic<Variables> botMBF = allMBFs[realBottom];

				Monotonic<Variables> botMBFPermutes[factorial(Variables)];
				{
					size_t j = 0;
					botMBF.forEachPermutation([&](Monotonic<Variables> permut){
						botMBFPermutes[j++] = permut;
					});
				}
				uint64_t botClassSize = classInfos[realBottom].classSize;

				for(NodeIndex* localTop = localTopsForThisBottom; localTop != localTopsForThisBottom + numLocalTopsForThisBottom; localTop++) {
					NodeIndex realTop = startAt + *localTop;
					
					Monotonic<Variables> topMBF = allMBFs[realTop];

					uint64_t countForThisBotTop = 0;
					for(Monotonic<Variables> botPermut : botMBFPermutes) {
						if(botPermut <= topMBF) {
							countForThisBotTop++;
						}
					}
					assert(countForThisBotTop != 0);
					totalIntervalSizesDown[*localTop] += botClassSize * countForThisBotTop;
				}
			}
		}
	}

	std::cout << "Checking these tops..." << std::endl;
	for(size_t localI = 0; localI < numInThisBlock; localI++) {
		uint64_t foundIntervalSize = totalIntervalSizesDown[localI];
		size_t i = startAt + localI;
		foundIntervalSize /= factorial(Variables);
		if(foundIntervalSize != classInfos[i].intervalSizeDown) {
			std::cout << "Top " + std::to_string(i) + " is certainly wrong! Bad intervalSizeDown: (should be: " << classInfos[i].intervalSizeDown << ", found: " << foundIntervalSize
			 << ") // classSize=" << classInfos[i].classSize
			  << ", bs_dual=" << fullResultsList[i].betaSumDualDedup.countedIntervalSizeDown
			  << ", original interval size=" << fullResultsList[i].betaSum.countedIntervalSizeDown / factorial(Variables)
			   << std::endl;
		}
	}
}

template<unsigned int Variables>
static void checkTopsSecondHalfWithSwapper(const std::vector<BetaSumPair>& fullResultsList, const FlatNode* flatNodes, const ClassInfo* classInfos, const Monotonic<Variables>* allMBFs, const uint32_t* flatLinks) {
	std::cout << "Checking intervalSizeDown SECOND HALF... (" << flatNodeLayerOffsets[Variables][(1 << Variables) / 2 + 1] << " - " << mbfCounts[Variables] << ")" << std::endl;

	constexpr size_t MAX_BLOCK_SIZE = 16;
	MBFSwapper<MAX_BLOCK_SIZE> swapper(Variables, 0);

	for(size_t layer = (1 << Variables) / 2 + 1; layer < (1 << Variables); layer++) {
		NodeIndex layerTopsStart = flatNodeLayerOffsets[Variables][layer];
		NodeIndex layerTopsEnd = flatNodeLayerOffsets[Variables][layer+1];

		NodeIndex topsBatch;
		for(topsBatch = layerTopsStart; topsBatch < layerTopsEnd - MAX_BLOCK_SIZE; topsBatch+=MAX_BLOCK_SIZE) {
			checkResultsBlock<Variables, MAX_BLOCK_SIZE>(topsBatch, MAX_BLOCK_SIZE, swapper, fullResultsList, flatNodes, classInfos, allMBFs, flatLinks);
		}
		checkResultsBlock<Variables, MAX_BLOCK_SIZE>(topsBatch, layerTopsEnd - topsBatch, swapper, fullResultsList, flatNodes, classInfos, allMBFs, flatLinks);
	}
}

template<unsigned int Variables>
void findErrorInNearlyCorrectResults(const std::vector<std::string>& args) {
	std::string allResultsPath = args[0];

	std::cout << "Loading fullResultsList..." << std::endl;
	std::vector<BetaSumPair> fullResultsList(mbfCounts[Variables]);
	{
		std::ifstream allResultsFile(allResultsPath);
		allResultsFile.read(reinterpret_cast<char*>(&fullResultsList[0]), sizeof(BetaSumPair) * mbfCounts[Variables]);
	}
	

	// std::cout << "Running check0AndModuloVariables..." << std::endl;
	// check0AndModuloVariables(Variables, fullResultsList);

	std::cout << "Loading flatNodes..." << std::endl;
	const FlatNode* flatNodes = readFlatBuffer<FlatNode>(FileName::flatNodes(Variables), mbfCounts[Variables]);
	std::cout << "Loading classInfos..." << std::endl;
	const ClassInfo* classInfos = readFlatBuffer<ClassInfo>(FileName::flatClassInfo(Variables), mbfCounts[Variables]);

	//std::cout << "Running checkTopsFirstHalf..." << std::endl;
	//checkTopsFirstHalf(Variables, fullResultsList, flatNodes, classInfos);

	std::cout << "Loading allMBFs..." << std::endl;
	const Monotonic<Variables>* allMBFs = readFlatBuffer<Monotonic<Variables>>(FileName::flatMBFs(Variables), mbfCounts[Variables]);

	//checkTopsSecondHalfNaive<Variables>(fullResultsList, flatNodes, classInfos, allMBFs);

	std::cout << "Loading flatLinks..." << std::endl;
	const uint32_t* flatLinks = readFlatBuffer<uint32_t>(FileName::mbfStructure(Variables), getTotalLinkCount(Variables));
	
	//std::cout << "Running checkTopsSecondHalf..." << std::endl;
	checkTopsSecondHalfWithSwapper<Variables>(fullResultsList, flatNodes, classInfos, allMBFs, flatLinks);
	
	//checkTopsTail<Variables>(fullResultsList, flatNodes, classInfos, allMBFs);

	std::cout << "All Checks done" << std::endl;
}

template<unsigned int Variables>
void correctTops(const std::vector<std::string>& args) {
	std::string allResultsPath = args[0];
	std::string newResultsPath = args[1];
	std::vector<NodeIndex> topsToCorrect;
	for(size_t i = 2; i < args.size(); i++) {
		topsToCorrect.push_back(std::stoi(args[i]));
	}

	std::vector<BetaSumPair> fullResultsList(mbfCounts[Variables]);
	{
		std::ifstream allResultsFile(allResultsPath);
		allResultsFile.read(reinterpret_cast<char*>(&fullResultsList[0]), sizeof(BetaSumPair) * mbfCounts[Variables]);
	}

	for(NodeIndex topToCorrect : topsToCorrect) {
		std::cout << "Computing new top..." << std::endl;
		BetaSumPair oldValue = fullResultsList[topToCorrect];
		std::cout << "Old value: " << toString(oldValue.betaSum) << "; " << toString(oldValue.betaSumDualDedup) << std::endl;
		SingleTopResult newTopResult = computeSingleTopWithAllCores<Variables, true>(topToCorrect);

		fullResultsList[topToCorrect].betaSum = newTopResult.resultSum;
		fullResultsList[topToCorrect].betaSumDualDedup = newTopResult.dualSum;

		std::cout << "Top " << topToCorrect << std::endl;
		std::cout << "Old value: " << toString(oldValue.betaSum) << "; " << toString(oldValue.betaSumDualDedup) << std::endl;
		std::cout << "New value: " << toString(newTopResult.resultSum) << "; " << toString(newTopResult.dualSum) << std::endl;
	}

	{
		std::ofstream allResultsFileCorrected(newResultsPath);
		allResultsFileCorrected.write(reinterpret_cast<const char*>(&fullResultsList[0]), sizeof(BetaSumPair) * mbfCounts[Variables]);
	}
	std::cout << "Computation finished." << std::endl;
	u192 dedekindNumber = computeDedekindNumberFromBetaSums(Variables, fullResultsList);
	std::cout << "D(" << (Variables + 2) << ") = " << toString(dedekindNumber) << std::endl;
}

CommandSet superCommands {"Supercomputing Commands", {}, {
	{"initializeSupercomputingProject", [](const std::vector<std::string>& args) {
		std::string projectFolderPath = args[0];
		unsigned int targetDedekindNumber = std::stoi(args[1]);
		size_t numberOfJobs = std::stoi(args[2]);
		size_t numberOfJobsToActuallyGenerate = numberOfJobs;
		if(args.size() >= 4) {
			numberOfJobsToActuallyGenerate = std::stoi(args[3]);
			std::cerr << "WARNING: GENERATING ONLY PARTIAL JOBS:" << numberOfJobsToActuallyGenerate << '/' << numberOfJobs << " -> INVALID COMPUTE PROJECT!\n" << std::flush;
		}
		initializeComputeProject(targetDedekindNumber - 2, projectFolderPath, numberOfJobs, numberOfJobsToActuallyGenerate);
	}},

	{"resetUnfinishedJobs", [](const std::vector<std::string>& args){
		size_t lowerBound = 0;
		size_t upperBound = 99999999;
		if(args.size() >= 3) {
			lowerBound = std::stoi(args[2]);
			upperBound = std::stoi(args[3]);
		}

		resetUnfinishedJobs(args[0], args[1], lowerBound, upperBound);
	}},
	{"checkProjectResultsIdentical", [](const std::vector<std::string>& args){checkProjectResultsIdentical(std::stoi(args[0]) - 2, args[1], args[2]);}},

	{"collectAllSupercomputingProjectResults", [](const std::vector<std::string>& args){collectAndProcessResults(std::stoi(args[0]) - 2, args[1]);}},

	{"collectAllSupercomputingProjectResultsMessy", [](const std::vector<std::string>& args){collectAndProcessResultsMessy(std::stoi(args[0]) - 2, args[1]);}},

	{"checkProjectIntegrity", [](const std::vector<std::string>& args){checkProjectIntegrity(std::stoi(args[0]) - 2, args[1]);}},

	{"processJobCPU1_ST", processSuperComputingJob_ST<1>},
	{"processJobCPU2_ST", processSuperComputingJob_ST<2>},
	{"processJobCPU3_ST", processSuperComputingJob_ST<3>},
	{"processJobCPU4_ST", processSuperComputingJob_ST<4>},
	{"processJobCPU5_ST", processSuperComputingJob_ST<5>},
	{"processJobCPU6_ST", processSuperComputingJob_ST<6>},
	{"processJobCPU7_ST", processSuperComputingJob_ST<7>},

	{"processJobCPU1_FMT", processSuperComputingJob_FMT<1>},
	{"processJobCPU2_FMT", processSuperComputingJob_FMT<2>},
	{"processJobCPU3_FMT", processSuperComputingJob_FMT<3>},
	{"processJobCPU4_FMT", processSuperComputingJob_FMT<4>},
	{"processJobCPU5_FMT", processSuperComputingJob_FMT<5>},
	{"processJobCPU6_FMT", processSuperComputingJob_FMT<6>},
	{"processJobCPU7_FMT", processSuperComputingJob_FMT<7>},

	{"processJobCPU1_SMT", processSuperComputingJob_SMT<1>},
	{"processJobCPU2_SMT", processSuperComputingJob_SMT<2>},
	{"processJobCPU3_SMT", processSuperComputingJob_SMT<3>},
	{"processJobCPU4_SMT", processSuperComputingJob_SMT<4>},
	{"processJobCPU5_SMT", processSuperComputingJob_SMT<5>},
	{"processJobCPU6_SMT", processSuperComputingJob_SMT<6>},
	{"processJobCPU7_SMT", processSuperComputingJob_SMT<7>},

	{"checkErrorBuffer1", checkErrorBuffer<1>},
	{"checkErrorBuffer2", checkErrorBuffer<2>},
	{"checkErrorBuffer3", checkErrorBuffer<3>},
	{"checkErrorBuffer4", checkErrorBuffer<4>},
	{"checkErrorBuffer5", checkErrorBuffer<5>},
	{"checkErrorBuffer6", checkErrorBuffer<6>},
	{"checkErrorBuffer7", checkErrorBuffer<7>},

	{"checkAllErrorBuffers1", checkAllErrorBuffers<1>},
	{"checkAllErrorBuffers2", checkAllErrorBuffers<2>},
	{"checkAllErrorBuffers3", checkAllErrorBuffers<3>},
	{"checkAllErrorBuffers4", checkAllErrorBuffers<4>},
	{"checkAllErrorBuffers5", checkAllErrorBuffers<5>},
	{"checkAllErrorBuffers6", checkAllErrorBuffers<6>},
	{"checkAllErrorBuffers7", checkAllErrorBuffers<7>},

	{"printErrorBuffer", printErrorBuffer},

	{"checkResultsFileSamples1", checkResultsFileSamples<1>},
	{"checkResultsFileSamples2", checkResultsFileSamples<2>},
	{"checkResultsFileSamples3", checkResultsFileSamples<3>},
	{"checkResultsFileSamples4", checkResultsFileSamples<4>},
	{"checkResultsFileSamples5", checkResultsFileSamples<5>},
	{"checkResultsFileSamples6", checkResultsFileSamples<6>},
	{"checkResultsFileSamples7", checkResultsFileSamples<7>},

	{"produceBasicResultsList1", produceBasicResultsList<1>},
	{"produceBasicResultsList2", produceBasicResultsList<2>},
	{"produceBasicResultsList3", produceBasicResultsList<3>},
	{"produceBasicResultsList4", produceBasicResultsList<4>},
	{"produceBasicResultsList5", produceBasicResultsList<5>},
	{"produceBasicResultsList6", produceBasicResultsList<6>},
	{"produceBasicResultsList7", produceBasicResultsList<7>},

	{"generateMissingTopJobs", [](const std::vector<std::string>& args){
		unsigned int Variables = std::stoi(args[0]) - 2;
		std::string projectFolder = args[1];
		BetaResultCollector collector = collectAllResultFilesAndRecoverFailures(Variables, projectFolder);

		unsigned int startJobIndex = std::stoi(args[2]);
		unsigned int numberOfTopsPerJob = std::stoi(args[3]);

		size_t numFoundTops = 0;
		u128 maxBetaSum = 0;
		u128 maxBetaSumDedup = 0;
		for(size_t i = 0; i < mbfCounts[Variables]; i++) {
			if(collector.hasSeenResult[i]) {
				numFoundTops++;
				BetaSumPair bsp = collector.allBetaSums[i];
				if(bsp.betaSum.betaSum > maxBetaSum) maxBetaSum = bsp.betaSum.betaSum;
				if(bsp.betaSumDualDedup.betaSum > maxBetaSumDedup) maxBetaSumDedup = bsp.betaSumDualDedup.betaSum;
			}
		}
		size_t missingTopCount = mbfCounts[Variables] - numFoundTops;

		std::cout << "Found " + std::to_string(numFoundTops) + "/" + std::to_string(mbfCounts[Variables]) + "  (" + std::to_string(missingTopCount) + " missing)" << std::endl;
		std::cout << "Maximum betaSum: " << toString(maxBetaSum) << "\nMaximum betaSumDualDedup: " << toString(maxBetaSumDedup) << std::endl;


		std::vector<int> missingIndices;
		missingIndices.reserve(missingTopCount);
		for(size_t i = 0; i < mbfCounts[Variables]; i++) {
			if(!collector.hasSeenResult[i]) {
				missingIndices.push_back((int) i);
			}
		}

		std::default_random_engine generator;
		std::shuffle(missingIndices.begin(), missingIndices.end(), generator);

		// TODO Generate new jobs

		const FlatNode* flatNodes = readFlatBuffer<FlatNode>(FileName::flatNodes(Variables), mbfCounts[Variables]);
		unsigned int jobIndex = startJobIndex;
		size_t curTopIdx = 0;
		while(curTopIdx < missingIndices.size()) {
			std::vector<JobTopInfo> thisJobTops;
			thisJobTops.reserve(numberOfTopsPerJob);
			for(size_t j = 0; j < numberOfTopsPerJob; j++) {
				if(curTopIdx >= missingIndices.size()) break;
				NodeIndex selectedTop = missingIndices[curTopIdx++];
				thisJobTops.push_back(JobTopInfo{selectedTop, static_cast<NodeIndex>(flatNodes[selectedTop].dual)});
			}

			writeJobToFile(Variables, computeFilePath(projectFolder, "jobs", std::to_string(jobIndex++), ".job"), thisJobTops);
		}
		freeFlatBuffer<FlatNode>(flatNodes, mbfCounts[Variables]);
	}},

	/*{"tryCorrectAllResults1", tryCorrectAllResults<1>},
	{"tryCorrectAllResults2", tryCorrectAllResults<2>},
	{"tryCorrectAllResults3", tryCorrectAllResults<3>},
	{"tryCorrectAllResults4", tryCorrectAllResults<4>},
	{"tryCorrectAllResults5", tryCorrectAllResults<5>},
	{"tryCorrectAllResults6", tryCorrectAllResults<6>},
	{"tryCorrectAllResults7", tryCorrectAllResults<7>},*/

	{"createJobForEasyWrongTops", [](const std::vector<std::string>& args){
		unsigned int Variables = std::stoi(args[0]);
		std::string computeFolder = args[1];
		std::string jobID = args[2];
		createJobForEasyWrongTops(Variables, computeFolder, jobID);
	}},

	{"checkResultsCountsLowerHalf", [](const std::vector<std::string>& args){
		unsigned int Variables = std::stoi(args[0]);
		std::string resultsPath = args[1];

		std::vector<BetaSumPair> fullResultsList(mbfCounts[Variables]);
		std::ifstream allResultsFile(resultsPath);
		allResultsFile.read(reinterpret_cast<char*>(&fullResultsList[0]), sizeof(BetaSumPair) * mbfCounts[Variables]);

		for(size_t i = 0; i < mbfCounts[Variables]; i++) {
			if(fullResultsList[i].betaSum.countedIntervalSizeDown == 0) {
				std::cout << "Counted interval size for top " << i << " is 0???" << std::endl;
			}
			/*if(fullResultsList[i].betaSumDualDedup.countedIntervalSizeDown == 0) {
				std::cout << "DualDedup interval size for top " << i << " is 0???" << std::endl;
			}*/
		}

		const FlatNode* flatNodes = readFlatBuffer<FlatNode>(FileName::flatNodes(Variables), mbfCounts[Variables]);
		const ClassInfo* classInfos = readFlatBuffer<ClassInfo>(FileName::flatClassInfo(Variables), mbfCounts[Variables]);

		std::cout << "Loaded all files." << std::endl;

		for(size_t i = 0; i < mbfCounts[Variables]; i++) {
			if(flatNodes[i].dual > i) {
				uint64_t foundIntervalSizeDown = fullResultsList[i].betaSum.countedIntervalSizeDown + fullResultsList[i].betaSumDualDedup.countedIntervalSizeDown;
				if(foundIntervalSizeDown != classInfos[i].intervalSizeDown * factorial(Variables)) {
					std::cout 
						<< "Incorrect intervalSizeDown for top " 
						<< i 
						<< ": " 
						<< fullResultsList[i].betaSum.countedIntervalSizeDown
						<< " + " 
						<< fullResultsList[i].betaSumDualDedup.countedIntervalSizeDown
						<< " != "
						<< classInfos[i].intervalSizeDown
						<< std::endl;
				}
			}
		}
	}},

	{"applyFPGAResultFileToAllResults", [](const std::vector<std::string>& args){
		unsigned int Variables = std::stoi(args[0]);
		std::string allResultsPath = args[1];
		std::string jobResultsPath = args[2];
		std::string outputAllResultsPath = args[3];

		std::vector<BetaSumPair> fullResultsList(mbfCounts[Variables]);
		{
			std::ifstream allResultsFile(allResultsPath);
			allResultsFile.read(reinterpret_cast<char*>(&fullResultsList[0]), sizeof(BetaSumPair) * mbfCounts[Variables]);
		}

		ValidationData checkSum_unused;
		checkSum_unused.dualBetaSum.betaSum = 0;
		checkSum_unused.dualBetaSum.countedIntervalSizeDown = 0;
		std::vector<BetaResult> additionalResults = readResultsFile(Variables, jobResultsPath.c_str(), checkSum_unused);

		std::cout << "Applying new results\n" << std::endl;
		for(BetaResult r : additionalResults) {
			fullResultsList[r.topIndex] = r.dataForThisTop;
		}

		{
			std::ofstream allResultsFileCorrected(outputAllResultsPath);
			allResultsFileCorrected.write(reinterpret_cast<const char*>(&fullResultsList[0]), sizeof(BetaSumPair) * mbfCounts[Variables]);
		}
		std::cout << "Computation finished." << std::endl;
		u192 dedekindNumber = computeDedekindNumberFromBetaSums(Variables, fullResultsList);
		std::cout << "D(" << (Variables + 2) << ") = " << toString(dedekindNumber) << std::endl;
	}},

	{"findErrorInNearlyCorrectResults1", findErrorInNearlyCorrectResults<1>},
	{"findErrorInNearlyCorrectResults2", findErrorInNearlyCorrectResults<2>},
	{"findErrorInNearlyCorrectResults3", findErrorInNearlyCorrectResults<3>},
	{"findErrorInNearlyCorrectResults4", findErrorInNearlyCorrectResults<4>},
	{"findErrorInNearlyCorrectResults5", findErrorInNearlyCorrectResults<5>},
	{"findErrorInNearlyCorrectResults6", findErrorInNearlyCorrectResults<6>},
	{"findErrorInNearlyCorrectResults7", findErrorInNearlyCorrectResults<7>},

	{"correctTops1", correctTops<1>},
	{"correctTops2", correctTops<2>},
	{"correctTops3", correctTops<3>},
	{"correctTops4", correctTops<4>},
	{"correctTops5", correctTops<5>},
	{"correctTops6", correctTops<6>},
	{"correctTops7", correctTops<7>},
}};
