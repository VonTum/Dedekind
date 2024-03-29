#include "commands.h"

#include "../dedelib/supercomputerJobs.h"
#include "../dedelib/pcoeffValidator.h"
#include "../dedelib/singleTopVerification.h"

#include <sstream>
#include <filesystem>
#include <unordered_map>
#include "../dedelib/threadPool.h"

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

	std::cout << "Computation finished." << std::endl;
	u192 dedekindNumber = computeDedekindNumberFromBetaSums(Variables, fullResultsList);
	std::cout << "D(" << (Variables + 2) << ") = " << toString(dedekindNumber) << std::endl;

	{
		std::ofstream allResultsFileCorrected(newResultsPath);
		allResultsFileCorrected.write(reinterpret_cast<const char*>(&fullResultsList[0]), sizeof(BetaSumPair) * mbfCounts[Variables]);
	}
}

/*template<unsigned int Variables>
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
}*/

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
class FastPermutationCounter {
	Monotonic<Variables> permutes[factorial(Variables)];
public:
	void init(Monotonic<Variables> mbf) {
		size_t j = 0;
		mbf.forEachPermutation([&](Monotonic<Variables> permut){
			permutes[j++] = permut;
		});
	}

	template<bool isTop>
	unsigned int countPermutes(Monotonic<Variables> mbf2) {
		unsigned int totalCount = 0;
		for(size_t i = 0; i < factorial(Variables); i++) {
			Monotonic<Variables> mbf1 = permutes[i];
			if constexpr(isTop) {
				if(mbf1 <= mbf2) totalCount++;
			} else {
				if(mbf2 <= mbf1) totalCount++;
			}
		}
		return totalCount;
	}
};

/*template<>
class FastPermutationCounter<7> {
	alignas(32) __int128_t permutes[factorial(7) / 6];

public:
	void init(Monotonic<7> mbf) {
		size_t blockI = 0;
		for(int v0 = 0; v0 < 7; v0++) {
			Monotonic<7> mbf0 = mbf;
			if(v0 != 0) mbf0.swap(v0, 0);
			for(int v1 = 1; v1 < 7; v1++) {
				Monotonic<7> mbf1 = mbf0;
				if(v1 != 1) mbf1.swap(v1, 1);
				for(int v2 = 2; v2 < 7; v2++) {
					Monotonic<7> mbf2 = mbf1;
					if(v2 != 2) mbf2.swap(v2, 2);
					for(int v3 = 3; v3 < 7; v3++) {
						Monotonic<7> mbf3 = mbf2;
						if(v3 != 3) mbf3.swap(v3, 3);
						
						union {
							__m128i b;
							__int128_t asU128;
						};
						b = mbf3.bf.bitset.data;
						permutes[blockI++] = asU128;
					}
				}
			}
		}
	}

	template<bool isTop>
	uint32_t countPermutes(Monotonic<7> mbf2) {
		union {
			__m128i mbits;
			int32_t swizzled[24];
			__int128_t permute6[6];
		};
		mbits = mbf2.bf.bitset.data;

		swizzled[4] = swizzled[0];
		swizzled[5] = swizzled[1];
		swizzled[6] = swizzled[3];
		swizzled[7] = swizzled[2];

		swizzled[8] = swizzled[0];
		swizzled[9] = swizzled[2];
		swizzled[10] = swizzled[1];
		swizzled[11] = swizzled[3];

		swizzled[12] = swizzled[0];
		swizzled[13] = swizzled[2];
		swizzled[14] = swizzled[3];
		swizzled[15] = swizzled[1];

		swizzled[16] = swizzled[0];
		swizzled[17] = swizzled[3];
		swizzled[18] = swizzled[1];
		swizzled[19] = swizzled[2];

		swizzled[20] = swizzled[0];
		swizzled[21] = swizzled[3];
		swizzled[22] = swizzled[2];
		swizzled[23] = swizzled[1];

		unsigned int totalCount = 0;
		for(size_t i = 0; i < factorial(7) / 6; i++) {
			for(int p = 0; p < 6; p++) {
				if constexpr(!isTop) {
					if((permute6[p] & ~permutes[i]) == 0) totalCount++;
				} else {
					if((~permute6[p] & permutes[i]) == 0) totalCount++;
				}
			}
		}
		return totalCount;
	}
};*/


template<>
class FastPermutationCounter<7> {
	static constexpr size_t CHUNK_SIZE = 8;

	alignas(64) uint32_t permutesSH[factorial(7) / 6];
	alignas(64) uint32_t permutesA[factorial(7) / 6];
	alignas(64) uint32_t permutesB[factorial(7) / 6];
	alignas(64) uint32_t permutesC[factorial(7) / 6];

	static void swizzle(BooleanFunction<7> bf, int32_t* swizzled) {
		__m128i b = bf.bitset.data;
		swizzled[0] = (static_cast<int32_t>(_mm_extract_epi16(b, 0)) << 16) | _mm_extract_epi16(b, 7);
		swizzled[1] = (static_cast<int32_t>(_mm_extract_epi16(b, 1)) << 16) | _mm_extract_epi16(b, 6);
		swizzled[2] = (static_cast<int32_t>(_mm_extract_epi16(b, 2)) << 16) | _mm_extract_epi16(b, 5);
		swizzled[3] = (static_cast<int32_t>(_mm_extract_epi16(b, 4)) << 16) | _mm_extract_epi16(b, 3);
	}
public:
	void init(Monotonic<7> mbf) {
		size_t blockI = 0;
		for(int v0 = 0; v0 < 7; v0++) {
			Monotonic<7> mbf0 = mbf;
			if(v0 != 0) mbf0.swap(v0, 0);
			for(int v1 = 1; v1 < 7; v1++) {
				Monotonic<7> mbf1 = mbf0;
				if(v1 != 1) mbf1.swap(v1, 1);
				for(int v2 = 2; v2 < 7; v2++) {
					Monotonic<7> mbf2 = mbf1;
					if(v2 != 2) mbf2.swap(v2, 2);
					for(int v3 = 3; v3 < 7; v3++) {
						Monotonic<7> mbf3 = mbf2;
						if(v3 != 3) mbf3.swap(v3, 3);
						
						int32_t swizzled[4];
						swizzle(mbf3.bf, swizzled);

						permutesSH[blockI] = swizzled[0];
						permutesA[blockI] = swizzled[1];
						permutesB[blockI] = swizzled[2];
						permutesC[blockI] = swizzled[3];
						blockI++;
					}
				}
			}
		}
	}

	template<bool isTop>
	uint32_t countPermutes(Monotonic<7> mbf2) {
		int32_t swizzled[4];
		swizzle(mbf2.bf, swizzled);
		
		__m256i totalCounts = _mm256_setzero_si256();

		__m256i sh2 = _mm256_set1_epi32(swizzled[0]);
		__m256i a2 = _mm256_set1_epi32(swizzled[1]);
		__m256i b2 = _mm256_set1_epi32(swizzled[2]);
		__m256i c2 = _mm256_set1_epi32(swizzled[3]);

		for(unsigned int i = 0; i < factorial(7) / 6; i+=8) {
			__m256i sh1 = _mm256_load_si256((const __m256i*) &permutesSH[i]);
			__m256i a1  = _mm256_load_si256((const __m256i*) &permutesA[i]);
			__m256i b1  = _mm256_load_si256((const __m256i*) &permutesB[i]);
			__m256i c1  = _mm256_load_si256((const __m256i*) &permutesC[i]);

			__m256i sh, aa, ab, ac, ba, bb, bc, ca, cb, cc;

			#define IS_SUBSET(x, y) _mm256_cmpeq_epi32(_mm256_andnot_si256(x, y), _mm256_setzero_si256())
			if constexpr(isTop) {
				sh = IS_SUBSET(sh2, sh1);
				aa = IS_SUBSET(a2, a1);
				ab = IS_SUBSET(a2, b1);
				ac = IS_SUBSET(a2, c1);
				ba = IS_SUBSET(b2, a1);
				bb = IS_SUBSET(b2, b1);
				bc = IS_SUBSET(b2, c1);
				ca = IS_SUBSET(c2, a1);
				cb = IS_SUBSET(c2, b1);
				cc = IS_SUBSET(c2, c1);
			} else {
				sh = IS_SUBSET(sh1, sh2);
				aa = IS_SUBSET(a1, a2);
				ab = IS_SUBSET(a1, b2);
				ac = IS_SUBSET(a1, c2);
				ba = IS_SUBSET(b1, a2);
				bb = IS_SUBSET(b1, b2);
				bc = IS_SUBSET(b1, c2);
				ca = IS_SUBSET(c1, a2);
				cb = IS_SUBSET(c1, b2);
				cc = IS_SUBSET(c1, c2);
			}

			//#define AND_M256(x, y, z) _mm256_and_si256(x, _mm256_and_si256(y, z))
			//__m256i abc = AND_M256(aa, bb, cc);
			//__m256i acb = AND_M256(aa, bc, cb);
			//__m256i bac = AND_M256(ab, ba, cc);
			//__m256i bca = AND_M256(ab, bc, ca);
			//__m256i cab = AND_M256(ac, ba, cb);
			//__m256i cba = AND_M256(ac, bb, ca);
			//__m256i perm6Total = _mm256_add_epi32(_mm256_add_epi32(_mm256_add_epi32(abc, acb), _mm256_add_epi32(bac, bca)), _mm256_add_epi32(cab, cba));
			
			#define AND_ADD_AND(shared, xx, yy, xy, yx) _mm256_and_si256(shared, _mm256_add_epi32(_mm256_and_si256(xx, yy), _mm256_and_si256(xy, yx)))
			__m256i abc_acb = AND_ADD_AND(aa, bb, cc, cb, bc);
			__m256i bac_bca = AND_ADD_AND(ab, ba, cc, bc, ca);
			__m256i cab_cba = AND_ADD_AND(ac, ba, cb, bb, ca);

			__m256i perm6Total = _mm256_add_epi32(_mm256_add_epi32(abc_acb, bac_bca), cab_cba);
			totalCounts = _mm256_sub_epi32(totalCounts, _mm256_and_si256(sh, perm6Total));
		}
		
		__m256i totalCountsH1 = _mm256_hadd_epi32(totalCounts, totalCounts);
		__m256i totalCountsH2 = _mm256_hadd_epi32(totalCountsH1, totalCountsH1);
		return _mm256_extract_epi32(totalCountsH2, 0) + _mm256_extract_epi32(totalCountsH2, 4);
	}
};

