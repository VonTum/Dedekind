#pragma once

#include <algorithm>

#include "functionInput.h"
#include "bitSet.h"

template<unsigned int Variables>
class FunctionInputBitSet {
	static_assert(Variables >= 1, "Cannot make 0 variable FunctionInputBitSet");

	BitSet<size_t(1) << Variables> bitset;
public:

	FunctionInputBitSet() : bitset() {}
	FunctionInputBitSet(const BitSet<size_t(1) << Variables>& bitset) : bitset(bitset) {}
	FunctionInputBitSet(const FunctionInputSet& inputSet) : bitset() {
		for(FunctionInput fi : inputSet) {
			bitset.set(fi.inputBits);
		}
	}

	static constexpr FunctionInput::underlyingType maxSize() {
		return FunctionInput::underlyingType(1) << Variables;
	}

	FunctionInput::underlyingType size() const {
		return bitset.count();
	}
	uint64_t hash() const {
		return this->bitset.hash();
	}

	constexpr bool contains(FunctionInput::underlyingType index) const {
		return bitset.get(index);
	}

	constexpr void add(FunctionInput::underlyingType index) {
		assert(bitset.get(index) == false); // assert bit was off
		return bitset.set(index);
	}

	constexpr void remove(FunctionInput::underlyingType index) {
		assert(bitset.get(index) == true); // assert bit was on
		return bitset.reset(index);
	}

	constexpr FunctionInputBitSet& operator|=(const FunctionInputBitSet& other) {
		this->bitset |= other.bitset;
		return *this;
	}
	constexpr FunctionInputBitSet& operator&=(const FunctionInputBitSet& other) {
		this->bitset &= other.bitset;
		return *this;
	}
	constexpr FunctionInputBitSet& operator^=(const FunctionInputBitSet& other) {
		this->bitset ^= other.bitset;
		return *this;
	}
	constexpr FunctionInputBitSet operator~() const {
		return FunctionInputBitSet(~this->bitset);
	}
	constexpr FunctionInputBitSet& operator<<=(unsigned int shift) {
		this->bitset <<= shift;
		return *this;
	}
	constexpr FunctionInputBitSet& operator>>=(unsigned int shift) {
		this->bitset >>= shift;
		return *this;
	}
	bool operator==(const FunctionInputBitSet& other) const {
		return this->bitset == other.bitset;
	}
	bool operator!=(const FunctionInputBitSet& other) const {
		return this->bitset != other.bitset;
	}

	static constexpr FunctionInputBitSet empty() {
		return FunctionInputBitSet(BitSet<size_t(1) << Variables>::empty());
	}

	static constexpr FunctionInputBitSet full() {
		return FunctionInputBitSet(BitSet<size_t(1) << Variables>::full());
	}

	static constexpr BitSet<(1 << Variables)> varMask(unsigned int var) {
		assert(var < Variables);

		constexpr uint64_t varPattern[6]{0xaaaaaaaaaaaaaaaa, 0xcccccccccccccccc, 0xF0F0F0F0F0F0F0F0, 0xFF00FF00FF00FF00, 0xFFFF0000FFFF0000, 0xFFFFFFFF00000000};

		BitSet<(1 << Variables)> result;

		if constexpr(Variables >= 7) {
			if(var < 6) {
				uint64_t chosenPattern = varPattern[var];
				for(uint64_t& item : result.data) {
					item = chosenPattern;
				}
			} else {
				size_t stepSize = size_t(1) << (var - 6);
				assert(stepSize > 0);
				for(size_t curIndex = 0; curIndex < result.BLOCK_COUNT; curIndex += stepSize * 2) {
					for(size_t indexInStep = 0; indexInStep < stepSize; indexInStep++) {
						result.data[curIndex + indexInStep] = 0x0000000000000000;
					}
					for(size_t indexInStep = 0; indexInStep < stepSize; indexInStep++) {
						result.data[curIndex + stepSize + indexInStep] = 0xFFFFFFFFFFFFFFFF;
					}
				}
			}
		} else if constexpr(Variables >= 3) {
			result.data = static_cast<decltype(result.data)>(varPattern[var]);
		} else if constexpr(Variables == 2) {
			if(var == 0) {
				result.data = uint8_t(0b1010);
			} else {
				result.data = uint8_t(0b1100);
			}
		} else if constexpr(Variables == 1) {
			result.data = uint8_t(0b10);
		}

		return result;
	}

