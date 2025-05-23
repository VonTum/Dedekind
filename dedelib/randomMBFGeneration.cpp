#ifdef USE_NUMA
#define _GNU_SOURCE
#include <sched.h>
#endif

#include "randomMBFGeneration.h"


#include <iostream>
#include <random>
#include <chrono>

#include <sys/mman.h>
#include <memory>
#include <string.h>

#include <thread>
#include <fstream>

#include "threadPool.h"

#include "generators.h"
#include "fileNames.h"
#include "flatBufferManagement.h"
#include "knownData.h"
#include "pcoeffClasses.h"
#include "numaMem.h"

typedef std::mt19937_64 RandomEngine;

constexpr size_t PREFETCH_CACHE_SIZE = 64;

template<unsigned int Variables>
struct MBFSampler{
	struct CumulativeBuffer{
		uint64_t mbfCountHere;
		uint64_t inverseClassSize; // factorial(Variables) / classSize // Used to get rid of a division
		const Monotonic<Variables>* mbfs;
	};

	std::unique_ptr<const CumulativeBuffer[]> cumulativeBuffers;
	size_t numCumulativeBuffers;
	const Monotonic<Variables>* mbfsByClassSize;

	MBFSampler(const MBFSampler& other) : numCumulativeBuffers(other.numCumulativeBuffers) {
        
        std::cout << "Allocating huge pages..." << std::endl;
		size_t allocSizeBytes = sizeof(Monotonic<Variables>) * mbfCounts[Variables];
		allocSizeBytes = align_to(allocSizeBytes, 1024*1024*1024);
		Monotonic<Variables>* mbfsByClassSize = (Monotonic<Variables>*) aligned_alloc(1024*1024*1024, allocSizeBytes);
		madvise(mbfsByClassSize, allocSizeBytes, MADV_HUGEPAGE);

        std::cout << "Copying data..." << std::endl;
        memcpy(mbfsByClassSize, other.mbfsByClassSize, allocSizeBytes);

        CumulativeBuffer* cumulativeBuffers = new CumulativeBuffer[other.numCumulativeBuffers];
        for(size_t i = 0; i < other.numCumulativeBuffers; i++) {
            cumulativeBuffers[i] = other.cumulativeBuffers[i];
            // Translate mbfs ptrs
            cumulativeBuffers[i].mbfs = mbfsByClassSize + (other.cumulativeBuffers[i].mbfs - other.mbfsByClassSize);
        }

        this->cumulativeBuffers = std::unique_ptr<const CumulativeBuffer[]>(cumulativeBuffers);
        this->mbfsByClassSize = mbfsByClassSize;
    }
	MBFSampler() {
		constexpr int VAR_FACTORIAL = factorial(Variables);
		struct BufferStruct {
			int classSize;
			std::vector<Monotonic<Variables>> mbfs;
		};

		std::cout << "Reading buffers..." << std::endl;
		const Monotonic<Variables>* mbfs = readFlatBufferNoMMAP<Monotonic<Variables>>(FileName::flatMBFs(Variables), mbfCounts[Variables]);
		const ClassInfo* allBigIntervalSizes = readFlatBufferNoMMAP<ClassInfo>(FileName::flatClassInfo(Variables), mbfCounts[Variables]);
		
		std::cout << "Sorting buffers..." << std::endl;
		
		std::vector<BufferStruct> buffers;
		
		// Have sizes sorted in decending order, because most likely classes have full 5040 permutations!
		for(int i = VAR_FACTORIAL / 2; i > 0; i--) {
			if(VAR_FACTORIAL % i == 0) {
				BufferStruct newStruct{i, std::vector<Monotonic<Variables>>{}};
				buffers.push_back(std::move(newStruct));
			}
		}

		// Add all MBFs to these groups
		// Already push the biggest group into the final buffer, saves on a big allocation
		std::cout << "Allocating huge pages..." << std::endl;
		constexpr size_t ALIGN = 1024*1024*1024;
		size_t allocSizeBytes = sizeof(Monotonic<Variables>) * mbfCounts[Variables];
		allocSizeBytes = align_to(allocSizeBytes, ALIGN);
		Monotonic<Variables>* mbfsByClassSize = (Monotonic<Variables>*) aligned_alloc(ALIGN, allocSizeBytes);
		madvise(mbfsByClassSize, allocSizeBytes, MADV_HUGEPAGE);
		//madvise(mbfsByClassSize, allocSizeBytes, MADV_WILLNEED);

		// MMAP to force 1GB pages gives segmentation fault
		//Monotonic<Variables>* mbfsByClassSize = (Monotonic<Variables>*) mmap(nullptr, allocSizeBytes, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_HUGETLB | (30 << MAP_HUGE_SHIFT), -1, 0);
		if(mbfsByClassSize == nullptr) {
			perror("While allocating 1GB pages");
			exit(1);
		}
		Monotonic<Variables>* curMBFsPtr = mbfsByClassSize;
		for(size_t i = 0; i < mbfCounts[Variables]; i++) {
			uint64_t classSize = allBigIntervalSizes[i].classSize;
			if(classSize == VAR_FACTORIAL) {
				*curMBFsPtr++ = mbfs[i];
			} else {
				for(BufferStruct& bf : buffers) {
					if(bf.classSize == classSize) {
						bf.mbfs.push_back(mbfs[i]);
						break;
					}
				}
			}
		}

		std::cout << "Buffer Table:" << std::endl;
		std::cout << "sz = " << factorial(Variables) << " , count = " << (curMBFsPtr - mbfsByClassSize) << std::endl;
		for(BufferStruct& bf : buffers) {
			std::cout << "sz = " << bf.classSize << " , count = " << bf.mbfs.size() << std::endl;
		}

		std::cout << "Regrouping buffers..." << std::endl;
		freeFlatBufferNoMMAP(mbfs, mbfCounts[Variables]); // Free up memory
		freeFlatBufferNoMMAP(allBigIntervalSizes, mbfCounts[Variables]); // Free up memory
		// Now move them all to one large buffer (not really necessary but makes everything a little neater)
		std::unique_ptr<CumulativeBuffer[]> cumulativeBuffers = std::unique_ptr<CumulativeBuffer[]>(new CumulativeBuffer[1 + buffers.size()]);
		cumulativeBuffers[0] = CumulativeBuffer{(curMBFsPtr - mbfsByClassSize) * VAR_FACTORIAL, 1, mbfsByClassSize};
		this->numCumulativeBuffers = 1;
		for(BufferStruct& bf : buffers) {
			if(bf.classSize >= 1) {
				uint64_t mbfCountHere = bf.classSize * bf.mbfs.size();
				cumulativeBuffers[this->numCumulativeBuffers++] = CumulativeBuffer{mbfCountHere, VAR_FACTORIAL / bf.classSize, curMBFsPtr};
				for(Monotonic<Variables>& mbf : bf.mbfs) {
					*curMBFsPtr++ = mbf;
				}
			}
		}

		std::cout << "Finished Regrouping buffers!" << std::endl;
		this->mbfsByClassSize = std::move(mbfsByClassSize);
		this->cumulativeBuffers = std::move(cumulativeBuffers);
	}