struct TailPreCompute {
	static constexpr int16_t NO_PRECOMPUTE = -1;
	size_t capacity;
	// -1 values mean that this bottom is not precomputed
	NUMAArray<int16_t> precomputedCounts;

	TailPreCompute() = default;
	TailPreCompute(size_t capacity, int numaNode) : capacity(capacity), precomputedCounts(NUMAArray<int16_t>::alloc_onnode(capacity, numaNode)) {}
	void initFromHighestFullLayer(size_t highestDual, int highestFullLayerOfTop, const int8_t* highestNonEmptyLayers, const ClassInfo* classInfos) {
		assert(highestDual < capacity);
		for(size_t i = 0; i <= highestDual; i++) {
			// If the bottom fits fully under the highest full layer of the top, then all 5040 permutations will be valid. 
			// In fact, this condition goes both ways, if all 5040 permutations are valid, 
			// the bottom's highest nonempty layer is <= the highest full layer of top
			if(highestNonEmptyLayers[i] <= highestFullLayerOfTop) {
				precomputedCounts[i] = static_cast<int16_t>(classInfos[i].classSize);
			} else {
				precomputedCounts[i] = NO_PRECOMPUTE;
			}
		}
	}

	template<unsigned int Variables>
	void initFromExtendingPreviousPreCompute(const TailPreCompute& parent, size_t highestDual, int highestLayerOfTopStub, Monotonic<Variables> topStub, const Monotonic<Variables>* allMBFs, const int8_t* highestNonEmptyLayers, const ClassInfo* classInfos) {
		assert(highestLayerOfTopStub >= getLowestEmptyLayer(topStub.bf) - 1);
		assert(highestDual < capacity);

		FastPermutationCounter<Variables> counter;
		counter.init(topStub);
		constexpr int VAR_FACTORIAL = factorial(Variables);
		for(size_t i = 0; i <= highestDual; i++) {
			if(parent.precomputedCounts[i] != TailPreCompute::NO_PRECOMPUTE) {
				this->precomputedCounts[i] = parent.precomputedCounts[i];
			} else if(highestNonEmptyLayers[i] <= highestLayerOfTopStub) {
				this->precomputedCounts[i] = counter.template countPermutes<false>(allMBFs[i]) * classInfos[i].classSize / VAR_FACTORIAL;
			} else {
				this->precomputedCounts[i] = TailPreCompute::NO_PRECOMPUTE;
			}
		}
	}
};

static NodeIndex getHighestDual(const std::vector<JobTopInfo>& tops) {
	NodeIndex highestDual = 0;
	for(JobTopInfo info : tops) {
		if(info.topDual > highestDual) highestDual = info.topDual;
	}
	return highestDual;
}

static int8_t getHighestLayerOfTopVector(const std::vector<JobTopInfo>& tops, const int8_t* highestLayerBuffer) {
	int8_t highest = 0;
	for(JobTopInfo inf : tops) {
		if(highestLayerBuffer[inf.top] > highest) highest = highestLayerBuffer[inf.top];
	}
	return highest;
}

template<unsigned int Variables>
struct std::hash<BooleanFunction<Variables>> {
	size_t operator()(const BooleanFunction<Variables>& bf) const noexcept {
		return bf.hash();
	}
};

template<unsigned int Variables>
struct std::hash<Monotonic<Variables>> {
	size_t operator()(const Monotonic<Variables>& mbf) const noexcept {
		return mbf.bf.hash();
	}
};

template<unsigned int Variables>
struct std::hash<AntiChain<Variables>> {
	size_t operator()(const AntiChain<Variables>& ac) const noexcept {
		return ac.bf.hash();
	}
};

template<unsigned int Variables>
struct std::hash<Layer<Variables>> {
	size_t operator()(const Layer<Variables>& layer) const noexcept {
		return layer.bf.hash();
	}
};


template<typename K, typename V>
std::vector<std::pair<K, V>> mapToVector(std::unordered_map<K, V>&& map) {
	std::vector<std::pair<K, V>> result;
	result.reserve(map.size());
	for(const std::pair<K, V>& entry : std::move(map)) {
		result.push_back(entry);
	}
	return result;
}

template<unsigned int Variables>
static std::unordered_map<Monotonic<Variables>, std::vector<JobTopInfo>> categoriseTops(int upToLayer, const std::vector<JobTopInfo>& tops, const Monotonic<Variables>* allMBFs) {
	std::unordered_map<Monotonic<Variables>, std::vector<JobTopInfo>> result;

	BooleanFunction<Variables> topMask = BooleanFunction<Variables>::empty();
	for(int l = 0; l <= upToLayer; l++) {
		topMask.bitset |= BooleanFunction<Variables>::layerMask(l);
	}

	for(JobTopInfo topInfo : tops) {
		Monotonic<Variables> topMBF = allMBFs[topInfo.top];
		topMBF.bf &= topMask;

		Monotonic<Variables> canonized = topMBF.canonize();

		auto found = result.find(canonized);
		if(found != result.end()) {
			found->second.push_back(topInfo);
		} else {
			result.emplace(canonized, std::vector<JobTopInfo>{topInfo});
		}
	}

	size_t totalSizeInSubParts = 0;
	for(const auto& entry : result) {
		totalSizeInSubParts += entry.second.size();
	}

	if(tops.size() != totalSizeInSubParts) {
		std::cerr << "Error, categoriseTops didn't properly divide the tops!: total=" + std::to_string(tops.size()) + ", splitsum=" + std::to_string(totalSizeInSubParts) + "\n" << std::flush;
		exit(-1);
	}

	return result;
}

static std::atomic<int> totalNumberOfCheckedTops;
/*static std::atomic<size_t> totalBenchmarkPoints;


struct Benchmarker{
	std::atomic<uint64_t> v;
	Benchmarker() {
		v.store(0);
		totalBenchmarkPoints.store(0);
	}

	uint64_t getTotalMillis() const {
		return v.load() >> 20;
	}

	uint64_t getCount() const {
		return v.load() & ((1ULL << 20) - 1);
	}

	static Benchmarker categoriseTopsTime;
	static Benchmarker runningSumTime;
	static Benchmarker extendingTime;
	static Benchmarker otherTime;

	void addBenchData(std::chrono::duration<std::chrono::nanoseconds, > time) {
		v.fetch_add(((uint64_t(time.count().count()) / 1000000) << 20) + 1);

		if(totalBenchmarkPoints.fetch_add(1) % 1000 == 0) {
			std::cout << "categoriseTopsTime: " + std::to_string(categoriseTopsTime.getTotalMillis()) + "ms #" + std::to_string(categoriseTopsTime.getCount())
			+ "\nrunningSumTime: " + std::to_string(runningSumTime.getTotalMillis()) + "ms #" + std::to_string(runningSumTime.getCount())
			+ "\nextendingTime: " + std::to_string(extendingTime.getTotalMillis()) + "ms #" + std::to_string(extendingTime.getCount())
			+ "\notherTime: " + std::to_string(otherTime.getTotalMillis()) + "ms #" + std::to_string(otherTime.getCount()) + "\n" << std::flush;
		}	
	}
};*/


