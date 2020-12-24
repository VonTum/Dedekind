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

	constexpr FunctionInputBitSet() : data{} {
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
			this->data[i] &= other.data[i];
		}
		return *this;
	}
	constexpr FunctionInputBitSet& operator^=(const FunctionInputBitSet& other) {
		for(size_t i = 0; i < numDataBlocks; i++) {
			this->data[i] ^= other.data[i];
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
		assert(var >= 0 && var < Variables);
		FunctionInputBitSet result;
		if(var < 6) {
			constexpr uint64_t varPattern[6]{0x5555555555555555, 0x3333333333333333, 0x0F0F0F0F0F0F0F0F, 0x00FF00FF00FF00FF, 0x0000FFFF0000FFFF, 0x00000000FFFFFFFF};

			uint64_t chosenPattern = varPattern[var];
			for(uint64_t& item : result.data) {
				item = chosenPattern;
			}
		} else {
			int stepSize = 1 << var;
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

template<>
class FunctionInputBitSet<5> {
	uint32_t data;

	static constexpr uint32_t makeMask(FunctionInput inputBits) {
		return (uint32_t(1) << 31) >> inputBits.inputBits;
	}
public:

	constexpr FunctionInputBitSet() : data{0} {}

	static constexpr FunctionInput::underlyingType size() {
		return FunctionInput::underlyingType(1) << 5;
	}

	constexpr bool contains(FunctionInput inputBits) const {
		assert(inputBits.inputBits < size());
		return (this->data & makeMask(inputBits)) != 0; // bits are indexed starting from the most significant bit. This is to make sorting easier
	}

	constexpr void add(FunctionInput inputBits) {
		assert(inputBits.inputBits < size());
		assert((this->data & makeMask(inputBits)) == 0); // assert bit was off
		this->data |= makeMask(inputBits);
	}

	constexpr void remove(FunctionInput inputBits) {
		assert(inputBits.inputBits < size());
		assert((this->data & makeMask(inputBits)) != 0); // assert bit was on
		this->data &= ~makeMask(inputBits);
	}

	constexpr FunctionInputBitSet& operator|=(const FunctionInputBitSet& other) {
		this->data |= other.data;
		return *this;
	}
	constexpr FunctionInputBitSet& operator&=(const FunctionInputBitSet& other) {
		this->data &= other.data;
		return *this;
	}
	constexpr FunctionInputBitSet& operator^=(const FunctionInputBitSet& other) {
		this->data ^= other.data;
		return *this;
	}

	constexpr FunctionInputBitSet& operator<<=(unsigned int shift) {
		assert(shift < size());
		this->data >>= shift;
		return *this;
	}
	constexpr FunctionInputBitSet& operator>>=(unsigned int shift) {
		assert(shift < size());
		this->data <<= shift;
		return *this;
	}

	static constexpr FunctionInputBitSet empty() {
		FunctionInputBitSet result;
		result.data = 0x00000000;
		return result;
	}

	static constexpr FunctionInputBitSet full() {
		FunctionInputBitSet result;
		result.data = 0xFFFFFFFF;
		return result;
	}

	static constexpr FunctionInputBitSet withVarEnabled(int var) {
		assert(var >= 0 && var < 5);
		FunctionInputBitSet result;
		
		constexpr uint32_t varPattern[5]{0x55555555, 0x33333333, 0x0F0F0F0F, 0x00FF00FF, 0x0000FFFF};
		result.data = varPattern[var];
		return result;
	}
};

template<>
class FunctionInputBitSet<4> {
	uint16_t data;

	static constexpr uint16_t makeMask(FunctionInput inputBits) {
		return (uint16_t(1) << 15) >> inputBits.inputBits;
	}
public:

	constexpr FunctionInputBitSet() : data{0} {}

	static constexpr FunctionInput::underlyingType size() {
		return FunctionInput::underlyingType(1) << 4;
	}

	constexpr bool contains(FunctionInput inputBits) const {
		assert(inputBits.inputBits < size());
		return (this->data & makeMask(inputBits)) != 0; // bits are indexed starting from the most significant bit. This is to make sorting easier
	}

	constexpr void add(FunctionInput inputBits) {
		assert(inputBits.inputBits < size());
		assert((this->data & makeMask(inputBits)) == 0); // assert bit was off
		this->data |= makeMask(inputBits);
	}

	constexpr void remove(FunctionInput inputBits) {
		assert(inputBits.inputBits < size());
		assert((this->data & makeMask(inputBits)) != 0); // assert bit was on
		this->data &= ~makeMask(inputBits);
	}

	constexpr FunctionInputBitSet& operator|=(const FunctionInputBitSet& other) {
		this->data |= other.data;
		return *this;
	}
	constexpr FunctionInputBitSet& operator&=(const FunctionInputBitSet& other) {
		this->data &= other.data;
		return *this;
	}
	constexpr FunctionInputBitSet& operator^=(const FunctionInputBitSet& other) {
		this->data ^= other.data;
		return *this;
	}

	constexpr FunctionInputBitSet& operator<<=(unsigned int shift) {
		assert(shift < size());
		this->data >>= shift;
		return *this;
	}
	constexpr FunctionInputBitSet& operator>>=(unsigned int shift) {
		assert(shift < size());
		this->data <<= shift;
		return *this;
	}

	static constexpr FunctionInputBitSet empty() {
		FunctionInputBitSet result;
		result.data = 0x0000;
		return result;
	}

	static constexpr FunctionInputBitSet full() {
		FunctionInputBitSet result;
		result.data = 0xFFFF;
		return result;
	}

	static constexpr FunctionInputBitSet withVarEnabled(int var) {
		assert(var >= 0 && var < 4);
		FunctionInputBitSet result;

		constexpr uint16_t varPattern[4]{0x5555, 0x3333, 0x0F0F, 0x00FF};
		result.data = varPattern[var];
		return result;
	}
};

template<>
class FunctionInputBitSet<3> {
	uint8_t data;

	static constexpr uint8_t makeMask(FunctionInput inputBits) {
		return (uint8_t(1) << 7) >> inputBits.inputBits;
	}
public:

	constexpr FunctionInputBitSet() : data{0} {}

	static constexpr FunctionInput::underlyingType size() {
		return FunctionInput::underlyingType(1) << 3;
	}

	constexpr bool contains(FunctionInput inputBits) const {
		assert(inputBits.inputBits < size());
		return (this->data & makeMask(inputBits)) != 0; // bits are indexed starting from the most significant bit. This is to make sorting easier
	}

	constexpr void add(FunctionInput inputBits) {
		assert(inputBits.inputBits < size());
		assert((this->data & makeMask(inputBits)) == 0); // assert bit was off
		this->data |= makeMask(inputBits);
	}

	constexpr void remove(FunctionInput inputBits) {
		assert(inputBits.inputBits < size());
		assert((this->data & makeMask(inputBits)) != 0); // assert bit was on
		this->data &= ~makeMask(inputBits);
	}

	constexpr FunctionInputBitSet& operator|=(const FunctionInputBitSet& other) {
		this->data |= other.data;
		return *this;
	}
	constexpr FunctionInputBitSet& operator&=(const FunctionInputBitSet& other) {
		this->data &= other.data;
		return *this;
	}
	constexpr FunctionInputBitSet& operator^=(const FunctionInputBitSet& other) {
		this->data ^= other.data;
		return *this;
	}

	constexpr FunctionInputBitSet& operator<<=(unsigned int shift) {
		assert(shift < size());
		this->data >>= shift;
		return *this;
	}
	constexpr FunctionInputBitSet& operator>>=(unsigned int shift) {
		assert(shift < size());
		this->data <<= shift;
		return *this;
	}

	static constexpr FunctionInputBitSet empty() {
		FunctionInputBitSet result;
		result.data = 0x00;
		return result;
	}

	static constexpr FunctionInputBitSet full() {
		FunctionInputBitSet result;
		result.data = 0xFF;
		return result;
	}

	static constexpr FunctionInputBitSet withVarEnabled(int var) {
		assert(var >= 0 && var < 3);
		FunctionInputBitSet result;

		constexpr uint8_t varPattern[3]{0x55, 0x33, 0x0F};
		result.data = varPattern[var];
		return result;
	}
};

template<>
class FunctionInputBitSet<2> {
	uint8_t data;

	static constexpr uint8_t makeMask(FunctionInput inputBits) {
		return (uint8_t(1) << 3) >> inputBits.inputBits;
	}
public:

	constexpr FunctionInputBitSet() : data{0} {}

	static constexpr FunctionInput::underlyingType size() {
		return FunctionInput::underlyingType(1) << 2;
	}

	constexpr bool contains(FunctionInput inputBits) const {
		assert(inputBits.inputBits < size());
		return (this->data & makeMask(inputBits)) != 0; // bits are indexed starting from the most significant bit. This is to make sorting easier
	}

	constexpr void add(FunctionInput inputBits) {
		assert(inputBits.inputBits < size());
		assert((this->data & makeMask(inputBits)) == 0); // assert bit was off
		this->data |= makeMask(inputBits);
	}

	constexpr void remove(FunctionInput inputBits) {
		assert(inputBits.inputBits < size());
		assert((this->data & makeMask(inputBits)) != 0); // assert bit was on
		this->data &= ~makeMask(inputBits);
	}

	constexpr FunctionInputBitSet& operator|=(const FunctionInputBitSet& other) {
		this->data |= other.data;
		return *this;
	}
	constexpr FunctionInputBitSet& operator&=(const FunctionInputBitSet& other) {
		this->data &= other.data;
		return *this;
	}
	constexpr FunctionInputBitSet& operator^=(const FunctionInputBitSet& other) {
		this->data ^= other.data;
		return *this;
	}

	constexpr FunctionInputBitSet& operator<<=(unsigned int shift) {
		assert(shift < size());
		this->data >>= shift;
		return *this;
	}
	constexpr FunctionInputBitSet& operator>>=(unsigned int shift) {
		assert(shift < size());
		this->data <<= shift;
		return *this;
	}

	static constexpr FunctionInputBitSet empty() {
		FunctionInputBitSet result;
		result.data = 0x00;
		return result;
	}

	static constexpr FunctionInputBitSet full() {
		FunctionInputBitSet result;
		result.data = 0xFF;
		return result;
	}

	static constexpr FunctionInputBitSet withVarEnabled(int var) {
		assert(var >= 0 && var < 2);
		FunctionInputBitSet result;

		constexpr uint8_t varPattern[2]{0x55, 0x33};
		result.data = varPattern[var];
		return result;
	}
};

template<>
class FunctionInputBitSet<1> {
	uint8_t data;

	static constexpr uint8_t makeMask(FunctionInput inputBits) {
		return (uint8_t(1) << 1) >> inputBits.inputBits;
	}
public:

	constexpr FunctionInputBitSet() : data{0} {}

	static constexpr FunctionInput::underlyingType size() {
		return FunctionInput::underlyingType(1) << 1;
	}

	constexpr bool contains(FunctionInput inputBits) const {
		assert(inputBits.inputBits < size());
		return (this->data & makeMask(inputBits)) != 0; // bits are indexed starting from the most significant bit. This is to make sorting easier
	}

	constexpr void add(FunctionInput inputBits) {
		assert(inputBits.inputBits < size());
		assert((this->data & makeMask(inputBits)) == 0); // assert bit was off
		this->data |= makeMask(inputBits);
	}

	constexpr void remove(FunctionInput inputBits) {
		assert(inputBits.inputBits < size());
		assert((this->data & makeMask(inputBits)) != 0); // assert bit was on
		this->data &= ~makeMask(inputBits);
	}

	constexpr FunctionInputBitSet& operator|=(const FunctionInputBitSet& other) {
		this->data |= other.data;
		return *this;
	}
	constexpr FunctionInputBitSet& operator&=(const FunctionInputBitSet& other) {
		this->data &= other.data;
		return *this;
	}
	constexpr FunctionInputBitSet& operator^=(const FunctionInputBitSet& other) {
		this->data ^= other.data;
		return *this;
	}

	constexpr FunctionInputBitSet& operator<<=(unsigned int shift) {
		assert(shift < size());
		this->data >>= shift;
		return *this;
	}
	constexpr FunctionInputBitSet& operator>>=(unsigned int shift) {
		assert(shift < size());
		this->data <<= shift;
		return *this;
	}

	static constexpr FunctionInputBitSet empty() {
		FunctionInputBitSet result;
		result.data = 0x00;
		return result;
	}

	static constexpr FunctionInputBitSet full() {
		FunctionInputBitSet result;
		result.data = 0xFF;
		return result;
	}

	static constexpr FunctionInputBitSet withVarEnabled(int var) {
		assert(var >= 0 && var < 1);
		FunctionInputBitSet result;

		constexpr uint8_t varPattern[1]{0x55};
		result.data = varPattern[var];
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