	// Does not actually return the Monotonic itself, because it's an expensive Main Memory access, instead the caller prefetches it and returns a result that has already arrived
	const Monotonic<Variables>* sample(RandomEngine& generator) const {
		constexpr int VAR_FACTORIAL = factorial(Variables);
		
		uint64_t chosenMBFIdx = std::uniform_int_distribution<uint64_t>(0, dedekindNumbers[Variables] - 1)(generator);
		
		const CumulativeBuffer* cb = cumulativeBuffers.get();
		while(true) { // Cheap iteration through data in L1 cache
			if(chosenMBFIdx < cb->mbfCountHere) { // will nearly always hit first cumulative buffer, because it's the largest and has the largest factor
				size_t idxInBuffer = chosenMBFIdx * cb->inverseClassSize / VAR_FACTORIAL; // Division by constant is cheap!

				return cb->mbfs + idxInBuffer;
			} else {
				chosenMBFIdx -= cb->mbfCountHere;
			}
			cb++;
		}
	}
};

template<unsigned int Variables>
struct FastRandomPermuter {
	FastRandomPermuter() {};
	BooleanFunction<Variables> permuteRandom(BooleanFunction<7> bf, RandomEngine& generator) const {
		permuteRandom(bf, generator);
		return bf;
	}
};