template<unsigned int Variables>
struct TailThreadContext {
	TailPreCompute preComputeBuffers[Variables];

	const Monotonic<Variables>* allMBFs;
	const ClassInfo* classInfos;
	const int8_t* highestNonEmptyLayers;
	const std::vector<BetaSumPair>* fullResultListPtr;

	NUMAArray<NodeIndex> exceptionBuffer;

	TailThreadContext(size_t numBottoms, int numaNode, const Monotonic<Variables>* allMBFs, const ClassInfo* classInfos, const int8_t* highestNonEmptyLayers, const std::vector<BetaSumPair>& fullResultList) :
		allMBFs(allMBFs), 
		classInfos(classInfos),
		highestNonEmptyLayers(highestNonEmptyLayers),
		fullResultListPtr(&fullResultList) {
		for(TailPreCompute& pre : preComputeBuffers) {
			pre = TailPreCompute(numBottoms, numaNode);
		}

		exceptionBuffer = NUMAArray<NodeIndex>::alloc_onnode(numBottoms, numaNode);
	}

	void checkWithRunningSumsOptimization(
		const TailPreCompute& tailPreCompute,
		const std::vector<JobTopInfo>& tops) { // Expects tops to be sorted
		constexpr int VAR_FACTORIAL = factorial(Variables);

		uint64_t runningSum = 0;
		NodeIndex curBot = 0;
		size_t exceptionUpTo = 0;
		
		for(JobTopInfo info : tops) {
			for(; curBot < info.topDual; curBot++) {
				if(tailPreCompute.precomputedCounts[curBot] != TailPreCompute::NO_PRECOMPUTE) {
					runningSum += tailPreCompute.precomputedCounts[curBot];
				} else {
					exceptionBuffer[exceptionUpTo++] = curBot;
				}
			}

			FastPermutationCounter<Variables> counter;
			if(exceptionUpTo > 0) counter.init(allMBFs[info.top]);

			uint64_t totalExceptionSum = 0;
			for(size_t i = 0; i < exceptionUpTo; i++) {
				NodeIndex exceptionBot = exceptionBuffer[i];
				totalExceptionSum += counter.template countPermutes<false>(allMBFs[exceptionBot]) * classInfos[exceptionBot].classSize;
			}

			uint64_t totalSum = runningSum * VAR_FACTORIAL + totalExceptionSum;

			const std::vector<BetaSumPair>& fullResultsList = *fullResultListPtr;
			if(totalSum != fullResultsList[info.top].betaSum.countedIntervalSizeDown) {
				std::cout << "Top " + std::to_string(info.top) + " is certainly wrong! Bad intervalSizeDown: (should be: " << totalSum / VAR_FACTORIAL << ", found: " << fullResultsList[info.top].betaSum.countedIntervalSizeDown / VAR_FACTORIAL << ") // classSize=" << classInfos[info.top].classSize << ", bs_dual=" << fullResultsList[info.top].betaSumDualDedup.countedIntervalSizeDown << std::endl;
			} else {
				//std::cout << "Top " + std::to_string(info.top) + " correct.\n" << std::flush;
			}
			if(totalNumberOfCheckedTops.fetch_add(1) % (1024*128) == 0) {
				std::cout << totalNumberOfCheckedTops.load() << std::endl;
			}
		}
	}

	uint64_t countExceptions(size_t numExceptions, NodeIndex topIdx) {
		FastPermutationCounter<Variables> counter;
		counter.init(allMBFs[topIdx]);

		uint64_t totalExceptionSum = 0;
		for(size_t i = 0; i < numExceptions; i++) {
			NodeIndex exceptionBot = exceptionBuffer[i];
			totalExceptionSum += counter.template countPermutes<false>(allMBFs[exceptionBot]) * classInfos[exceptionBot].classSize;
		}
		return totalExceptionSum;
	}

	void checkWithRunningSumsOptimizationAndSharedTopStub(
		const TailPreCompute& tailPreCompute,
		const std::vector<JobTopInfo>& tops,
		Monotonic<Variables> topStub,
		int topStubHighestLayer) { // Expects tops to be sorted
		constexpr int VAR_FACTORIAL = factorial(Variables);

		uint64_t runningSum = 0;
		NodeIndex curBot = 0;
		size_t exceptionUpTo = 0;

		FastPermutationCounter<Variables> topStubCounter;
		topStubCounter.init(topStub);
		for(JobTopInfo info : tops) {
			for(; curBot < info.topDual; curBot++) {
				if(tailPreCompute.precomputedCounts[curBot] != TailPreCompute::NO_PRECOMPUTE) {
					runningSum += tailPreCompute.precomputedCounts[curBot] * VAR_FACTORIAL;
				} else if(highestNonEmptyLayers[curBot] <= topStubHighestLayer) {
					runningSum += topStubCounter.template countPermutes<false>(allMBFs[curBot]) * classInfos[curBot].classSize;
				} else {
					exceptionBuffer[exceptionUpTo++] = curBot;
				}
			}

			
			uint64_t totalExceptionSum = 0;

			if(exceptionUpTo > 0 && highestNonEmptyLayers[info.top] > topStubHighestLayer) totalExceptionSum = countExceptions(exceptionUpTo, info.top);

			uint64_t totalSum = runningSum + totalExceptionSum;

			const std::vector<BetaSumPair>& fullResultsList = *fullResultListPtr;
			if(totalSum != fullResultsList[info.top].betaSum.countedIntervalSizeDown) {
				std::cout << "Top " + std::to_string(info.top) + " is certainly wrong! Bad intervalSizeDown: (should be: " << totalSum / VAR_FACTORIAL << ", found: " << fullResultsList[info.top].betaSum.countedIntervalSizeDown / VAR_FACTORIAL << ") // classSize=" << classInfos[info.top].classSize << ", bs_dual=" << fullResultsList[info.top].betaSumDualDedup.countedIntervalSizeDown << std::endl;
			} else {
				//std::cout << "Top " + std::to_string(info.top) + " correct.\n" << std::flush;
			}
		
			if(totalNumberOfCheckedTops.fetch_add(1) % (1024*16) == 0) {
				std::cout << totalNumberOfCheckedTops.load() << std::endl;
			}
		}
	}

	void checkTopsTailRecurse(const TailPreCompute& parentPreCompute, int recurseDepth, const std::vector<JobTopInfo>& tops, Monotonic<Variables> topStub, int topStubHighestLayer) {
		//auto startTime = std::chrono::high_resolution_clock::now();

		int highestLayerOfTops = getHighestLayerOfTopVector(tops, highestNonEmptyLayers);
		//auto highestLayerOfTopsDoneTime = std::chrono::high_resolution_clock::now();
		//Benchmarker::otherTime.addBenchData(highestLayerOfTopsDoneTime - startTime);
		
		
		if(highestLayerOfTops > topStubHighestLayer + 1) {
			TailPreCompute& childPreCompute = this->preComputeBuffers[recurseDepth];
			childPreCompute.initFromExtendingPreviousPreCompute(parentPreCompute, getHighestDual(tops), topStubHighestLayer, topStub, allMBFs, highestNonEmptyLayers, classInfos);
			//auto extendingDoneTime = std::chrono::high_resolution_clock::now();
			//Benchmarker::extendingTime.addBenchData(extendingDoneTime - highestLayerOfTopsDoneTime);

			std::unordered_map<Monotonic<Variables>, std::vector<JobTopInfo>> topCategories = categoriseTops(topStubHighestLayer + 1, tops, allMBFs);
			//auto categoriseTime = std::chrono::high_resolution_clock::now();
			//Benchmarker::categoriseTopsTime.addBenchData(categoriseTime - extendingDoneTime);

			//size_t countedSize = 0;

			for(const auto& entry : topCategories) {
				Monotonic<Variables> sharedTopStub = entry.first;
				const std::vector<JobTopInfo>& topsThatShareThisStub = entry.second;
				//countedSize += topsThatShareThisStub.size();

				this->checkTopsTailRecurse(childPreCompute, recurseDepth+1, topsThatShareThisStub, sharedTopStub, topStubHighestLayer+1);
			}
			//if(countedSize != tops.size()) std::cerr << "ERROR: Split tops different size than merged tops!\n" << std::flush;
		} else {
			this->checkWithRunningSumsOptimizationAndSharedTopStub(parentPreCompute, tops, topStub, topStubHighestLayer);
			//auto runningSumTime = std::chrono::high_resolution_clock::now();
			//Benchmarker::runningSumTime.addBenchData(runningSumTime - highestLayerOfTopsDoneTime);
		}
	}
};

