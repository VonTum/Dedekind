#pragma once

#include <memory>
#include "booleanFunction.h"
#include "funcTypes.h"


template<typename T>
size_t idxOf(const T& val, const T* arr) {
	for(size_t i = 0; ; i++) {
		if(arr[i] == val) return i;
	}
}

template<typename T>
size_t idxOf(const T& val, const T* arr, size_t arrSize) {
	for(size_t i = 0; i < arrSize; i++) {
		if(arr[i] == val) return i;
	}
	std::cerr << "idxOf: Element not found in array" << std::endl;
	std::abort();
}

template<unsigned int SmallVariables, unsigned int BigVariables>
BooleanFunction<SmallVariables> getPart(const BooleanFunction<BigVariables>& bf, size_t idx) {
	static_assert(BigVariables >= SmallVariables);
	assert(idx < (1 << (BigVariables - SmallVariables)));

	BooleanFunction<SmallVariables> result;
	size_t numBitsOffset = (1 << SmallVariables) * idx;
	if constexpr(BigVariables == 7) {
		if(numBitsOffset < 64) {
			uint64_t chunk64 = _mm_extract_epi64(bf.bitset.data, 0);
			result.bitset.data = static_cast<decltype(result.bitset.data)>(chunk64 >> numBitsOffset);
		} else {
			uint64_t chunk64 = _mm_extract_epi64(bf.bitset.data, 1);
			result.bitset.data = static_cast<decltype(result.bitset.data)>(chunk64 >> (numBitsOffset - 64));
		}
	} else if constexpr(BigVariables <= 6) {
		result.bitset.data = static_cast<decltype(result.bitset.data)>(bf.bitset.data >> numBitsOffset);
	} else {
		std::cerr << "getPart > 7 Not implemeted.";
		std::abort();
	}
	return result;
}
template<unsigned int SmallVariables, unsigned int BigVariables>
Monotonic<SmallVariables> getPart(const Monotonic<BigVariables>& bf, size_t idx) {
	Monotonic<SmallVariables> result;
	result.bf = getPart<SmallVariables, BigVariables>(bf.bf, idx);
	return result;
}
template<unsigned int SmallVariables, unsigned int BigVariables>
AntiChain<SmallVariables> getPart(const AntiChain<BigVariables>& bf, size_t idx) {
	AntiChain<SmallVariables> result;
	result.bf = getPart<SmallVariables, BigVariables>(bf.bf, idx);
	return result;
}


template<unsigned int Variables>
std::unique_ptr<Monotonic<Variables>[]> generateAllMBFs() {
	std::unique_ptr<Monotonic<Variables>[]> result(new Monotonic<Variables>[dedekindNumbers[Variables]]);

	size_t i = 0;

	forEachMonotonicFunction<Variables>([&i, &result](const Monotonic<Variables>& v){
		result[i++] = v;
	});

	std::sort(result.get(), result.get() + dedekindNumbers[Variables], [](const Monotonic<Variables>& a, const Monotonic<Variables>& b){
		return a.size() < b.size();
	});

	return result;
}

template<unsigned int Variables>
std::unique_ptr<uint16_t[]> generateInverseIndexMap(const Monotonic<Variables>* mbfs) {
	static_assert(Variables <= 5); // Otherwise LUT becomes astronomically large
	std::unique_ptr<uint16_t[]> inverseIndexMap(new uint16_t[size_t(1) << (1 << Variables)]);
	for(size_t i = 0; i < dedekindNumbers[Variables]; i++) {
		inverseIndexMap[mbfs[i].bf.bitset.data] = i;
	}
	return inverseIndexMap;
}