template<> alignas(64) struct FastRandomPermuter<7> {
	__m128i permute3456[24];

	struct PrecomputedPermuteInfo {
		__m128i shiftAndSwapMaskX; // stores 0, shift1, shift0, mask0
		__m128i shiftAndSwapMaskY; // stores 0, shift2, mask2, mask1
		__m128i shiftLeft0;
		__m128i shiftLeft1;
		__m128i shiftLeft2;
	};

	PrecomputedPermuteInfo bigPermuteInfo[210];

	FastRandomPermuter() {
		// Initialize permute4567 masks
		__m128i identy = _mm_set_epi8(15,14,13,12,11,10,9 ,8 ,7 ,6 ,5 ,4 ,3 ,2 ,1 ,0);
		__m128i swap56 = _mm_set_epi8(15,14,13,12,7 ,6 ,5 ,4 ,11,10,9 ,8 ,3 ,2 ,1 ,0);
		__m128i swap46 = _mm_set_epi8(15,14,7 ,6 ,11,10,3 ,2 ,13,12,5 ,4 ,9 ,8 ,1 ,0);
		__m128i swap36 = _mm_set_epi8(15,7 ,13,5 ,11,3 ,9 ,1 ,14,6 ,12,4 ,10,2 ,8 ,0);

		// All permutations of last three
		__m128i p3456 = identy;
		__m128i p3465 = swap56;
		__m128i p3654 = swap46;
		__m128i p3645 = _mm_shuffle_epi8(p3654, swap56);
		__m128i p3546 = _mm_shuffle_epi8(p3645, swap46);
		__m128i p3564 = _mm_shuffle_epi8(p3546, swap56);

		__m128i permutesLastThree[6]{p3456, p3465, p3654, p3645, p3546, p3564};
		__m128i toPermute[4]{identy, swap36, _mm_shuffle_epi8(p3465, swap36), _mm_shuffle_epi8(p3564, swap36)};

		{
			size_t i = 0;
			for(__m128i to : toPermute) {
				for(__m128i permut : permutesLastThree) {
					this->permute3456[i++] = _mm_shuffle_epi8(to, permut);
				}
			}
		}


		size_t i = 0;
		// Remaining 7, 6, 5 permutations
		for(unsigned int v0 = 0; v0 < 7; v0++) {
			PrecomputedPermuteInfo permutInfo;


			//          v0 = x x _ _ x x _ _
			//           0 = x _ x _ x _ x _
			//  shiftLeft0 = _ _ x _ _ _ x _ = 0 & ~v0
			// shiftRight0 = _ x _ _ _ x _ _ = v0 & ~0 
			// shift = 1, swap = 0

			// 64 bit crossing case
			//          v0 = x x x x _ _ _ _
			//           0 = x _ x _ x _ x _
			// shiftRight0 = _ x _ x _ _ _ _ = 0 & ~v0 // Reverse the variables
			//  shiftLeft0 = _ _ _ _ x _ x _ = v0 & ~0 
			// shift = 1, swap = 1

			uint32_t shift0 = (1 << v0) - (1 << 0);
			uint32_t shouldSwap0 = 0x00000000;
			permutInfo.shiftLeft0 = andnot(BooleanFunction<7>::varMask(0), BooleanFunction<7>::varMask(v0)).data;
			if(v0 == 6) {
				shift0 = (1 << 0);
				shouldSwap0 = 0xFFFFFFFF;
				permutInfo.shiftLeft0 = andnot(BooleanFunction<7>::varMask(6), BooleanFunction<7>::varMask(0)).data;
			}
			for(unsigned int v1 = 1; v1 < 7; v1++) {
				uint32_t shift1 = (1 << v1) - (1 << 1);
				uint32_t shouldSwap1 = 0x00000000;
				permutInfo.shiftLeft1 = andnot(BooleanFunction<7>::varMask(1), BooleanFunction<7>::varMask(v1)).data;
				if(v1 == 6) {
					shift1 = (1 << 1);
					shouldSwap1 = 0xFFFFFFFF;
					permutInfo.shiftLeft1 = andnot(BooleanFunction<7>::varMask(6), BooleanFunction<7>::varMask(1)).data;
				}
				for(unsigned int v2 = 2; v2 < 7; v2++) {
					uint32_t shift2 = (1 << v2) - (1 << 2);
					uint32_t shouldSwap2 = 0x00000000;
					permutInfo.shiftLeft2 = andnot(BooleanFunction<7>::varMask(2), BooleanFunction<7>::varMask(v2)).data;
					if(v2 == 6) {
						shift2 = (1 << 2);
						shouldSwap2 = 0xFFFFFFFF;
						permutInfo.shiftLeft2 = andnot(BooleanFunction<7>::varMask(6), BooleanFunction<7>::varMask(2)).data;
					}
					
					permutInfo.shiftAndSwapMaskX = _mm_set_epi32(0, shift1, shift0, shouldSwap0);
					permutInfo.shiftAndSwapMaskY = _mm_set_epi32(0, shift2, shouldSwap2, shouldSwap1);

					this->bigPermuteInfo[i++] = permutInfo;
				}
			}
		}
	}

	inline __m128i applyPreComputedSwap(__m128i bf, __m128i shiftLeft, __m128i shift, __m128i shouldSwap) const {
		__m128i shiftRight = _mm_shuffle_epi32(_mm_sll_epi64(shiftLeft, shift), _MM_SHUFFLE(1, 0, 3, 2));
		__m128i bitsToShiftLeft = _mm_and_si128(bf, shiftLeft);
		__m128i bitsToShiftRight = _mm_and_si128(bf, shiftRight);
		__m128i bitsToKeep = _mm_andnot_si128(_mm_or_si128(shiftLeft, shiftRight), bf);
		__m128i combinedShiftedBits = _mm_or_si128(_mm_sll_epi64(bitsToShiftLeft, shift), _mm_srl_epi64(bitsToShiftRight, shift));
		__m128i swappedCombinedShiftedBits = _mm_shuffle_epi32(combinedShiftedBits, _MM_SHUFFLE(1, 0, 3, 2));
		__m128i shiftedAndSwapped = _mm_blendv_epi8(combinedShiftedBits, swappedCombinedShiftedBits, shouldSwap);
		return _mm_or_si128(shiftedAndSwapped, bitsToKeep);
	}

	__m128i permuteWithIndex(uint16_t chosenPermutation, __m128i bf) const {
		uint16_t permut3456 = chosenPermutation % 24;
		chosenPermutation /= 24;
		
		const PrecomputedPermuteInfo& bigPermut = this->bigPermuteInfo[chosenPermutation];
		__m128i shiftAndSwapMaskX = _mm_load_si128(&bigPermut.shiftAndSwapMaskX); // stores 0, shift1, shift0, mask0
		__m128i shiftAndSwapMaskY = _mm_load_si128(&bigPermut.shiftAndSwapMaskY); // stores 0, shift2, mask2, mask1

		__m128i shift0 = _mm_shuffle_epi32(shiftAndSwapMaskX, _MM_SHUFFLE(3, 1, 3, 1));
		__m128i shouldSwap0 = _mm_broadcastd_epi32(shiftAndSwapMaskX);
		bf = this->applyPreComputedSwap(bf, _mm_load_si128(&bigPermut.shiftLeft0), shift0, shouldSwap0);

		__m128i shift1 = _mm_shuffle_epi32(shiftAndSwapMaskX, _MM_SHUFFLE(3, 2, 3, 2));
		__m128i shouldSwap1 = _mm_broadcastd_epi32(shiftAndSwapMaskY);
		bf = this->applyPreComputedSwap(bf, _mm_load_si128(&bigPermut.shiftLeft1), shift1, shouldSwap1);

		__m128i shift2 = _mm_shuffle_epi32(shiftAndSwapMaskY, _MM_SHUFFLE(3, 2, 3, 2));
		__m128i shouldSwap2 = _mm_shuffle_epi32(shiftAndSwapMaskY, _MM_SHUFFLE(1, 1, 1, 1));
		bf = this->applyPreComputedSwap(bf, _mm_load_si128(&bigPermut.shiftLeft2), shift2, shouldSwap2);

		bf = _mm_shuffle_epi8(bf, this->permute3456[permut3456]);
		return bf;
	}

	BooleanFunction<7> permuteRandom(BooleanFunction<7> bf, RandomEngine& generator) const {
		size_t selectedIndex = std::uniform_int_distribution<size_t>(0, 5039)(generator);
		bf.bitset.data = this->permuteWithIndex(selectedIndex, bf.bitset.data);
		return bf;
	}
};