// Don't count stuff that's been covered by checkTopsSecondHalfWithSwapper
template<unsigned int Variables>
static void checkTopsTail(const std::vector<BetaSumPair>& fullResultsList, const FlatNode* flatNodes, const ClassInfo* classInfos, const Monotonic<Variables>* allMBFs, int offsetLayerFromCenter, int highestFullLayerOfTopTarget = -1, int divisionCount = 1, int selectedDivision = 0) {
	std::cout << "Checking tops tail" << std::endl;

	constexpr int SKIP_INITIAL = 1;	

	// Start halfway, because lower tops are trivial to check
	NodeIndex startAt = flatNodeLayerOffsets[Variables][(1 << Variables) / 2 + 1 + offsetLayerFromCenter];
	//NodeIndex bottomsEndAt = flatNodeLayerOffsets[Variables][(1 << Variables) / 2 - offsetLayerFromCenter];

	std::vector<JobTopInfo> topToCheckVectors[Variables+1];
	for(NodeIndex top = startAt; top < mbfCounts[Variables]; top++) {
		Monotonic<Variables> topMBF = allMBFs[top];
		JobTopInfo newElem;
		newElem.top = top;
		newElem.topDual = static_cast<NodeIndex>(flatNodes[top].dual);
		int highestFullLayer = getHighestFullLayer(topMBF.bf);
		topToCheckVectors[highestFullLayer].push_back(std::move(newElem));
	}

	std::unique_ptr<int8_t[]> highestNonEmptyLayers(new int8_t[mbfCounts[Variables]]);
	for(size_t i = 0; i < mbfCounts[Variables]; i++) {
		highestNonEmptyLayers[i] = getLowestEmptyLayer(allMBFs[i].bf) - 1;
	}

	std::cout << "cached data prepared." << std::endl;

	totalNumberOfCheckedTops.store(0);
	size_t totalTopsProcessedAccordingToCategories = 0;
	for(int highestFullLayerOfTop = Variables; highestFullLayerOfTop >= 0; highestFullLayerOfTop--) {
		if(highestFullLayerOfTopTarget != -1 && highestFullLayerOfTop != highestFullLayerOfTopTarget) continue;

		std::vector<JobTopInfo>& curTopsVec = topToCheckVectors[highestFullLayerOfTop];
		std::cout << "Highest full layer: " << highestFullLayerOfTop << std::endl;
		std::cout << "Number of tops with this: " << curTopsVec.size() << std::endl;
		std::cout << "Sorting tops by ascending duals..." << std::endl;
		std::sort(curTopsVec.begin(), curTopsVec.end(), [](JobTopInfo a, JobTopInfo b){return a.topDual < b.topDual;});
		std::cout << "Sorting tops done." << std::endl;

		size_t highestDual = getHighestDual(curTopsVec);
		TailPreCompute preCompute(highestDual+1, 0);

		std::cout << "initFromHighestFullLayer..." << std::endl;
		preCompute.initFromHighestFullLayer(highestDual, highestFullLayerOfTop, highestNonEmptyLayers.get(), classInfos);
		std::cout << "initFromHighestFullLayer done." << std::endl;
		
		if(highestFullLayerOfTop < int(Variables) - 2) { // Don't bother using advanced techniques for really high tops
			std::cout << "Categorising tops..." << std::endl;
			std::unordered_map<Monotonic<Variables>, std::vector<JobTopInfo>> topCategories = categoriseTops(highestFullLayerOfTop+1+SKIP_INITIAL, curTopsVec, allMBFs);
			std::cout << "Categorising tops done. " << topCategories.size() << " categories found." << std::endl;

			/*std::vector<std::pair<Monotonic<Variables>, std::vector<JobTopInfo>>> topCategoriesVector = mapToVector(topCategories);
			std::cout << "Converted to vector. " << topCategoriesVector.size() << " top categories found!" << std::endl;

			std::sort(topCategoriesVector.begin(), topCategoriesVector.end(), [](const auto& a, const auto& b){
				return a.second.size() > b.second.size(); // Sort big vectors first, that way we do expensive stuff first
			});*/

			std::vector<std::pair<Monotonic<Variables>, std::vector<JobTopInfo>>> topCategoriesAsVector = mapToVector(std::move(topCategories));
			std::default_random_engine generator(0xB00B1E5);
			std::shuffle(topCategoriesAsVector.begin(), topCategoriesAsVector.end(), generator);
			std::atomic<size_t> divisionStartAt;
			divisionStartAt.store(selectedDivision * topCategoriesAsVector.size() / divisionCount);
			size_t divisionEndAt = (selectedDivision+1) * topCategoriesAsVector.size() / divisionCount;


			runInParallelOnAllCores([&](int threadID){
				TailThreadContext<Variables> threadContext(highestDual+1, threadID / 16, allMBFs, classInfos, highestNonEmptyLayers.get(), fullResultsList);

				size_t claimedCategoryI;
				while((claimedCategoryI = divisionStartAt.fetch_add(1)) < divisionEndAt) {
					Monotonic<Variables> sharedTopStub = topCategoriesAsVector[claimedCategoryI].first;
					const std::vector<JobTopInfo>& topsThatShareThisStub = topCategoriesAsVector[claimedCategoryI].second;
					if(topsThatShareThisStub.size() <= 2) {continue;}
					//std::cout << "Category Size: " + std::to_string(topsThatShareThisStub.size()) + "\n";
					totalTopsProcessedAccordingToCategories += topsThatShareThisStub.size();

					threadContext.checkTopsTailRecurse(preCompute, 0, topsThatShareThisStub, sharedTopStub, highestFullLayerOfTop+1+SKIP_INITIAL);
				}
			});
		} else {
			std::cout << "Running checkWithRunningSumsOptimization" << std::endl;
			TailThreadContext<Variables> threadContext(highestDual+1, 0, allMBFs, classInfos, highestNonEmptyLayers.get(), fullResultsList);
			threadContext.checkWithRunningSumsOptimization(preCompute, curTopsVec);
			totalTopsProcessedAccordingToCategories += curTopsVec.size();
		}
	}

	std::cout << "Total number of tops checked: " << totalNumberOfCheckedTops.load() << std::endl;
	std::cout << "Number of tops checked in this highestFullLayer: " << totalTopsProcessedAccordingToCategories << std::endl;
}

template<bool baseIsBottom, unsigned int Variables>
uint64_t sumSegmentPermutations(Monotonic<Variables> base, const ClassInfo* classInfos, const Monotonic<Variables>* allMBFs, NodeIndex from, NodeIndex to) {
	uint64_t totalSum = 0;

	FastPermutationCounter<Variables> permutationCounter;
	permutationCounter.init(base);

	for(NodeIndex i = from; i < to; i++) {
		Monotonic<Variables> bot = allMBFs[i];
		uint64_t classSize = classInfos[i].classSize;
		uint64_t count = permutationCounter.template countPermutes<baseIsBottom>(bot);

		/*uint64_t correctCount = 0;
		bot.forEachPermutation([&](Monotonic<Variables> permuted){if(permuted <= base) correctCount++;});
		assert(count == correctCount);*/

		uint64_t thisTerm = count * classSize;
		totalSum += thisTerm;
		assert(thisTerm % factorial(Variables) == 0);
	}
	return totalSum;
}

template<bool baseIsBottom, unsigned int Variables>
uint64_t sumSegmentPermutationsOnAllCores(Monotonic<Variables> base, const ClassInfo* classInfos, const Monotonic<Variables>* allMBFs, NodeIndex from, NodeIndex to) {
	std::atomic<uint64_t> totalSum;
	totalSum.store(0);

	FastPermutationCounter<Variables> permutationCounter;
	permutationCounter.init(base);

	iterRangeInParallelBlocks(std::thread::hardware_concurrency(), CPUAffinityType::CORE, from, to, 4096, [&](int core, NodeIndex iStart, NodeIndex iEnd) {
		uint64_t localSum = 0;
		for(NodeIndex i = iStart; i != iEnd; i++) {
			Monotonic<Variables> bot = allMBFs[i];
			uint64_t classSize = classInfos[i].classSize;
			uint64_t count = permutationCounter.template countPermutes<baseIsBottom>(bot);

			/*uint64_t correctCount = 0;
			bot.forEachPermutation([&](Monotonic<Variables> permuted){if(permuted <= base) correctCount++;});
			assert(count == correctCount);*/

			uint64_t thisTerm = count * classSize;
			localSum += thisTerm;
			assert(thisTerm % factorial(Variables) == 0);
		}
		totalSum.fetch_add(localSum);
	});
	return totalSum.load();
}