template<unsigned int Variables>
std::unique_ptr<uint16_t[]> generateAboveLUT(const Monotonic<Variables>* mbfs) {
	constexpr size_t dedekNum = dedekindNumbers[Variables];
	std::unique_ptr<uint16_t[]> aboveLUT(new uint16_t[(dedekNum+1) * dedekNum]);

	for(size_t i = 0; i < dedekNum; i++) {
		uint16_t* thisRow = aboveLUT.get() + (dedekNum+1) * i;
		uint16_t count = 0;

		Monotonic<Variables> mbfI = mbfs[i];

		for(size_t j = 0; j < dedekNum; j++) {
			Monotonic<Variables> mbfJ = mbfs[j];
			if(mbfI <= mbfJ) {
				thisRow[++count] = j;
			}
		}
		thisRow[0] = count;
	}

	return aboveLUT;
}

template<unsigned int Variables>
std::unique_ptr<uint16_t[]> generateIntervalSizeMatrix(const Monotonic<Variables>* mbfLUT, const uint16_t* aboveLUT) {
	constexpr size_t dedekNum = dedekindNumbers[Variables];
	std::unique_ptr<uint16_t[]> intervalSizesLUT(new uint16_t[dedekNum * dedekNum]);
	memset(intervalSizesLUT.get(), 0xFF, sizeof(uint16_t) * dedekNum * dedekNum);

	for(size_t bot = 0; bot < dedekNum; bot++) {
		const uint16_t* thisRowAboveLUT = aboveLUT + (dedekNum+1) * bot;
		uint16_t aboveCount = thisRowAboveLUT[0];
		for(size_t i = 0; i < aboveCount; i++) {
			uint16_t top = thisRowAboveLUT[i+1];
			intervalSizesLUT[bot*dedekNum + top] = (uint16_t) intervalSizeFast(mbfLUT[bot], mbfLUT[top]);
		}
	}
	
	return intervalSizesLUT;
}

template<unsigned int Variables>
uint64_t pawelskiIntervalToTopSize(const Monotonic<Variables>& bot, const Monotonic<Variables-2>* mbfLUT, const uint16_t* inverseIndexMap, const uint16_t* aboveLUT, const uint16_t* smallerIntervalSizeMatrix) {
	Monotonic<Variables-2> f5 = getPart<Variables-2>(bot, 0);
	Monotonic<Variables-2> f3 = getPart<Variables-2>(bot, 1);
	Monotonic<Variables-2> f1 = getPart<Variables-2>(bot, 2);
	Monotonic<Variables-2> f0 = getPart<Variables-2>(bot, 3);

	assert(f0 <= f1);
	assert(f0 <= f3);
	assert(f0 <= f5);
	assert(f1 <= f5);
	assert(f3 <= f5);

	uint64_t totalIntervalSize = 0;

	constexpr size_t smallMBFCount = dedekindNumbers[Variables-2];
	for(size_t f2Idx = 0; f2Idx < smallMBFCount; f2Idx++) {
		Monotonic<Variables-2> f2 = mbfLUT[f2Idx];
		if(!(f0 <= f2)) continue;

		Monotonic<Variables-2> f4Min = f1 | f2;
		Monotonic<Variables-2> f6Min = f3 | f2;

		Monotonic<Variables-2> f7Min = f5 | f2;
		size_t f7MinIdx = inverseIndexMap[f7Min.bf.bitset.data];//idxOf(f7Min, mbfLUT, smallMBFCount);

		const uint16_t* f4MinIntervalLUT = smallerIntervalSizeMatrix + smallMBFCount * inverseIndexMap[f4Min.bf.bitset.data];//idxOf(f4Min, mbfLUT, smallMBFCount);
		const uint16_t* f6MinIntervalLUT = smallerIntervalSizeMatrix + smallMBFCount * inverseIndexMap[f6Min.bf.bitset.data];//idxOf(f6Min, mbfLUT, smallMBFCount);

		const uint16_t* thisRowAboveLUT = aboveLUT + (smallMBFCount+1) * f7MinIdx;
		uint16_t aboveCount = thisRowAboveLUT[0];

		for(uint16_t i = 0; i < aboveCount; i++) {
			uint16_t f7Idx = thisRowAboveLUT[1+i];

			uint64_t intervalSizeF4 = f4MinIntervalLUT[f7Idx];
			uint64_t intervalSizeF6 = f6MinIntervalLUT[f7Idx];

			assert(intervalSizeF4 != 0xFFFF);
			assert(intervalSizeF6 != 0xFFFF);

			totalIntervalSize += intervalSizeF4 * intervalSizeF6;
		}
	}

	return totalIntervalSize;
}