template<unsigned int Variables>
void testFastRandomPermuter(BooleanFunction<Variables> sample5040) {
	constexpr unsigned int VAR_FACTORIAL = factorial(Variables);

	BooleanFunction<Variables> sampleCopy = sample5040;

	std::cout << "Testing FastRandomPermuter..." << std::endl;
	FastRandomPermuter<Variables> permuter;

	BooleanFunction<Variables> table[VAR_FACTORIAL];
	{
		size_t i = 0;
		sample5040.forEachPermutation([&](BooleanFunction<Variables> permut){table[i++] = permut;});
	}
	uint64_t counts[VAR_FACTORIAL];
	for(uint64_t& v : counts) {v = 0;}

	RandomEngine generator = properlySeededRNG();

	constexpr uint64_t SAMPLE_COUNT = 1000000;
	for(uint64_t sample_i = 0; sample_i < SAMPLE_COUNT; sample_i++) {
		BooleanFunction<Variables> sampleCopyCopy = sampleCopy;
		BooleanFunction<Variables> permut = permuter.permuteRandom(sampleCopyCopy, generator);
		for(size_t i = 0; i < VAR_FACTORIAL; i++) {
			if(table[i] == permut) {
				counts[i]++;
				goto next;
			}
		}
		std::cout << "Invalid permutation produced!" << std::endl;
		next:;
	}
	std::cout << "Done testing! Exact counts:";
	uint64_t min = SAMPLE_COUNT;
	uint64_t max = 0;
	for(size_t i = 0; i < VAR_FACTORIAL; i++) {
		uint64_t v = counts[i];
		if(v < min) min = v;
		if(v > max) max = v;
		if(i % 24 == 0) std::cout << "\n";
		std::cout << v << ", ";
	}
	std::cout << "\nmin: " << min << ", max: " << max << std::endl;
}

template<unsigned int Variables>
struct RandomMBFGenerationSharedData {
	MBFSampler<Variables> sampler;
	FastRandomPermuter<Variables> permuter;
};

template<unsigned int Variables>
class RandomMBFGenerationThreadLocalState {
	const RandomMBFGenerationSharedData<Variables>* generationData;
	RandomEngine generator;
	size_t curPrefetchLine;
	const Monotonic<Variables>* prefetchCache[PREFETCH_CACHE_SIZE];
public:
	uint64_t numRandomCalls;
	uint64_t numMBFSamples;
	uint64_t numPermutations;

	RandomMBFGenerationThreadLocalState(const RandomMBFGenerationThreadLocalState&) = delete;
	RandomMBFGenerationThreadLocalState(const RandomMBFGenerationThreadLocalState&&) = delete;
	RandomMBFGenerationThreadLocalState& operator=(const RandomMBFGenerationThreadLocalState&) = delete;
	RandomMBFGenerationThreadLocalState& operator=(const RandomMBFGenerationThreadLocalState&&) = delete;

