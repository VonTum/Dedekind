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
		allocSizeBytes = (allocSizeBytes + ALIGN - 1) & ~(ALIGN - 1);
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

		std::cout << "Regrouping buffers..." << std::endl;
		freeFlatBufferNoMMAP(mbfs, mbfCounts[Variables]); // Free up memory
		freeFlatBufferNoMMAP(allBigIntervalSizes, mbfCounts[Variables]); // Free up memory
		// Now move them all to one large buffer (not really necessary but makes everything a little neater)
		this->numCumulativeBuffers = 1 + buffers.size();
		std::unique_ptr<CumulativeBuffer[]> cumulativeBuffers = std::unique_ptr<CumulativeBuffer[]>(new CumulativeBuffer[this->numCumulativeBuffers]);
		cumulativeBuffers[0] = CumulativeBuffer{(curMBFsPtr - mbfsByClassSize) * VAR_FACTORIAL, 1, mbfsByClassSize};
		for(size_t i = 0; i < buffers.size(); i++) {
			BufferStruct& bf = buffers[i];
			uint64_t mbfCountHere = bf.classSize * bf.mbfs.size();
			cumulativeBuffers[i+1] = CumulativeBuffer{mbfCountHere, VAR_FACTORIAL / bf.classSize, curMBFsPtr};
			for(Monotonic<Variables>& mbf : bf.mbfs) {
				*curMBFsPtr++ = mbf;
			}
		}

		this->mbfsByClassSize = std::move(mbfsByClassSize);
		this->cumulativeBuffers = std::move(cumulativeBuffers);
	}

	// Does not actually return the Monotonic itself, because it's an expensive Main Memory access, instead the caller prefetches it and returns a result that has already arrived
	const Monotonic<Variables>* sample(std::default_random_engine& generator) const {
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
	BooleanFunction<Variables> permuteRandom(BooleanFunction<7> bf, std::default_random_engine& generator) const {
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

	BooleanFunction<7> permuteRandom(BooleanFunction<7> bf, std::default_random_engine& generator) const {
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

	std::default_random_engine generator;
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
	std::default_random_engine generator;
	size_t curPrefetchLine;
	const Monotonic<Variables>* prefetchCache[PREFETCH_CACHE_SIZE];
public:
	uint64_t numRandomCalls;
	uint64_t numMBFSamples;
	uint64_t numPermutations;

	RandomMBFGenerationThreadLocalState(const RandomMBFGenerationSharedData<Variables>* generationData) : generationData(generationData) {
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
	while(true) {
		std::array<Monotonic<7>, 2> arr{gen.sampleNonPermuted(), gen.samplePermuted()};
		if(arr[0] <= arr[1]) {
			return arr;
		}
	}
}

std::array<Monotonic<7>, 4> mbfUp9(RandomMBFGenerationThreadLocalState<7>& gen) {
	std::array<Monotonic<7>, 4> arr;
	do {
		do {
			arr[0] = gen.sampleNonPermuted();
			arr[1] = gen.samplePermuted();
		} while(!(arr[0] <= arr[1]));

		do {
			arr[2] = gen.sampleNonPermuted();
			arr[3] = gen.samplePermuted();
		} while(!(arr[2] <= arr[3]));
		gen.coPermuteRandom(&arr[2], 2);
	} while(!(arr[0] <= arr[2] && arr[1] <= arr[3]));
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

void parallelizeMBF9GenerationAcrossAllCores(size_t numToGenerate) {
    RandomMBFGenerationSharedData<7>* generationDatas[8];
    int threadsPerNUMANode;

    setNUMANodeAffinity(7);
    generationDatas[0] = new RandomMBFGenerationSharedData<7>(); // Alloc memory in NUMA node 0

    if(std::thread::hardware_concurrency() >= 128) {
        threadsPerNUMANode = CORES_PER_NUMA_NODE;
        struct ThreadData {
            RandomMBFGenerationSharedData<7>* from;
            RandomMBFGenerationSharedData<7>** to;
        } datas[7];
        for(int i = 0; i < 7; i++) {
            datas[i].from = generationDatas[0];
            datas[i].to = &generationDatas[i+1];
        }
        PThreadBundle copyThreads = spreadThreads(7, CPUAffinityType::NUMA_DOMAIN, datas, [](void* data) -> void* {
            ThreadData* d = (ThreadData*) data;
            *(d->to) = new RandomMBFGenerationSharedData<7>(*(d->from));
            return NULL;
        });
        copyThreads.join();
        std::cout << "All data copied!" << std::endl;
    } else {
        threadsPerNUMANode = std::thread::hardware_concurrency();
    }

    size_t bufferSize = sizeof(std::array<Monotonic<7>, 4>) * numToGenerate;
    std::array<Monotonic<7>, 4>* resultBuf = (std::array<Monotonic<7>, 4>*) aligned_alloc(1024*4, bufferSize);
    madvise(resultBuf, bufferSize, MADV_NOHUGEPAGE); // Preserve huge pages for buffers that need it
    
	std::cout << "Random Generation..." << std::endl;
	auto start = std::chrono::high_resolution_clock::now();
    iterRangeInParallelBlocksOnAllCores<size_t, size_t, RandomMBFGenerationThreadLocalState<7>>(0, numToGenerate, 64, [&](RandomMBFGenerationThreadLocalState<7>& threadState, size_t elem){
        resultBuf[elem] = mbfUp9(threadState);
    }, [&](int threadID) -> RandomMBFGenerationThreadLocalState<7> {
        return RandomMBFGenerationThreadLocalState<7>(generationDatas[threadID / threadsPerNUMANode]);
    });

	auto time = std::chrono::high_resolution_clock::now() - start;
	double seconds = std::chrono::duration_cast<std::chrono::milliseconds>(time).count() / 1000.0;
	std::cout << "Took " << seconds << "s: " << (numToGenerate / seconds) << " random MBF9 per second usign " << std::thread::hardware_concurrency() << " threads." << std::endl;

	std::ofstream outFile(FileName::dataPath + "randomMBF9.mbf9");
    outFile.write((const char*) resultBuf, bufferSize);
}