template<unsigned int Variables>
std::unique_ptr<uint16_t[]> generateSliceSizeIntervalSizeMatrix(const Monotonic<Variables>* mbfLUT, const uint16_t* aboveLUT) {
	constexpr size_t dedekNum = dedekindNumbers[Variables];
	constexpr size_t numLayers = (1 << Variables) + 1;
	std::unique_ptr<uint16_t[]> intervalSizesMultiLUT(new uint16_t[dedekNum * dedekNum * numLayers]);
	memset(intervalSizesMultiLUT.get(), 0, sizeof(uint16_t) * dedekNum * dedekNum * numLayers);

	for(size_t bot = 0; bot < dedekNum; bot++) {
		const uint16_t* thisRowAboveLUT = aboveLUT + (dedekNum+1) * bot;
		uint16_t aboveCount = thisRowAboveLUT[0];
		for(size_t i = 0; i < aboveCount; i++) {
			uint16_t top = thisRowAboveLUT[i+1];
			Monotonic<Variables> topMBF = mbfLUT[top];

			for(size_t inbetweenI = 0; inbetweenI < aboveCount; inbetweenI++) {
				uint16_t inbetweenMBFI = thisRowAboveLUT[1+inbetweenI];
				if(inbetweenMBFI > top) break;
				Monotonic<Variables> inbetweenMBF = mbfLUT[inbetweenMBFI];

				if(inbetweenMBF <= topMBF) {
					intervalSizesMultiLUT[inbetweenMBF.size() * dedekNum * dedekNum + bot*dedekNum + top]++;
				}
			}
		}
	}
	
	return intervalSizesMultiLUT;
}