	RandomMBFGenerationThreadLocalState(const RandomMBFGenerationSharedData<Variables>* generationData) : 
		generationData(generationData),
		generator(properlySeededRNG()) {
		

		for(size_t i = 0; i < PREFETCH_CACHE_SIZE; i++) {
			this->prefetchCache[i] = generationData->sampler.mbfsByClassSize;
		}
		this->curPrefetchLine = 0;
		// Get some samples to initialize prefetchCache with good random samples
		for(size_t i = 0; i < PREFETCH_CACHE_SIZE; i++) {
			this->sampleNonPermuted();
		}
		this->numRandomCalls = 0;
		this->numMBFSamples = 0;
		this->numPermutations = 0;
	}

	Monotonic<Variables> sampleNonPermuted() {
		const Monotonic<Variables>* newMBFPtr = this->generationData->sampler.sample(this->generator);
		_mm_prefetch((char const *) newMBFPtr, _MM_HINT_T0);
		Monotonic<Variables> mbfFound = *this->prefetchCache[this->curPrefetchLine]; // Expensive Main Memory access, so we prefetch it and return a result that has already arrived
		this->numRandomCalls++;
		this->numMBFSamples++;
		this->prefetchCache[this->curPrefetchLine] = newMBFPtr;
		this->curPrefetchLine = (this->curPrefetchLine + 1) % PREFETCH_CACHE_SIZE;
		return mbfFound;
	}
	Monotonic<Variables> permuteRandom(Monotonic<Variables> mbf) {
		this->numRandomCalls++;
		this->numPermutations++;
		mbf.bf = this->generationData->permuter.permuteRandom(mbf.bf, this->generator);
		return mbf;
	}
	void coPermuteRandom(Monotonic<Variables>* mbfList, size_t size) {
		if constexpr(Variables == 7) {
			this->numRandomCalls++;
			this->numPermutations += size;
			size_t selectedIndex = std::uniform_int_distribution<size_t>(0, 5039)(this->generator);
			for(size_t i = 0; i < size; i++) {
				Monotonic<Variables>& mbf = mbfList[i];
				mbf.bf.bitset.data = this->generationData->permuter.permuteWithIndex(selectedIndex, mbf.bf.bitset.data);
			}	
		} else {
			throw "NOT IMPLEMENTED";
		}
	}
	Monotonic<Variables> samplePermuted() {
		return this->permuteRandom(this->sampleNonPermuted());
	}
};

template<unsigned int Variables>
void benchmarkRandomMBFGeneration() {
	const RandomMBFGenerationSharedData<Variables> generationData;

	//testFastRandomPermuter(sampler.mbfsByClassSize[0].bf); // Will have 5040 variations

	std::cout << "Random Generation!" << std::endl;
	RandomMBFGenerationThreadLocalState<Variables> threadState{&generationData};

	uint64_t dont_optimize_summer = 0;
	constexpr uint64_t SAMPLE_SIZE = 1000000000;

	auto start = std::chrono::high_resolution_clock::now();
	for(uint64_t i = 0; i < SAMPLE_SIZE; i++) {
		Monotonic<Variables> mbfFound = threadState.samplePermuted();

		if constexpr(Variables == 7) {
			dont_optimize_summer += _mm_extract_epi64(mbfFound.bf.bitset.data, 0) + _mm_extract_epi64(mbfFound.bf.bitset.data, 1);
		} else {
			dont_optimize_summer += mbfFound.bf.bitset.data;
		}
		//if(i % (SAMPLE_SIZE / 100) == 0) std::cout << i << std::endl;
	}
	auto time = std::chrono::high_resolution_clock::now() - start;
	uint64_t millis = std::chrono::duration_cast<std::chrono::milliseconds>(time).count();
	std::cout << "Took " << millis << "ms: " << (SAMPLE_SIZE / (millis / 1000.0)) << " random MBF7 per second!" << std::endl;

	std::cout << "Don't optimize print: " << dont_optimize_summer << std::endl;
}

template void benchmarkRandomMBFGeneration<1>();
template void benchmarkRandomMBFGeneration<2>();
template void benchmarkRandomMBFGeneration<3>();
template void benchmarkRandomMBFGeneration<4>();
template void benchmarkRandomMBFGeneration<5>();
template void benchmarkRandomMBFGeneration<6>();
template void benchmarkRandomMBFGeneration<7>();



std::array<Monotonic<7>, 2> mbfUp8(RandomMBFGenerationThreadLocalState<7>& gen) {
	std::array<Monotonic<7>, 2> arr{gen.sampleNonPermuted(), gen.samplePermuted()};
	do {
		arr[0] = gen.sampleNonPermuted();
		arr[1] = gen.samplePermuted();
	} while(!(arr[1] <= arr[0]));
    gen.coPermuteRandom(&arr[0], 2);
	return arr;
}

std::array<Monotonic<7>, 4> mbfUp9(RandomMBFGenerationThreadLocalState<7>& gen) {
	std::array<Monotonic<7>, 4> arr;
	do {
		do {
			arr[0] = gen.sampleNonPermuted();
			arr[1] = gen.samplePermuted();
		} while(!(arr[1] <= arr[0]));

		do {
			arr[2] = gen.sampleNonPermuted();
			arr[3] = gen.samplePermuted();
		} while(!(arr[3] <= arr[2]));
		gen.coPermuteRandom(&arr[2], 2);
	} while(!(arr[2] <= arr[0] && arr[3] <= arr[1]));
    gen.coPermuteRandom(&arr[0], 4);
	return arr;
}