#include "../dedelib/pawelski.h"
template<unsigned int Variables>
static void checkTopsTailUsingPawelskiIntervalCounting(const std::vector<BetaSumPair>& fullResultsList, const FlatNode* flatNodes, const ClassInfo* classInfos, const Monotonic<Variables>* allMBFs, NodeIndex firstMBF, NodeIndex mbfToCheckEnd) {
	std::cout << "Checking tops tail with Pawelski IntervalCounting" << std::endl;

	PawelskiPartialIntervalCounter<Variables> partialIntervalCounter;
	std::cout << "Initialized partialIntervalCounter" << std::endl;

	//for(NodeIndex topI = firstMBF; topI < mbfToCheckEnd; topI++) {
	iterRangeInParallelBlocksOnAllCores(firstMBF, mbfToCheckEnd, 64, [&](int core, NodeIndex topI){
		if(topI % 100000 == 0) std::cout << std::to_string(topI) + "\n" << std::flush;
		int topLayer = getFlatLayerOfIndex(Variables, topI);
		int topDualLayer = (1 << Variables) - topLayer;
		NodeIndex topDual = flatNodes[topI].dual;
		NodeIndex topDualLayerStart = flatNodeLayerOffsets[Variables][topDualLayer];
		NodeIndex topDualLayerEnd = flatNodeLayerOffsets[Variables][topDualLayer+1];

		// Optimization, include dual layer in partialIntervalCounter too, to reduce number of countPermutes calls
		bool includeDualLayerOptimization = double(topDual - topDualLayerStart) / (topDualLayerEnd - topDualLayerStart) >= 0.5;
		
		uint64_t totalSum;

		if(includeDualLayerOptimization) {
			uint64_t intervalSizeBelowDualLayer = partialIntervalCounter.intervalToTopSizeAboveSize(allMBFs[topDual], topLayer);
			uint64_t remainingIntervalSize = sumSegmentPermutations<false>(allMBFs[topI], classInfos, allMBFs, topDual, topDualLayerEnd);
			totalSum = intervalSizeBelowDualLayer * factorial(Variables) - remainingIntervalSize;
		} else {
			uint64_t intervalSizeBelowDualLayer = partialIntervalCounter.intervalToTopSizeAboveSize(allMBFs[topDual], topLayer+1);
			uint64_t remainingIntervalSize = sumSegmentPermutations<false>(allMBFs[topI], classInfos, allMBFs, topDualLayerStart, topDual);
			totalSum = intervalSizeBelowDualLayer * factorial(Variables) + remainingIntervalSize;
		}
		
		//std::cout << intervalSizeBelowDualLayer << ", " << remainingIntervalSize / factorial(Variables) << std::endl;
		if(totalSum != fullResultsList[topI].betaSum.countedIntervalSizeDown) {
			std::cout << "PawelskiTail Sais: Top " + std::to_string(topI) + " is certainly wrong! Bad intervalSizeDown: (should be: " << totalSum / factorial(Variables) << ", found: " << fullResultsList[topI].betaSum.countedIntervalSizeDown / factorial(Variables) << ") // classSize=" << classInfos[topI].classSize << ", bs_dual=" << fullResultsList[topI].betaSumDualDedup.countedIntervalSizeDown << std::endl;
			// exit(-1);
		} else {
			//std::cout << "Top " + std::to_string(topI) + " correct.\n" << std::flush;
		}
	});
}

#include "../dedelib/bitSet.h"
template<size_t Width>
struct MBFSwapper {
	alignas(4096) BitSet<Width>* __restrict__ upper;
	alignas(4096) BitSet<Width>* __restrict__ lower;
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

		FastPermutationCounter<Variables> permutationCounter;
		permutationCounter.init(mbfI);
		
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
				unsigned int countForThisBot = permutationCounter.countPermutes<false>(inbetweenMBF);
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
static void checkResultsBlock(NodeIndex startAt, size_t numInThisBlock, MBFSwapper<MaxBlockSize>& swapper, const BetaSumPair* fullResultsList, const FlatNode* flatNodes, const ClassInfo* classInfos, const Monotonic<Variables>* allMBFs, const uint32_t* flatLinks, ThreadPool& pool) {
	int blockLayer = getFlatLayerOfIndex(Variables, startAt);

	if(blockLayer <= (1 << Variables) / 2) {
		std::cerr << "Assertion Error. This function should only be run for tops in the upper half. Top " << startAt << " has layer " << blockLayer << std::endl;
		exit(-1);
	}
	int dualsLayer = (1 << Variables) - blockLayer;
	swapper.initRange(startAt - flatNodeLayerOffsets[Variables][blockLayer], numInThisBlock, layerSizes[Variables][blockLayer]);

	std::atomic<uint64_t> totalIntervalSizesDown[MaxBlockSize];
	for(size_t i = 0; i < numInThisBlock; i++) {
		totalIntervalSizesDown[i].store(fullResultsList[startAt + i].betaSum.countedIntervalSizeDown
			// + fullResultsList[startAt + i].betaSumDualDedup.countedIntervalSizeDown
			 + factorial(Variables)); // For the top itself, we don't include it in the swapper run
	}

	for(int layer = blockLayer-1; layer >= dualsLayer; layer--) {
		//std::cout << "Iter layer " << layer << std::endl;

		NodeIndex thisLayerSize = layerSizes[Variables][layer];
		NodeIndex thisLayerOffset = flatNodeLayerOffsets[Variables][layer];
		size_t numLinksBetweenLayers = linkCounts[Variables][layer];

		swapper.followLinksToNextLayer(flatLinks + flatLinkOffsets[Variables][(1 << Variables) - layer-1], thisLayerSize, numLinksBetweenLayers); // mbfStructure is ordered in referse, seems I thought that optimization worth the confusion

		pool.iterRangeInParallel(thisLayerSize, 1024*64, [&](NodeIndex localBottomStart, NodeIndex numLocalBottomsInThisBlock){
			uint64_t subtotalIntervalSizesDown[MaxBlockSize];
			for(size_t i = 0; i < MaxBlockSize; i++) {
				subtotalIntervalSizesDown[i] = 0;
			}
			for(NodeIndex localBottom = localBottomStart; localBottom < localBottomStart + numLocalBottomsInThisBlock; localBottom++) {

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

					FastPermutationCounter<Variables> permutationCounter;
					permutationCounter.init(botMBF);
					uint64_t botClassSize = classInfos[realBottom].classSize;

					for(NodeIndex* localTop = localTopsForThisBottom; localTop != localTopsForThisBottom + numLocalTopsForThisBottom; localTop++) {
						NodeIndex realTop = startAt + *localTop;
						
						Monotonic<Variables> topMBF = allMBFs[realTop];

						uint64_t countForThisBotTop = permutationCounter.template countPermutes<true>(topMBF);
						assert(countForThisBotTop != 0);
						subtotalIntervalSizesDown[*localTop] += botClassSize * countForThisBotTop;
					}
				}
			}
			for(size_t i = 0; i < MaxBlockSize; i++) {
				if(subtotalIntervalSizesDown[i] != 0) totalIntervalSizesDown[i].fetch_add(subtotalIntervalSizesDown[i]);
			}
		});
	}

