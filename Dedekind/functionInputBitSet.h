#pragma once

#include <cassert>
#include <cstdint>

template<size_t Variables>
class FunctionInputBitSet {
	uint64_t data[1 << (Variables - 6)]; // 6 bits per uint64_t, smaller ones should have more specific implementations

	static constexpr uint64_t makeMask(uint64_t inputBits) {
		return (1 << 63) >> (inputBits & 0x3F);
	}
public:

	FunctionInputBitSet() : data{} {
		for(size_t i = 0; i < 1 << (Variables - 6); i++) {
			this->data[i] = 0;
		}
	}

	constexpr bool contains(uint64_t inputBits) const {
		assert(inputBits < 1 << Variables);
		uint64_t subPart = data[inputBits >> 6];
		return (subPart & makeMask(inputBits)) != 0; // bits are indexed starting from the most significant bit. This is to make sorting easier
	}

	constexpr void add(uint64_t inputBits) const {
		assert(inputBits < 1 << Variables);
		uint64_t subPart = data[inputBits >> 6];
		assert(subPart & makeMask(inputBits) == 0); // assert bit was off
		subPart |= makeMask(inputBits);
	}

	constexpr void remove(uint64_t inputBits) const {
		assert(inputBits < 1 << Variables);
		uint64_t subPart = data[inputBits >> 6];
		assert(subPart & makeMask(inputBits) != 0); // assert bit was on
		subPart &= ~makeMask(inputBits);
	}

	constexpr FunctionInputBitSet& operator|=(const FunctionInputBitSet& other) {
		for(size_t i = 0; i < 1 << (Variables - 6); i++) {
			this->data[i] |= other.data[i];
		}
		return *this;
	}
	constexpr FunctionInputBitSet& operator&=(const FunctionInputBitSet& other) {
		for(size_t i = 0; i < 1 << (Variables - 6); i++) {
			this->data[i] |= other.data[i];
		}
		return *this;
	}
	constexpr FunctionInputBitSet& operator^=(const FunctionInputBitSet& other) {
		for(size_t i = 0; i < 1 << (Variables - 6); i++) {
			this->data[i] |= other.data[i];
		}
		return *this;
	}

	constexpr FunctionInputBitSet& operator<<=(unsigned long long shift) {

	}

	template<size_t ShiftAmount>
	constexpr FunctionInputBitSet constLeftShift() {
		
	}

	static constexpr FunctionInputBitSet empty() {
		FunctionInputBitSet result;
		for(size_t i = 0; i < 1 << (Variables - 6); i++) {
			result.data[i] = 0x0000000000000000;
		}
		return result;
	}

	static constexpr FunctionInputBitSet full() {
		FunctionInputBitSet result;
		for(size_t i = 0; i < 1 << (Variables - 6); i++) {
			result.data[i] = 0xFFFFFFFFFFFFFFFF;
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