void benchmarkMBF9Generation() {
	const RandomMBFGenerationSharedData<7> generationData;

	RandomMBFGenerationThreadLocalState<7> threadState{&generationData};

	__m128i dont_optimize_summer = _mm_setzero_si128();
	constexpr uint64_t SAMPLE_SIZE = 10000;

	std::cout << "Random Generation..." << std::endl;

	auto start = std::chrono::high_resolution_clock::now();
	for(uint64_t i = 0; i < SAMPLE_SIZE; i++) {
		std::array<Monotonic<7>, 4> mbf9 = mbfUp9(threadState);
		for(int i = 3; i >= 0; i--) {
			Monotonic<7>& mbf7 = mbf9[i];
			//std::cout << mbf7;
			dont_optimize_summer = _mm_add_epi64(dont_optimize_summer, mbf7.bf.bitset.data);
		}
		std::cout << '.' << std::flush;
		//std::cout << std::endl;
	}
	uint64_t dont_optimize_summer_64 = _mm_extract_epi64(dont_optimize_summer, 0) + _mm_extract_epi64(dont_optimize_summer, 1);

	auto time = std::chrono::high_resolution_clock::now() - start;
	uint64_t millis = std::chrono::duration_cast<std::chrono::milliseconds>(time).count();
	std::cout << "\nTook " << millis << "ms: " << (SAMPLE_SIZE / (millis / 1000.0)) << " random MBF9 per second!" << std::endl;

	std::cout << "numRandomCalls: " << threadState.numRandomCalls << std::endl;
	std::cout << "numMBFSamples: " << threadState.numMBFSamples << std::endl;
	std::cout << "numPermutations: " << threadState.numPermutations << std::endl;

	std::cout << "Don't optimize print: " << dont_optimize_summer_64 << std::endl;
}

template<typename T>
struct CombinedData {
	void(*func)(int nodeID, T* data);
	int numaNode;
	T* data;
};
template<typename T>
void* numaNodePthreadFunc(void* voidData) {
	CombinedData<T>* data = (CombinedData<T>*) voidData;
	(*data->func)(data->numaNode, data->data);
	return NULL;
}

#ifdef USE_NUMA
#include <sched.h>
#include <numa.h>
int getNumNumaNodes() {
	if(numa_available() == 0) {
		return numa_num_configured_nodes();
	} else {
		return 1;
	}
}
std::vector<cpu_set_t> getNUMA_CPUSets() {
	int numNumaNodes = getNumNumaNodes();
	std::vector<cpu_set_t> result;
	result.reserve(numNumaNodes);

	struct bitmask* numa_bits = numa_allocate_cpumask();

	for(int numaNode = 0; numaNode < numNumaNodes; numaNode++) {
		if(numa_node_to_cpus(numaNode, numa_bits) < 0) {
			perror("numa_node_to_cpus");
			exit(1);
		}

		cpu_set_t cpuset;
		CPU_ZERO(&cpuset);
		for (unsigned int cpu = 0; cpu < std::thread::hardware_concurrency(); ++cpu) {
            if(numa_bitmask_isbitset(numa_bits, cpu)) {
                CPU_SET(cpu, &cpuset);
            }
        }
		result.push_back(cpuset);
	}

	numa_bitmask_free(numa_bits);
	
	return result;
}
#else
int getNumNumaNodes() {
	return 1;
}
std::vector<cpu_set_t> getNUMA_CPUSets() {
	cpu_set_t allCPUsSet;
	CPU_ZERO(&allCPUsSet);
	for(int i = 0; i < std::thread::hardware_concurrency(); i++) {
		CPU_SET(i, &allCPUsSet);
	}
	return std::vector<cpu_set_t>{allCPUsSet};
}
int numa_node_of_cpu(int cpu) {
	return 0;
}
#endif
template<typename T>
void runOneThreadPerNUMANode(std::vector<cpu_set_t>& cpusPerNUMANode, void(*func)(int, T*), T* data) {
	int numNumaNodes = getNumNumaNodes();
	pthread_t* threads = new pthread_t[numNumaNodes];
	CombinedData<T>* dataDatas = new CombinedData<T>[numNumaNodes];

	for(int numaNode = 0; numaNode < numNumaNodes; numaNode++) {
		dataDatas[numaNode].func = func;
		dataDatas[numaNode].data = data;
		dataDatas[numaNode].numaNode = numaNode;

		threads[numaNode] = createPThreadAffinity(cpusPerNUMANode[numaNode], numaNodePthreadFunc<T>, &dataDatas[numaNode]);
	}
	std::cout << "Join all threads..." << std::endl;

	PThreadBundle bundle(threads, numNumaNodes);
	bundle.join();
	std::cout << "Threads joined!" << std::endl;
}

