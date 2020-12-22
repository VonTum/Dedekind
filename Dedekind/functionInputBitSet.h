#pragma once

#include <cassert>
#include <cstdint>

#include "functionInput.h"

template<int Variables>
class FunctionInputBitSet {
	static_assert(Variables >= 6, "Other implementations must provide Variables < 6");

	static constexpr size_t numDataBlocks = 1 << (Variables - 6);

	uint64_t data[numDataBlocks]; // 6 bits per uint64_t, smaller ones should have more specific implementations


	constexpr uint64_t& getSubPart(FunctionInput inputBits) {
		assert(inputBits.inputBits >> 6 < numDataBlocks);
		return this->data[inputBits.inputBits >> 6];
	}
	constexpr uint64_t getSubPart(FunctionInput inputBits) const {
		assert(inputBits.inputBits >> 6 < numDataBlocks);
		return this->data[inputBits.inputBits >> 6];
	}
	static constexpr uint64_t makeMask(FunctionInput inputBits) {
		return (uint64_t(1) << 63) >> (inputBits.inputBits & 0x3F);
	}
public:

	FunctionInputBitSet() : data{} {
		for(size_t i = 0; i < numDataBlocks; i++) {
			this->data[i] = 0;
		}
	}

	static constexpr FunctionInput::underlyingType size() {
		return FunctionInput::underlyingType(1) << Variables;
	}

	constexpr bool contains(FunctionInput inputBits) const {
		assert(inputBits.inputBits < size());
		uint64_t subPart = getSubPart(inputBits);
		return (subPart & makeMask(inputBits)) != 0; // bits are indexed starting from the most significant bit. This is to make sorting easier
	}

	constexpr void add(FunctionInput inputBits) {
		assert(inputBits.inputBits < size());
		uint64_t& subPart = getSubPart(inputBits);
		assert((subPart & makeMask(inputBits)) == 0); // assert bit was off
		subPart |= makeMask(inputBits);
	}

	constexpr void remove(FunctionInput inputBits) {
		assert(inputBits.inputBits < size());
		uint64_t& subPart = getSubPart(inputBits);
		assert((subPart & makeMask(inputBits)) != 0); // assert bit was on
		subPart &= ~makeMask(inputBits);
	}

	constexpr FunctionInputBitSet& operator|=(const FunctionInputBitSet& other) {
		for(size_t i = 0; i < numDataBlocks; i++) {
			this->data[i] |= other.data[i];
		}
		return *this;
	}
	constexpr FunctionInputBitSet& operator&=(const FunctionInputBitSet& other) {
		for(size_t i = 0; i < numDataBlocks; i++) {
			this->data[i] |= other.data[i];
		}
		return *this;
	}
	constexpr FunctionInputBitSet& operator^=(const FunctionInputBitSet& other) {
		for(size_t i = 0; i < numDataBlocks; i++) {
			this->data[i] |= other.data[i];
		}
		return *this;
	}

	constexpr FunctionInputBitSet& operator<<=(unsigned int shift) {
		int subPartOffset = (shift >> 6) + 1;

		int relativeShift = shift & 0x3F;

		for(int i = 0; i < numDataBlocks - subPartOffset; i++) {

		}
	}

	template<unsigned int ShiftAmount>
	constexpr FunctionInputBitSet constLeftShift() {
		
	}

	static constexpr FunctionInputBitSet empty() {
		FunctionInputBitSet result;
		for(uint64_t& item : result.data) {
			item = 0x0000000000000000;
		}
		return result;
	}

	static constexpr FunctionInputBitSet full() {
		FunctionInputBitSet result;
		for(uint64_t& item : result.data) {
			item = 0xFFFFFFFFFFFFFFFF;
		}
		return result;
	}

	static constexpr FunctionInputBitSet withVarEnabled(int var) {
		assert(var >= 1 && var < Variables);
		FunctionInputBitSet result;
		if(var <= 6) {
			constexpr uint64_t varPattern[6]{0x5555555555555555, 0x3333333333333333, 0x0F0F0F0F0F0F0F0F, 0x00FF00FF00FF00FF, 0x0000FFFF0000FFFF, 0x00000000FFFFFFFF};

			uint64_t chosenPattern = varPattern[var - 1];
			for(uint64_t& item : result.data) {
				item = chosenPattern;
			}
		} else {
			int stepSize = (1 << (var - 1));
			for(int curIndex = 0; curIndex < numDataBlocks; curIndex += stepSize*2) {
				for(int indexInStep = 0; indexInStep < stepSize; indexInStep++) {
					result.data[curIndex + indexInStep] = 0x0000000000000000;
				}
				for(int indexInStep = 0; indexInStep < stepSize; indexInStep++) {
					result.data[curIndex + stepSize + indexInStep] = 0xFFFFFFFFFFFFFFFF;
				}
			}
		}
		return result;
	}
};


template<size_t Variables>
FunctionInputBitSet<Variables> operator|(FunctionInputBitSet<Variables> result, const FunctionInputBitSet<Variables>& b) {
	result |= b;
	return result;
}
template<size_t Variables>
FunctionInputBitSet<Variables> operator&(FunctionInputBitSet<Variables> result, const FunctionInputBitSet<Variables>& b) {
	result &= b;
	return result;
}
template<size_t Variables>
FunctionInputBitSet<Variables> operator^(FunctionInputBitSet<Variables> result, const FunctionInputBitSet<Variables>& b) {
	result ^= b;
	return result;
}