template<unsigned int Variables>
uint64_t pawelskiIntervalToTopSizeAboveSize(const Monotonic<Variables>& bot, int minimumSize, const Monotonic<Variables-2>* mbfLUT, const uint16_t* inverseIndexMap, const uint16_t* aboveLUT, const uint16_t* smallerIntervalSizeMatrix3D) {
	Monotonic<Variables-2> f5 = getPart<Variables-2>(bot, 0);
	Monotonic<Variables-2> f3 = getPart<Variables-2>(bot, 1);
	Monotonic<Variables-2> f1 = getPart<Variables-2>(bot, 2);
	Monotonic<Variables-2> f0 = getPart<Variables-2>(bot, 3);

	assert(f0 <= f1);
	assert(f0 <= f3);
	assert(f0 <= f5);
	assert(f1 <= f5);
	assert(f3 <= f5);

	uint64_t totalIntervalSize = 0;

	constexpr size_t smallMBFLayerCount = (1 << (Variables - 2)) + 1;
	constexpr size_t smallMBFCount = dedekindNumbers[Variables-2];
	for(size_t f2Idx = 0; f2Idx < smallMBFCount; f2Idx++) {
		Monotonic<Variables-2> f2 = mbfLUT[f2Idx];
		int f2Size = f2.size();
		if(!(f0 <= f2)) continue;

		Monotonic<Variables-2> f4Min = f1 | f2;
		Monotonic<Variables-2> f6Min = f3 | f2;

		int f4MinSize = f4Min.size();
		int f6MinSize = f6Min.size();

		Monotonic<Variables-2> f7Min = f5 | f2;
		//int f7MinCount = f7Min.size();

		int f7MinIdx = inverseIndexMap[f7Min.bf.bitset.data];//idxOf(f7Min, mbfLUT, smallMBFCount);

		size_t f4MinIntervalLUTOffset = smallMBFCount * inverseIndexMap[f4Min.bf.bitset.data];//idxOf(f4Min, mbfLUT, smallMBFCount);
		size_t f6MinIntervalLUTOffset = smallMBFCount * inverseIndexMap[f6Min.bf.bitset.data];//idxOf(f6Min, mbfLUT, smallMBFCount);

		const uint16_t* thisRowAboveLUT = aboveLUT + (smallMBFCount+1) * f7MinIdx;
		uint16_t aboveCount = thisRowAboveLUT[0];

		for(uint16_t i = 0; i < aboveCount; i++) {
			uint16_t f7Idx = thisRowAboveLUT[1+i];
			int f7Size = mbfLUT[f7Idx].size();

			//int minimumSize = f2Size + f4MinSize + f6MinSize + f7Size;
			//int maximumSize = f2Size + f7Size + f7Size + f7Size;

			for(int mbfSize4 = f4MinSize; mbfSize4 <= f7Size; mbfSize4++) {
				uint32_t intervalSizeF4 = smallerIntervalSizeMatrix3D[mbfSize4 * smallMBFCount * smallMBFCount + f7Idx + f4MinIntervalLUTOffset];
				uint32_t intervalSizeF6Sum = 0;
				for(int mbfSize6 = f6MinSize; mbfSize6 <= f7Size; mbfSize6++) {
					if(f2Size + f7Size + mbfSize4 + mbfSize6 >= minimumSize) {
						intervalSizeF6Sum += smallerIntervalSizeMatrix3D[mbfSize6 * smallMBFCount * smallMBFCount + f7Idx + f6MinIntervalLUTOffset];
					}
				}
				totalIntervalSize += intervalSizeF4 * intervalSizeF6Sum;
			}
			/*uint64_t intervalSizeF4 = 0;
			for(size_t mbfSize = f4MinSize; mbfSize <= f7Size; mbfSize++) {
				intervalSizeF4 += smallerIntervalSizeMatrix3D[mbfSize * smallMBFCount * smallMBFCount + f7Idx + f4MinIntervalLUTOffset];
			}
			uint64_t intervalSizeF6 = 0;
			for(size_t mbfSize = f6MinSize; mbfSize <= f7Size; mbfSize++) {
				intervalSizeF6 += smallerIntervalSizeMatrix3D[mbfSize * smallMBFCount * smallMBFCount + f7Idx + f6MinIntervalLUTOffset];
			}

			totalIntervalSize += intervalSizeF4 * intervalSizeF6;*/
		}
	}

	return totalIntervalSize;
}

template<unsigned int Variables>
struct PawelskiPartialIntervalCounter {
	std::unique_ptr<const Monotonic<Variables-2>[]> mbfLUT;
	std::unique_ptr<const uint16_t[]> inverseIndexMap;
	std::unique_ptr<const uint16_t[]> aboveLUT;
	std::unique_ptr<const uint16_t[]> smallerIntervalSizeMatrix3D;

	PawelskiPartialIntervalCounter() {
		mbfLUT = generateAllMBFs<Variables-2>();
		inverseIndexMap = generateInverseIndexMap<Variables-2>(mbfLUT.get());
		aboveLUT = generateAboveLUT<Variables-2>(mbfLUT.get());
		smallerIntervalSizeMatrix3D = generateSliceSizeIntervalSizeMatrix<Variables-2>(mbfLUT.get(), aboveLUT.get());
	}

	//const Monotonic<Variables>& bot, const size_t minimumSize, const Monotonic<Variables-2>* mbfLUT, const uint16_t* inverseIndexMap, const uint16_t* aboveLUT, const uint16_t* smallerIntervalSizeMatrix3D
	uint64_t intervalToTopSizeAboveSize(const Monotonic<Variables>& bot, int minimumSize) const {
		return pawelskiIntervalToTopSizeAboveSize<Variables>(bot, minimumSize, mbfLUT.get(), inverseIndexMap.get(), aboveLUT.get(), smallerIntervalSizeMatrix3D.get());
	}
};