template<unsigned int Variables>
void parallelizeMBFGenerationAcrossAllCores(size_t numToGenerate) {
	std::vector<cpu_set_t> cpusPerNUMANode = getNUMA_CPUSets();

	#define INITIAL_NUMA_NODE 0
	std::cout << "Initializing and reading file..." << std::endl;
	RandomMBFGenerationSharedData<7>** generationDatas = new RandomMBFGenerationSharedData<7>*[getNumNumaNodes()];
	pthread_t initial_create_thread = createPThreadAffinity(cpusPerNUMANode[INITIAL_NUMA_NODE], [](void* voidData) -> void* {
		RandomMBFGenerationSharedData<7>** initialData = (RandomMBFGenerationSharedData<7>**) voidData;
		*initialData = new RandomMBFGenerationSharedData<7>();
		return NULL;
	}, (void*) &generationDatas[0]);
	void* nullTarget;
	pthread_join(initial_create_thread, &nullTarget);

	std::cout << "Copying data across " << getNumNumaNodes() << " NUMA Nodes..." << std::endl;
	runOneThreadPerNUMANode<RandomMBFGenerationSharedData<7>*>(cpusPerNUMANode, [](int nodeID, RandomMBFGenerationSharedData<7>** generationDatas){
		if(nodeID == INITIAL_NUMA_NODE) return;
		generationDatas[nodeID] = new RandomMBFGenerationSharedData<7>(*generationDatas[INITIAL_NUMA_NODE]);
	}, generationDatas);

	std::cout << "All data copied!" << std::endl;

	constexpr size_t NUM_MBF7_BLOCKS = 1 << (Variables - 7);

    size_t bufferSize = sizeof(std::array<Monotonic<7>, NUM_MBF7_BLOCKS>) * numToGenerate;
	size_t alignedBufferSize = align_to(bufferSize, 4096);
    std::array<Monotonic<7>, NUM_MBF7_BLOCKS>* resultBuf = (std::array<Monotonic<7>, NUM_MBF7_BLOCKS>*) aligned_alloc(4096, alignedBufferSize);
    madvise(resultBuf, alignedBufferSize, MADV_NOHUGEPAGE); // Preserve huge pages for buffers that need it
    
	std::cout << "Random Generation..." << std::endl;
	auto start = std::chrono::high_resolution_clock::now();
    iterRangeInParallelBlocksOnAllCores<size_t, size_t, RandomMBFGenerationThreadLocalState<7>>(0, numToGenerate, 64, [&](RandomMBFGenerationThreadLocalState<7>& threadState, size_t elem){
		if constexpr(Variables == 7) {
			resultBuf[elem][0] = threadState.samplePermuted();
		} else if constexpr(Variables == 8) {
        	resultBuf[elem] = mbfUp8(threadState);
		} else if constexpr(Variables == 9) {
        	resultBuf[elem] = mbfUp9(threadState);
		} else {
			throw "Unimplemented!";
		}
    }, [&](int threadID) -> RandomMBFGenerationThreadLocalState<7> {
        return RandomMBFGenerationThreadLocalState<7>(generationDatas[numa_node_of_cpu(threadID)]);
    });

	auto time = std::chrono::high_resolution_clock::now() - start;
	double seconds = std::chrono::duration_cast<std::chrono::milliseconds>(time).count() / 1000.0;
	std::cout << "Took " << seconds << "s: " << (numToGenerate / seconds) << " random MBF8 per second using " << std::thread::hardware_concurrency() << " threads." << std::endl;

	// Always append to the file, so we keep gathering more and more results
	std::ofstream outFile(FileName::randomMBFs(Variables), std::ios::binary | std::ios::app);
    outFile.write((const char*) resultBuf, bufferSize);
}

template void parallelizeMBFGenerationAcrossAllCores<7>(size_t numToGenerate);
template void parallelizeMBFGenerationAcrossAllCores<8>(size_t numToGenerate);
template void parallelizeMBFGenerationAcrossAllCores<9>(size_t numToGenerate);

void generateMBFsFromPreviousBuffer(unsigned int Variables) {
	size_t fileSize;
	const uint64_t* mbfs = static_cast<const uint64_t*>(mmapWholeFileSequentialRead(FileName::randomMBFs(Variables), fileSize));
	std::vector<uint64_t> outMBFs;

	size_t bitsPerMBF = 1 << Variables;
	size_t blocksPerMBF = bitsPerMBF / (sizeof(uint64_t) * 8);

	size_t numMBFs = (fileSize * 8) / bitsPerMBF;

	/*std::cout << "Checking input MBFs" << std::endl;
	for(size_t i = 0; i < numMBFs; i++) {
		const uint64_t* curMBFPair = &mbfs[i * blocksPerMBF];

		if(Variables == 8) {
			if(!reinterpret_cast<const BooleanFunction<8>*>(curMBFPair)->isMonotonic()) {
				throw "INVALID MBF";
			}
		}
		if(Variables == 9) {
			if(!reinterpret_cast<const BooleanFunction<9>*>(curMBFPair)->isMonotonic()) {
				throw "INVALID MBF";
			}
		}
	}*/

	std::cout << "Generating" << std::endl;
	for(size_t i = 0; i < numMBFs / 2; i++) {
		bool isValidPair = true;

		const uint64_t* curMBFPair = &mbfs[i * (blocksPerMBF * 2)];
		for(size_t b = 0; b < blocksPerMBF; b++) {
			if((curMBFPair[blocksPerMBF + b] & ~curMBFPair[b]) != 0) {
				isValidPair = false;
				break;
			}
		}

		if(isValidPair) {
			std::cout << '.' << std::flush;
			if(Variables == 8) {
				if(!reinterpret_cast<const BooleanFunction<9>*>(curMBFPair)->isMonotonic()) {
					throw "INVALID MBF";
				}
			}
			if(Variables == 9) {
				if(!reinterpret_cast<const BooleanFunction<10>*>(curMBFPair)->isMonotonic()) {
					throw "INVALID MBF";
				}
			}
			for(size_t b = 0; b < blocksPerMBF * 2; b++) {
				outMBFs.push_back(curMBFPair[b]);
			}
		}
	}

	std::cout << "Total generated MBFs: " << outMBFs.size() / (blocksPerMBF * 2) << std::endl;

	std::ofstream outFile(FileName::randomMBFs(Variables + 1), std::ios::binary);
    outFile.write((const char*) outMBFs.data(), outMBFs.size() * sizeof(uint64_t) * 8);
}