	//std::cout << "Checking tops..." << std::endl;
	for(size_t localI = 0; localI < numInThisBlock; localI++) {
		uint64_t foundIntervalSize = totalIntervalSizesDown[localI].load();
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
static void checkTopsSecondHalfWithSwapper(const BetaSumPair* fullResultsList, const FlatNode* flatNodes, const ClassInfo* classInfos, const Monotonic<Variables>* allMBFs, const uint32_t* flatLinks, size_t layer, NodeIndex layerTopsStart, NodeIndex layerTopsEnd) {

	std::cout << "Checking intervalSizeDown SECOND HALF... (" << flatNodeLayerOffsets[Variables][(1 << Variables) / 2 + 1] << " - " << mbfCounts[Variables] << ")" << std::endl;

	//for(size_t layer = (1 << Variables) / 2 + 1; layer < (1 << Variables); layer++) {
		std::cout << "Swapper Layer " << layer << std::endl;
		//NodeIndex layerTopsStart = flatNodeLayerOffsets[Variables][layer];
		//NodeIndex layerTopsEnd = flatNodeLayerOffsets[Variables][layer+1];
		NodeIndex numberOfTopsInLayer = layerTopsEnd - layerTopsStart;

		std::atomic<NodeIndex> curTopStart;
		curTopStart.store(0);

		runInParallel(16, CPUAffinityType::COMPLEX, [&](int complexI){
			constexpr size_t MAX_BLOCK_SIZE = 512*4;// Cache lines
			ThreadPool pool(8, complexI * 8);
			MBFSwapper<MAX_BLOCK_SIZE> swapper(Variables, complexI / 2);

			while(true) {
				NodeIndex blockStart = curTopStart.fetch_add(MAX_BLOCK_SIZE);

				if(blockStart >= numberOfTopsInLayer) {
					break;
				}
				if(blockStart % (1024*128) == 0) std::cout << "Block> " << layerTopsStart + blockStart << std::endl;
				size_t iterSize = MAX_BLOCK_SIZE;
				if(blockStart + MAX_BLOCK_SIZE > numberOfTopsInLayer) {
					iterSize = numberOfTopsInLayer - blockStart;
				}
				
				checkResultsBlock<Variables, MAX_BLOCK_SIZE>(layerTopsStart + blockStart, iterSize, swapper, fullResultsList, flatNodes, classInfos, allMBFs, flatLinks, pool);
			}
		});

		/*ThreadPool pool(8);
		MBFSwapper<MAX_BLOCK_SIZE> swapper(Variables, 0);
		NodeIndex topsBatch;
		for(topsBatch = layerTopsStart; topsBatch + MAX_BLOCK_SIZE < layerTopsEnd; topsBatch+=MAX_BLOCK_SIZE) {
			if((topsBatch - layerTopsStart) % (1024*64) == 0) std::cout << topsBatch;
			checkResultsBlock<Variables, MAX_BLOCK_SIZE>(topsBatch, MAX_BLOCK_SIZE, swapper, fullResultsList, flatNodes, classInfos, allMBFs, flatLinks, pool);
		}
		checkResultsBlock<Variables, MAX_BLOCK_SIZE>(topsBatch, layerTopsEnd - topsBatch, swapper, fullResultsList, flatNodes, classInfos, allMBFs, flatLinks, pool);
		*/
	//}
}

static std::vector<BetaSumPair> loadFullResultsList(unsigned int Variables, const std::string& file) {
	std::vector<BetaSumPair> fullResultsList(mbfCounts[Variables]);
	{
		std::ifstream allResultsFile(file);
		allResultsFile.read(reinterpret_cast<char*>(&fullResultsList[0]), sizeof(BetaSumPair) * mbfCounts[Variables]);
	}
	if(fullResultsList[20].betaSum.betaSum == 0) {
		std::cerr << "results file not read properly!\n";
		exit(-1);
	}
	return fullResultsList;
}

void findErrorInNearlyCorrectResultsCheckBasicsAndFirstHalf(const std::vector<std::string>& args) {
	unsigned int Variables = std::stoi(args[0]);
	std::string allResultsPath = args[1];

	std::cout << "Loading fullResultsList..." << std::endl;
	std::vector<BetaSumPair> fullResultsList = loadFullResultsList(Variables, allResultsPath);
	
	std::cout << "Running check0AndModuloVariables..." << std::endl;
	check0AndModuloVariables(Variables, fullResultsList);

	std::cout << "Loading flatNodes..." << std::endl;
	const FlatNode* flatNodes = readFlatBuffer<FlatNode>(FileName::flatNodes(Variables), mbfCounts[Variables]);
	std::cout << "Loading classInfos..." << std::endl;
	const ClassInfo* classInfos = readFlatBuffer<ClassInfo>(FileName::flatClassInfo(Variables), mbfCounts[Variables]);

	std::cout << "Running checkTopsFirstHalf..." << std::endl;
	checkTopsFirstHalf(Variables, fullResultsList, flatNodes, classInfos);

	freeFlatBuffer(flatNodes, mbfCounts[Variables]);
	freeFlatBuffer(classInfos, mbfCounts[Variables]);
}

template<unsigned int Variables>
void findErrorInNearlyCorrectResultsCheckMiddleLayer(const std::vector<std::string>& args) {
	std::string allResultsPath = args[0];
	int layer = std::stoi(args[1]);
	NodeIndex layerTopsStart = flatNodeLayerOffsets[Variables][layer];
	NodeIndex layerTopsEnd = flatNodeLayerOffsets[Variables][layer+1];
	NodeIndex layerSize = layerTopsEnd - layerTopsStart;

	int numParts = 1;
	int selectedPart = 0;
	if(args.size() >= 3) {
		numParts = std::stoi(args[2]);
		selectedPart = std::stoi(args[3]);
	}
	NodeIndex selectedStart = layerTopsStart + layerSize * selectedPart / numParts;
	NodeIndex selectedEnd = layerTopsStart + layerSize * (selectedPart+1) / numParts;
	std::cout << "Processing Part " << selectedPart << "/" << numParts << " of layer " << layer << ": from idx " << selectedStart << " to " << selectedEnd << " = " << (selectedEnd - selectedStart) << std::endl;
	
	std::cout << "Loading fullResultsList..." << std::endl;
	//std::vector<BetaSumPair> fullResultsList = loadFullResultsList(Variables, allResultsPath);
	const BetaSumPair* fullResultsList = readFlatBuffer<BetaSumPair>(allResultsPath, mbfCounts[Variables]);
	
	std::cout << "Loading flatNodes..." << std::endl;
	const FlatNode* flatNodes = readFlatBuffer<FlatNode>(FileName::flatNodes(Variables), mbfCounts[Variables]);
	std::cout << "Loading classInfos..." << std::endl;
	const ClassInfo* classInfos = readFlatBuffer<ClassInfo>(FileName::flatClassInfo(Variables), mbfCounts[Variables]);
	std::cout << "Loading allMBFs..." << std::endl;
	const Monotonic<Variables>* allMBFs = readFlatBuffer<Monotonic<Variables>>(FileName::flatMBFs(Variables), mbfCounts[Variables]);
	std::cout << "Loading flatLinks..." << std::endl;
	const uint32_t* flatLinks = readFlatBuffer<uint32_t>(FileName::mbfStructure(Variables), getTotalLinkCount(Variables));
	
	//checkTopsSecondHalfNaive<Variables>(fullResultsList, flatNodes, classInfos, allMBFs);
	
	std::cout << "Running checkTopsSecondHalfWithSwapper..." << std::endl;
	checkTopsSecondHalfWithSwapper<Variables>(fullResultsList, flatNodes, classInfos, allMBFs, flatLinks, layer, selectedStart, selectedEnd);

	std::cout << "All Checks done" << std::endl;

	freeFlatBuffer(flatNodes, mbfCounts[Variables]);
	freeFlatBuffer(classInfos, mbfCounts[Variables]);
	freeFlatBuffer(allMBFs, mbfCounts[Variables]);
	freeFlatBuffer(flatLinks, getTotalLinkCount(Variables));
}

template<unsigned int Variables>
void findErrorInNearlyCorrectResultsCheckTail(const std::vector<std::string>& args) {
	std::string allResultsPath = args[0];

	std::cout << "Loading fullResultsList..." << std::endl;
	std::vector<BetaSumPair> fullResultsList = loadFullResultsList(Variables, allResultsPath);
	std::cout << "Loading flatNodes..." << std::endl;
	const FlatNode* flatNodes = readFlatBuffer<FlatNode>(FileName::flatNodes(Variables), mbfCounts[Variables]);
	std::cout << "Loading classInfos..." << std::endl;
	const ClassInfo* classInfos = readFlatBuffer<ClassInfo>(FileName::flatClassInfo(Variables), mbfCounts[Variables]);
	std::cout << "Loading allMBFs..." << std::endl;
	const Monotonic<Variables>* allMBFs = readFlatBuffer<Monotonic<Variables>>(FileName::flatMBFs(Variables), mbfCounts[Variables]);
	std::cout << "Running checkTopsTail..." << std::endl;
	int layer = std::stoi(args[1]);
	int highestFullLayerOfTopTarget = -1;
	int divisionCount = 1;
	int selectedDivision = 0;
	if(args.size() >= 3) {
		highestFullLayerOfTopTarget = std::stoi(args[2]);
		if(args.size() >= 4) {
			divisionCount = std::stoi(args[3]);
			selectedDivision = std::stoi(args[4]);
		}
	}
	checkTopsTail<Variables>(fullResultsList, flatNodes, classInfos, allMBFs, layer, highestFullLayerOfTopTarget, divisionCount, selectedDivision);

	freeFlatBuffer(flatNodes, mbfCounts[Variables]);
	freeFlatBuffer(classInfos, mbfCounts[Variables]);
	freeFlatBuffer(allMBFs, mbfCounts[Variables]);
}

template<unsigned int Variables>
void findErrorInNearlyCorrectResultsCheckPawelskiTail(const std::vector<std::string>& args) {
	std::string allResultsPath = args[0];

	std::cout << "Loading fullResultsList..." << std::endl;
	std::vector<BetaSumPair> fullResultsList = loadFullResultsList(Variables, allResultsPath);
	
	std::cout << "Loading flatNodes..." << std::endl;
	const FlatNode* flatNodes = readFlatBuffer<FlatNode>(FileName::flatNodes(Variables), mbfCounts[Variables]);
	std::cout << "Loading classInfos..." << std::endl;
	const ClassInfo* classInfos = readFlatBuffer<ClassInfo>(FileName::flatClassInfo(Variables), mbfCounts[Variables]);
	std::cout << "Loading allMBFs..." << std::endl;
	const Monotonic<Variables>* allMBFs = readFlatBuffer<Monotonic<Variables>>(FileName::flatMBFs(Variables), mbfCounts[Variables]);
	std::cout << "Running checkTopsTail..." << std::endl;
	NodeIndex fromMBF = std::stoi(args[1]);
	NodeIndex toMBF = mbfCounts[Variables];
	if(args.size() >= 3) {
		toMBF = std::stoi(args[2]);
	}
	checkTopsTailUsingPawelskiIntervalCounting<Variables>(fullResultsList, flatNodes, classInfos, allMBFs, fromMBF, toMBF);

	freeFlatBuffer(flatNodes, mbfCounts[Variables]);
	freeFlatBuffer(classInfos, mbfCounts[Variables]);
	freeFlatBuffer(allMBFs, mbfCounts[Variables]);
}

template<unsigned int Variables>
void correctTops(const std::vector<std::string>& args) {
	std::string allResultsPath = args[0];
	std::string newResultsPath = args[1];
	std::vector<NodeIndex> topsToCorrect;
	for(size_t i = 2; i < args.size(); i++) {
		topsToCorrect.push_back(std::stoi(args[i]));
	}

	std::vector<BetaSumPair> fullResultsList = loadFullResultsList(Variables, allResultsPath);

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

#include <string.h>
static std::vector<NodeIndex> parseBadTopsFromFile(const std::string& fileName) {
	std::cout << "Parsing " << fileName << std::endl;
	char* buf;
	size_t length;

	{
		std::ifstream file(fileName);
		file.seekg(0,std::ios::end);
		length = file.tellg();
		file.seekg(0,std::ios::beg);

		buf = new char[length];
		file.read(buf, length);
	}

	std::vector<NodeIndex> result;
	for(size_t i = 0; i < length - 20; i++) {
		if(buf[i] == 'T' && buf[i] == 'T' && buf[i+1] == 'o' && buf[i+2] == 'p' && buf[i+3] == ' ') {
			const char* digit = buf + i + 4;
			NodeIndex totalDigitSum = 0;
			while(*digit >= '0' && *digit <= '9') {
				totalDigitSum *= 10;
				totalDigitSum += *digit - '0';
				digit++;
			}
			if(strncmp(digit, " is certainly wrong!", sizeof(" is certainly wrong!") - 1) == 0) {
				result.push_back(totalDigitSum);
			}
		}
	}

	return result;
}

template<unsigned int Variables>
void parseAndCheckPossiblyBadTops(const std::vector<std::string>& args) {
	const std::string& allResultsPath = args[0];
	std::vector<NodeIndex> possiblyBadTops = parseBadTopsFromFile(args[1]);

	std::vector<BetaSumPair> fullResultsList = loadFullResultsList(Variables, allResultsPath);

	std::cout << possiblyBadTops.size() << " possibly bad tops found!\n" << std::flush;

	std::cout << "Loading flatNodes..." << std::endl;
	const FlatNode* flatNodes = readFlatBuffer<FlatNode>(FileName::flatNodes(Variables), mbfCounts[Variables]);
	std::cout << "Loading classInfos..." << std::endl;
	const ClassInfo* classInfos = readFlatBuffer<ClassInfo>(FileName::flatClassInfo(Variables), mbfCounts[Variables]);
	std::cout << "Loading allMBFs..." << std::endl;
	const Monotonic<Variables>* allMBFs = readFlatBuffer<Monotonic<Variables>>(FileName::flatMBFs(Variables), mbfCounts[Variables]);

	std::mutex trueErrorMutex;
	std::vector<NodeIndex> trueErrors;
	std::cout << "Rechecking these tops." << std::endl;
	iterRangeInParallelBlocksOnOneCore<size_t>(0, possiblyBadTops.size(), 1, [&](int core, size_t idx){
		NodeIndex claimedTop = possiblyBadTops[idx];

		uint64_t foundIntervalSizeDown = fullResultsList[claimedTop].betaSum.countedIntervalSizeDown;

		uint64_t realIntervalSizeDown = sumSegmentPermutationsOnAllCores<false>(allMBFs[claimedTop], classInfos, allMBFs, 0, flatNodes[claimedTop].dual);
		
		if(realIntervalSizeDown != foundIntervalSizeDown) {
			std::cout << "Found real bad top! " + std::to_string(claimedTop) + ":  Bad intervalSizeDown: (should be: " + std::to_string(realIntervalSizeDown) + ", found: " + std::to_string(foundIntervalSizeDown) + "\n" << std::flush;
			trueErrorMutex.lock();
			trueErrors.push_back(claimedTop);
			trueErrorMutex.unlock();
		} else {
			std::cout << '.' << std::flush;
		}
	});

	std::cout << trueErrors.size() << " true errors found!\n" << std::flush;
	for(NodeIndex badTop : trueErrors) {
		std::cout << "," << badTop;
	}
	std::cout << std::endl;
}

template<unsigned int Variables>
void checkAllDedupTerms(const std::vector<std::string>& args) {
	const std::string& allResultsPath = args[0];

	std::vector<BetaSumPair> fullResultsList = loadFullResultsList(Variables, allResultsPath);

	std::cout << "Loading flatNodes..." << std::endl;
	const FlatNode* flatNodes = readFlatBuffer<FlatNode>(FileName::flatNodes(Variables), mbfCounts[Variables]);
	std::cout << "Loading classInfos..." << std::endl;
	const ClassInfo* classInfos = readFlatBuffer<ClassInfo>(FileName::flatClassInfo(Variables), mbfCounts[Variables]);
	std::cout << "Loading allMBFs..." << std::endl;
	const Monotonic<Variables>* allMBFs = readFlatBuffer<Monotonic<Variables>>(FileName::flatMBFs(Variables), mbfCounts[Variables]);

	std::cout << "Iterating betaSumDualDedups\n" << std::flush;
	iterRangeInParallelBlocksOnAllCores<NodeIndex>(0, mbfCounts[Variables], 8192, [&](int core, NodeIndex topIdx){
		if(topIdx % 1000000 == 0) std::cout << std::to_string(topIdx) + "\n" << std::flush;
		NodeIndex topDual = flatNodes[topIdx].dual;
		ClassInfo dualInfo = classInfos[topDual];
		ProcessedPCoeffSum result = processPCoeffSum(allMBFs[topIdx], allMBFs[topDual]);
		BetaSum trueDualSum = produceBetaTerm(dualInfo, result);
		if(fullResultsList[topIdx].betaSumDualDedup != trueDualSum) {
			std::cout << "Top " + std::to_string(topIdx) + " has incorrect betaSumDualDedup!\n" << std::flush;
		}
	});
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
		std::string allResultsPath = args[1];

		std::vector<BetaSumPair> fullResultsList = loadFullResultsList(Variables, allResultsPath);

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

		std::vector<BetaSumPair> fullResultsList = loadFullResultsList(Variables, allResultsPath);

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


	{"findErrorInNearlyCorrectResultsCheckBasicsAndFirstHalf", findErrorInNearlyCorrectResultsCheckBasicsAndFirstHalf},

	{"findErrorInNearlyCorrectResultsCheckMiddleLayer1", findErrorInNearlyCorrectResultsCheckMiddleLayer<1>},
	{"findErrorInNearlyCorrectResultsCheckMiddleLayer2", findErrorInNearlyCorrectResultsCheckMiddleLayer<2>},
	{"findErrorInNearlyCorrectResultsCheckMiddleLayer3", findErrorInNearlyCorrectResultsCheckMiddleLayer<3>},
	{"findErrorInNearlyCorrectResultsCheckMiddleLayer4", findErrorInNearlyCorrectResultsCheckMiddleLayer<4>},
	{"findErrorInNearlyCorrectResultsCheckMiddleLayer5", findErrorInNearlyCorrectResultsCheckMiddleLayer<5>},
	{"findErrorInNearlyCorrectResultsCheckMiddleLayer6", findErrorInNearlyCorrectResultsCheckMiddleLayer<6>},
	{"findErrorInNearlyCorrectResultsCheckMiddleLayer7", findErrorInNearlyCorrectResultsCheckMiddleLayer<7>},

	{"findErrorInNearlyCorrectResultsCheckTail1", findErrorInNearlyCorrectResultsCheckTail<1>},
	{"findErrorInNearlyCorrectResultsCheckTail2", findErrorInNearlyCorrectResultsCheckTail<2>},
	{"findErrorInNearlyCorrectResultsCheckTail3", findErrorInNearlyCorrectResultsCheckTail<3>},
	{"findErrorInNearlyCorrectResultsCheckTail4", findErrorInNearlyCorrectResultsCheckTail<4>},
	{"findErrorInNearlyCorrectResultsCheckTail5", findErrorInNearlyCorrectResultsCheckTail<5>},
	{"findErrorInNearlyCorrectResultsCheckTail6", findErrorInNearlyCorrectResultsCheckTail<6>},
	{"findErrorInNearlyCorrectResultsCheckTail7", findErrorInNearlyCorrectResultsCheckTail<7>},

	//{"findErrorInNearlyCorrectResultsCheckPawelskiTail1", findErrorInNearlyCorrectResultsCheckPawelskiTail<1>},
	//{"findErrorInNearlyCorrectResultsCheckPawelskiTail2", findErrorInNearlyCorrectResultsCheckPawelskiTail<2>},
	//{"findErrorInNearlyCorrectResultsCheckPawelskiTail3", findErrorInNearlyCorrectResultsCheckPawelskiTail<3>},
	//{"findErrorInNearlyCorrectResultsCheckPawelskiTail4", findErrorInNearlyCorrectResultsCheckPawelskiTail<4>},
	{"findErrorInNearlyCorrectResultsCheckPawelskiTail5", findErrorInNearlyCorrectResultsCheckPawelskiTail<5>},
	{"findErrorInNearlyCorrectResultsCheckPawelskiTail6", findErrorInNearlyCorrectResultsCheckPawelskiTail<6>},
	{"findErrorInNearlyCorrectResultsCheckPawelskiTail7", findErrorInNearlyCorrectResultsCheckPawelskiTail<7>},

	{"parseAndCheckPossiblyBadTops1", parseAndCheckPossiblyBadTops<1>},
	{"parseAndCheckPossiblyBadTops2", parseAndCheckPossiblyBadTops<2>},
	{"parseAndCheckPossiblyBadTops3", parseAndCheckPossiblyBadTops<3>},
	{"parseAndCheckPossiblyBadTops4", parseAndCheckPossiblyBadTops<4>},
	{"parseAndCheckPossiblyBadTops5", parseAndCheckPossiblyBadTops<5>},
	{"parseAndCheckPossiblyBadTops6", parseAndCheckPossiblyBadTops<6>},
	{"parseAndCheckPossiblyBadTops7", parseAndCheckPossiblyBadTops<7>},

	{"checkAllDedupTerms1", checkAllDedupTerms<1>},
	{"checkAllDedupTerms2", checkAllDedupTerms<2>},
	{"checkAllDedupTerms3", checkAllDedupTerms<3>},
	{"checkAllDedupTerms4", checkAllDedupTerms<4>},
	{"checkAllDedupTerms5", checkAllDedupTerms<5>},
	{"checkAllDedupTerms6", checkAllDedupTerms<6>},
	{"checkAllDedupTerms7", checkAllDedupTerms<7>},

	{"computeDedekindFromResultsList", [](const std::vector<std::string>& args){
		unsigned int Variables = std::stoi(args[0]);
		std::string fullResultsPath = args[1];
		std::vector<BetaSumPair> results = loadFullResultsList(Variables, fullResultsPath);

		u192 dedekindNumber = computeDedekindNumberFromBetaSums(Variables, results);
	}},

	{"correctTops1", correctTops<1>},
	{"correctTops2", correctTops<2>},
	{"correctTops3", correctTops<3>},
	{"correctTops4", correctTops<4>},
	{"correctTops5", correctTops<5>},
	{"correctTops6", correctTops<6>},
	{"correctTops7", correctTops<7>},

	{"showResultFileEnds", [](const std::vector<std::string>& args){
		unsigned int Variables = std::stoi(args[0]);
		const std::string& allResultsPath = args[1];
		const BetaSumPair* fullResultsList = readFlatBuffer<BetaSumPair>(allResultsPath, mbfCounts[Variables]);

		auto printPart = [&](NodeIndex idx){
			BetaSumPair bsp = fullResultsList[idx];
			std::cout
				 << idx << ":      "
				 << "\tbs: " << toString(bsp.betaSum.betaSum / factorial(Variables))
				 << "\tsz: " << bsp.betaSum.countedIntervalSizeDown / factorial(Variables)
				 << "\tDbs: " << toString(bsp.betaSumDualDedup.betaSum / factorial(Variables))
				 << "\tDsz: " << bsp.betaSumDualDedup.countedIntervalSizeDown / factorial(Variables)
				 << std::endl;
		};

		printPart(0);
		printPart(1);
		printPart(2);
		std::cout << std::endl;
		printPart(mbfCounts[Variables] - 3);
		printPart(mbfCounts[Variables] - 2);
		printPart(mbfCounts[Variables] - 1);
		printPart(mbfCounts[Variables] - 0);
	}},

	{"compareResultsFiles", [](const std::vector<std::string>& args){
		unsigned int Variables = std::stoi(args[0]);
		std::string pathA = args[1];
		std::string pathB = args[2];

		std::vector<BetaSumPair> resultsA = loadFullResultsList(Variables, pathA);

		std::vector<BetaSumPair> resultsB = loadFullResultsList(Variables, pathB);

		for(size_t i = 0; i < mbfCounts[Variables]; i++) {
			BetaSumPair a = resultsA[i];
			BetaSumPair b = resultsB[i];

			if(a.betaSum.betaSum != b.betaSum.betaSum) {
				std::cout << i << ": betaSum.betaSum: " << toString(a.betaSum.betaSum) << " != " << toString(b.betaSum.betaSum) << std::endl;
			}
			if(a.betaSum.countedIntervalSizeDown != b.betaSum.countedIntervalSizeDown) {
				std::cout << i << ": betaSum.countedIntervalSizeDown: " << toString(a.betaSum.countedIntervalSizeDown) << " != " << toString(b.betaSum.countedIntervalSizeDown) << std::endl;
			}
			if(a.betaSumDualDedup.betaSum != b.betaSumDualDedup.betaSum) {
				std::cout << i << ": betaSumDualDedup.betaSum: " << toString(a.betaSumDualDedup.betaSum) << " != " << toString(b.betaSumDualDedup.betaSum) << std::endl;
			}
			if(a.betaSumDualDedup.countedIntervalSizeDown != b.betaSumDualDedup.countedIntervalSizeDown) {
				std::cout << i << ": betaSumDualDedup.countedIntervalSizeDown: " << toString(a.betaSumDualDedup.countedIntervalSizeDown) << " != " << toString(b.betaSumDualDedup.countedIntervalSizeDown) << std::endl;
			}
		}
	}},

	{"extractFirstRunBetaSums", [](const std::vector<std::string>& args){
		unsigned int Variables = std::stoi(args[0]);
		std::string allResultsPath = args[1];

		const BetaSumPair* allBetaSums = readFlatBuffer<BetaSumPair>(allResultsPath, mbfCounts[Variables]);
		std::unique_ptr<u128[]> resultBuffer(new u128[mbfCounts[Variables]]);
		for(size_t i = 0; i < mbfCounts[Variables]; i++) {
			resultBuffer[i] = allBetaSums[i].betaSum.betaSum;
		}

		writeFlatBuffer<u128>(resultBuffer.get(), FileName::firstRunBetaSums(Variables), mbfCounts[Variables]);
	}},
}};