	size_t countVariableOccurences(unsigned int var) const {
		return (this->bitset & FunctionInputBitSet::varMask(var)).count();
	}

	bool isEmpty() const {
		return this->bitset.isEmpty();
	}
	bool isFull() const {
		return this->bitset.isFull();
	}

	bool hasVariable(unsigned int var) const {
		FunctionInputBitSet mask = FunctionInputBitSet::varMask(var);

		FunctionInputBitSet checkVar = *this & mask;

		return !checkVar.isEmpty();
	}

	unsigned int span() const {
		unsigned int result = 0;
		for(unsigned int var = 0; var < Variables; var++) {
			if(hasVariable(var)) {
				result |= 1 << var;
			}
		}
		assert(result < 128);
		return result;
	}

	void move(unsigned int original, unsigned int destination) {
		assert(!this->hasVariable(destination));

		BitSet<(1 << Variables)> originalMask = FunctionInputBitSet::varMask(original);

		// shift amount
		// assuming source < destination (moving a lower variable to a higher spot)
		// so for example, every source element will be 000s0000 -> 0d000000
		// + 1<<d - 1<<s

		if(original < destination) {
			unsigned int shift = (1 << destination) - (1 << original);

			bitset = ((bitset & originalMask) << shift) | (bitset & ~originalMask);
		} else {
			unsigned int shift = (1 << original) - (1 << destination);

			bitset = ((bitset & originalMask) >> shift) | (bitset & ~originalMask);
		}
	}

	void swap(unsigned int var1, unsigned int var2) {
		BitSet<(1 << Variables)> mask1 = FunctionInputBitSet::varMask(var1);
		BitSet<(1 << Variables)> mask2 = FunctionInputBitSet::varMask(var2);

		BitSet<(1 << Variables)> stayingElems = bitset & (~mask1 & ~mask2 | mask1 & mask2);

		if(var1 < var2) {
			unsigned int shift = (1 << var2) - (1 << var1);

			bitset = ((bitset & mask1 & ~mask2) << shift) | ((bitset & mask2 & ~mask1) >> shift) | stayingElems;
		} else {
			unsigned int shift = (1 << var1) - (1 << var2);

			bitset = ((bitset & mask2 & ~mask1) << shift) | ((bitset & mask1 & ~mask2) >> shift) | stayingElems;
		}
	}


	template<typename Func>
	void forEachPermutation(unsigned int fromVar, unsigned int toVar, const Func& func) {
		if(fromVar == toVar) {
			func(*this);
		} else {
			this->forEachPermutation(fromVar + 1, toVar, func);
			for(unsigned int swapping = fromVar + 1; swapping < toVar; swapping++) {
				this->swap(fromVar, swapping);
				this->forEachPermutation(fromVar + 1, toVar, func);
				this->swap(fromVar, swapping);
			}
		}
	}

	template<typename Func>
	void forEachPermutationInGroups(unsigned int fromVar, unsigned int toVar, unsigned int* nextGroup, unsigned int* groupsEnd, const Func& func) {
		assert(fromVar <= toVar && toVar <= Variables);
		if(fromVar == toVar) {
			if(nextGroup == groupsEnd) {
				func(*this);
			} else {
				forEachPermutationInGroups(toVar, toVar + *nextGroup, nextGroup + 1, groupsEnd, func);
			}
		} else {
			this->forEachPermutationInGroups(fromVar + 1, toVar, nextGroup, groupsEnd, func);
			for(unsigned int swapping = fromVar + 1; swapping < toVar; swapping++) {
				this->swap(fromVar, swapping);
				this->forEachPermutationInGroups(fromVar + 1, toVar, nextGroup, groupsEnd, func);
				this->swap(fromVar, swapping);
			}
		}
	}

