#pragma once

#include <intrin.h>

inline uint32_t lowerBitsOnes(int n) {
	return (1U << n) - 1;
}
inline uint32_t oneBitInterval(int n, int offset) {
	return lowerBitsOnes(n) << offset;
}

struct FunctionInput {
	uint32_t inputBits;

	inline bool operator[](int index) const {
		return (inputBits & (1 << index)) != 0;
	}

	inline void enableInput(int index) {
		inputBits |= (1 << index);
	}
	inline void disableInput(int index) {
		inputBits &= !(1 << index);
	}
	inline int getNumberEnabled() const {
		return __popcnt(inputBits);
	}
	inline bool empty() const {
		return inputBits == 0;
	}
	inline FunctionInput swizzle(const std::vector<int>& swiz) const {
		FunctionInput result{0};

		for(int i = 0; i < swiz.size(); i++) {
			if((*this)[swiz[i]]) {
				result.enableInput(i);
			}
		}

		return result;
	}

	inline FunctionInput& operator&=(FunctionInput f) { this->inputBits &= f.inputBits; return *this; }
	inline FunctionInput& operator|=(FunctionInput f) { this->inputBits |= f.inputBits; return *this; }
	inline FunctionInput& operator^=(FunctionInput f) { this->inputBits ^= f.inputBits; return *this; }
	inline FunctionInput& operator>>=(size_t shift) { this->inputBits >>= shift; return *this; }
	inline FunctionInput& operator<<=(size_t shift) { this->inputBits <<= shift; return *this; }

	inline operator uint32_t() const { return inputBits; }
	inline operator size_t() const { return static_cast<size_t>(inputBits); }
};

inline FunctionInput operator&(FunctionInput a, FunctionInput b) { return FunctionInput{a.inputBits & b.inputBits}; }
inline FunctionInput operator|(FunctionInput a, FunctionInput b) { return FunctionInput{a.inputBits | b.inputBits}; }
inline FunctionInput operator^(FunctionInput a, FunctionInput b) { return FunctionInput{a.inputBits ^ b.inputBits}; }
inline FunctionInput operator>>(FunctionInput a, size_t shift) { return FunctionInput{a.inputBits >> shift}; }
inline FunctionInput operator<<(FunctionInput a, size_t shift) { return FunctionInput{a.inputBits << shift}; }

inline bool operator==(FunctionInput a, FunctionInput b) {
	return a.inputBits == b.inputBits;
}
inline bool operator!=(FunctionInput a, FunctionInput b) {
	return a.inputBits != b.inputBits;
}
// returns true if all true inputs in a are also true in b
inline bool isSubSet(FunctionInput a, FunctionInput b) {
	return (a.inputBits & b.inputBits) == a.inputBits;
}
inline bool overlaps(FunctionInput a, FunctionInput b) {
	return (a.inputBits & b.inputBits) != 0;
}
