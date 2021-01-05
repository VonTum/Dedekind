#pragma once


#include "functionInput.h"
#include "bitSet.h"

template<unsigned int Variables>
class FunctionInputBitSet {
	static_assert(Variables >= 1, "Cannot make 0 variable FunctionInputBitSet");

	BitSet<size_t(1) << Variables> bitset;
public:

	FunctionInputBitSet() : bitset() {}
	FunctionInputBitSet(const BitSet<size_t(1) << Variables>& bitset) : bitset(bitset) {}

	static constexpr FunctionInput::underlyingType size() {
		return FunctionInput::underlyingType(1) << Variables;
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
		return FunctionInputBitSet<Variables>(BitSet<size_t(1) << Variables>::empty());
	}

	static constexpr FunctionInputBitSet full() {
		return FunctionInputBitSet<Variables>(BitSet<size_t(1) << Variables>::full());
	}

	static constexpr FunctionInputBitSet varMask(unsigned int var) {
		assert(var < Variables);

		constexpr uint64_t varPattern[6]{0xaaaaaaaaaaaaaaaa, 0xcccccccccccccccc, 0xF0F0F0F0F0F0F0F0, 0xFF00FF00FF00FF00, 0xFFFF0000FFFF0000, 0xFFFFFFFF00000000};

		FunctionInputBitSet<Variables> result;

		if constexpr(Variables >= 6) {
			if(var < 6) {
				uint64_t chosenPattern = varPattern[var];
				for(uint64_t& item : result.bitset.data) {
					item = chosenPattern;
				}
			} else {
				size_t stepSize = size_t(1) << (var - 6);
				assert(stepSize > 0);
				for(size_t curIndex = 0; curIndex < result.bitset.BLOCK_COUNT; curIndex += stepSize * 2) {
					for(size_t indexInStep = 0; indexInStep < stepSize; indexInStep++) {
						result.bitset.data[curIndex + indexInStep] = 0x0000000000000000;
					}
					for(size_t indexInStep = 0; indexInStep < stepSize; indexInStep++) {
						result.bitset.data[curIndex + stepSize + indexInStep] = 0xFFFFFFFFFFFFFFFF;
					}
				}
			}
		} else if constexpr(Variables >= 3) {
			result.bitset.data = static_cast<decltype(result.bitset.data)>(varPattern[var]);
		} else if constexpr(Variables == 2) {
			if(var == 0) {
				result.bitset.data = uint8_t(0b1010);
			} else {
				result.bitset.data = uint8_t(0b1100);
			}
		} else if constexpr(Variables == 1) {
			result.bitset.data = uint8_t(0b10);
		}

		return result;
	}

	bool isEmpty() const {
		return this->bitset.isEmpty();
	}
	bool isFull() const {
		return this->bitset.isFull();
	}

	bool hasVariable(unsigned int var) const {
		FunctionInputBitSet<Variables> mask = FunctionInputBitSet<Variables>::varMask(var);

		FunctionInputBitSet<Variables> checkVar = *this & mask;

		return !checkVar.isEmpty();
	}

	FunctionInputBitSet<Variables> moveVariable(unsigned int original, unsigned int destination) const {
		assert(!this->hasVariable(destination));

		FunctionInputBitSet<Variables> originalMask = FunctionInputBitSet<Variables>::varMask(original);
		
		// shift amount
		// assuming source < destination (moving a lower variable to a higher spot)
		// so for example, every source element will be 000s0000 -> 0d000000
		// + 1<<d - 1<<s

		if(original < destination) {
			unsigned int shift = (1 << destination) - (1 << original);

			return ((*this & originalMask) << shift) | (*this & ~originalMask);
		} else {
			unsigned int shift = (1 << original) - (1 << destination);

			return ((*this & originalMask) >> shift) | (*this & ~originalMask);
		}
	}

	FunctionInputBitSet<Variables> swapVars(unsigned int var1, unsigned int var2) const {
		assert(!this->hasVariable(destination));

		FunctionInputBitSet<Variables> mask1 = FunctionInputBitSet<Variables>::varMask(var1);
		FunctionInputBitSet<Variables> mask2 = FunctionInputBitSet<Variables>::varMask(var2);

		FunctionInputBitSet<Variables> stayingElems = *this & (~mask1 & ~mask2 | mask1 & mask2);

		if(var1 < var2) {
			unsigned int shift = (1 << var2) - (1 << var1);

			return ((*this & mask1 & ~mask2) << shift) | ((*this & mask2 & ~mask1) >> shift) | stayingElems;
		} else {
			unsigned int shift = (1 << var1) - (1 << var2);

			return ((*this & mask2 & ~mask1) << shift) | ((*this & mask1 & ~mask2) >> shift) | stayingElems;
		}
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