// ==== Random Walks ====

template<unsigned int Variables, typename RandomEngine>
__attribute__ ((noinline)) size_t stepRandom(Monotonic<Variables>& mbf, RandomEngine& rng) {
	Monotonic<Variables> succ = mbf.succ();
	Monotonic<Variables> pred = mbf.pred();
	
	BooleanFunction<Variables> possibleBits = andnot(succ.bf, mbf.bf) | andnot(mbf.bf, pred.bf);

	size_t numBits = possibleBits.size();

	size_t selectedToFlip = std::uniform_int_distribution<size_t>(0, numBits - 1)(rng);

	mbf.bf.bitset.toggle(possibleBits.getNth(selectedToFlip));

	assert(mbf.bf.isMonotonic());

	return numBits;
}

constexpr size_t NUM_STEPS = 15000;
constexpr size_t NUM_TRIALS = 100000;

template<unsigned int Variables, typename RandomEngine>
Monotonic<Variables> generateMBFByWalks(RandomEngine& rng) {
	Monotonic<Variables> curMbf = Monotonic<Variables>::getBot();
	
	for(size_t step = 0; step < NUM_STEPS; step++) {
		stepRandom(curMbf, rng);
	}

	return curMbf;
}

template<unsigned int Variables>
void estimateDedekRandomWalks() {
	RandomEngine rng = properlySeededRNG();

	std::cout << "Random Generation of MBF" << Variables << std::endl;

	const BooleanFunction<Variables> lowerHalf = Monotonic<Variables>::getFilledUpToIncludingLayer(Variables / 2 - 1).bf;
	const BooleanFunction<Variables> upperHalf = ~Monotonic<Variables>::getFilledUpToIncludingLayer(Variables / 2).bf;

	uint64_t numWellKnownMBFs = 0;

	auto start = std::chrono::high_resolution_clock::now();
	for(size_t trial = 0; trial < NUM_TRIALS; trial++) {
		Monotonic<Variables> genMbf = generateMBFByWalks<Variables>(rng);

		bool isWellKnown = ((upperHalf & genMbf.bf) | andnot(lowerHalf, genMbf.bf)).isEmpty();

		if(isWellKnown) {
			numWellKnownMBFs++;
		}
	}
	auto time = std::chrono::high_resolution_clock::now() - start;
	double seconds = std::chrono::duration_cast<std::chrono::milliseconds>(time).count() / 1000.0;

	double loopsPerSecond = NUM_TRIALS * NUM_STEPS / seconds;

	std::cout << "Took " << seconds << "s: " << (NUM_TRIALS / seconds) << " random MBF" << Variables << " per second." << std::endl;

	std::cout << "This means " << loopsPerSecond << " loops/s" << std::endl;

	double estimatedDedekindNumber = pow(2.0, double(choose(Variables, Variables / 2))) * NUM_TRIALS / numWellKnownMBFs;

	std::cout << "Fraction of well known: " << numWellKnownMBFs << " / " << NUM_TRIALS << " = " << double(numWellKnownMBFs) / NUM_TRIALS;

	std::cout << "Estimated D(" << Variables << ") = " << estimatedDedekindNumber << std::endl;
}

template void estimateDedekRandomWalks<1>();
template void estimateDedekRandomWalks<2>();
template void estimateDedekRandomWalks<3>();
template void estimateDedekRandomWalks<4>();
template void estimateDedekRandomWalks<5>();
template void estimateDedekRandomWalks<6>();
template void estimateDedekRandomWalks<7>();
template void estimateDedekRandomWalks<8>();
template void estimateDedekRandomWalks<9>();
template void estimateDedekRandomWalks<10>();
template void estimateDedekRandomWalks<11>();
template void estimateDedekRandomWalks<12>();
template void estimateDedekRandomWalks<13>();
template void estimateDedekRandomWalks<14>();
template void estimateDedekRandomWalks<15>();