	FunctionInputBitSet canonize() const {
		FunctionInputBitSet copy = *this;

		unsigned int counts[Variables];
		for(unsigned int v = 0; v < Variables; v++) {
			counts[v] = copy.countVariableOccurences(v);
		}

		// Fill all empty spots
		unsigned int varIdx = 0;
		for(; varIdx < Variables; varIdx++) {
			if(counts[varIdx] == 0) {
				for(unsigned int finderFromBack = Variables - 1; finderFromBack > varIdx; finderFromBack--) {
					if(counts[finderFromBack] != 0) {
						copy.move(finderFromBack, varIdx); // Fill in variables 0->#vars
						counts[varIdx] = counts[finderFromBack];
						counts[finderFromBack] = 0;
						goto nextVar;
					}
				}
				break; // all variables covered
			}
			nextVar:;
		}
		// varIdx is now number of variables

		// Assign groups
		struct Group {
			unsigned int value;
			unsigned int groupSize;
		} groups[Variables];
		unsigned int groupCount = 0;

		for(unsigned int i = 0; i < varIdx; i++) {
			unsigned int occ = counts[i];
			for(unsigned int g = 0; g < groupCount; g++) {
				if(groups[g].value == occ) {
					groups[g].groupSize++;
					goto groupFound; // group found
				}
			}
			// group not found, add new
			groups[groupCount++] = Group{occ, 1};
			groupFound:;
		}

		std::sort(groups, groups + groupCount, [](const Group& a, const Group& b) {
			if(a.groupSize != b.groupSize) {
				return a.groupSize < b.groupSize;
			} else {
				return a.value < b.value;
			}
		});

		unsigned int sortedTop = 0;
		for(unsigned int g = 0; g < groupCount; g++) {
			unsigned int occ = groups[g].value;
			for(unsigned int v = sortedTop; v < varIdx; v++) {
				if(counts[v] == occ) {
					// skip if already in place
					if(v != sortedTop) {
						std::swap(counts[v], counts[sortedTop]);
						copy.swap(v, sortedTop);
					}
					sortedTop++;
				}
			}
		}

		unsigned int numberOfSize1Groups = 0;
		for(; numberOfSize1Groups < groupCount; numberOfSize1Groups++) {
			if(groups[numberOfSize1Groups].groupSize != 1) break;
		}

		// assert is correct
		for(unsigned int v = 0; v < Variables; v++) {
			assert(counts[v] == copy.countVariableOccurences(v));
		}
		unsigned int curTestOffset = 0;
		for(unsigned int g = 0; g < groupCount; g++) {
			for(unsigned int i = 0; i < groups[g].groupSize; i++) {
				groups[g].value = copy.countVariableOccurences(curTestOffset++);
			}
		}

		/*std::cout << "groupCount: " << groupCount << "\n";
		for(int i = 0; i < groupCount; i++) {
			std::cout << groups[i].groupSize << ":" << groups[i].value << ",";
		}*/

		if(numberOfSize1Groups < groupCount) {
			unsigned int groupSizeBuffer[Variables];
			for(unsigned int i = 0; i < groupCount; i++) groupSizeBuffer[i] = groups[i].groupSize;

			FunctionInputBitSet best = copy;
			copy.forEachPermutationInGroups(numberOfSize1Groups, numberOfSize1Groups + groupSizeBuffer[numberOfSize1Groups], groupSizeBuffer + 1 + numberOfSize1Groups, groupSizeBuffer + groupCount, [&best](const FunctionInputBitSet& permut) {
				if(permut.bitset < best.bitset) {
					best = permut;
				}
			});
			copy = best;
		}

		return copy;
	}
};

template<unsigned int Variables>
FunctionInputBitSet<Variables> operator|(FunctionInputBitSet<Variables> result, const FunctionInputBitSet<Variables>& b) {
	result |= b;
	return result;
}
template<unsigned int Variables>
FunctionInputBitSet<Variables> operator&(FunctionInputBitSet<Variables> result, const FunctionInputBitSet<Variables>& b) {
	result &= b;
	return result;
}
template<unsigned int Variables>
FunctionInputBitSet<Variables> operator^(FunctionInputBitSet<Variables> result, const FunctionInputBitSet<Variables>& b) {
	result ^= b;
	return result;
}
template<unsigned int Variables>
FunctionInputBitSet<Variables> operator<<(FunctionInputBitSet<Variables> result, unsigned int shift) {
	result <<= shift;
	return result;
}
template<unsigned int Variables>
FunctionInputBitSet<Variables> operator>>(FunctionInputBitSet<Variables> result, unsigned int shift) {
	result >>= shift;
	return result;
}

