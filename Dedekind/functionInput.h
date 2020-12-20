#pragma once

#include <intrin.h>

#include "iteratorFactory.h"

#define MAX_DEDEKIND 10

#define FUNCINPUT_UNDERLYING_TYPE uint32_t

/*template<typename IntType>
inline IntType lowerBitsOnes(IntType n) {
	return (1U << n) - 1;
}
template<typename IntType>
inline IntType oneBitInterval(IntType n, IntType offset) {
	return lowerBitsOnes(n) << offset;
}*/

struct FunctionInputIter {
	FUNCINPUT_UNDERLYING_TYPE curFuncInput;
	int curIndex;

	FunctionInputIter(FUNCINPUT_UNDERLYING_TYPE curFuncInput) : curFuncInput(curFuncInput), curIndex(0) {
		while((this->curFuncInput & 1) == 0 && this->curFuncInput != 0) {
			this->curFuncInput >>= 1;
			curIndex++;
		}
	}

	void operator++() {
		do {
			curFuncInput >>= 1;
			curIndex++;
		} while((curFuncInput & 1) == 0 && curFuncInput != 0);
	}
	int operator*() const {
		return curIndex;
	}
	bool operator!=(IteratorEnd) { return curFuncInput != 0; }
};

struct FunctionInput {
	using underlyingType = FUNCINPUT_UNDERLYING_TYPE;
	underlyingType inputBits = 0;

	static FunctionInput allOnes(int count) {
		return FunctionInput{(1U << count) - 1};
	}

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
	inline int getHighestEnabled() const {
		int total = 0;
		underlyingType v = inputBits;
		while(v != 0) {
			total++;
			v >>= 1;
		}
		return total;
	}
	inline bool empty() const {
		return inputBits == 0;
	}
	template<typename PermutationVector>
	inline FunctionInput swizzle(const PermutationVector& swiz) const {
		FunctionInput result{0};

		for(int i = 0; i < swiz.size(); i++) {
			FunctionInput::underlyingType bit = (this->inputBits & (1 << swiz[i])) >> swiz[i];
			result.inputBits |= bit << i;
		}

		return result;
	}

	inline FunctionInput& operator&=(FunctionInput f) { this->inputBits &= f.inputBits; return *this; }
	inline FunctionInput& operator|=(FunctionInput f) { this->inputBits |= f.inputBits; return *this; }
	inline FunctionInput& operator^=(FunctionInput f) { this->inputBits ^= f.inputBits; return *this; }
	inline FunctionInput& operator>>=(size_t shift) { this->inputBits >>= shift; return *this; }
	inline FunctionInput& operator<<=(size_t shift) { this->inputBits <<= shift; return *this; }

	inline operator FUNCINPUT_UNDERLYING_TYPE() const { return inputBits; }
	inline operator size_t() const { return static_cast<size_t>(inputBits); }

	inline FunctionInputIter begin() const {
		return FunctionInputIter(inputBits);
	}
	inline IteratorEnd end() const { return IteratorEnd(); }
};

inline FunctionInput operator&(FunctionInput a, FunctionInput b) { return FunctionInput{static_cast<FunctionInput::underlyingType>(a.inputBits & b.inputBits)}; }
inline FunctionInput operator|(FunctionInput a, FunctionInput b) { return FunctionInput{static_cast<FunctionInput::underlyingType>(a.inputBits | b.inputBits)}; }
inline FunctionInput operator^(FunctionInput a, FunctionInput b) { return FunctionInput{static_cast<FunctionInput::underlyingType>(a.inputBits ^ b.inputBits)}; }
inline FunctionInput operator>>(FunctionInput a, size_t shift) { return FunctionInput{static_cast<FunctionInput::underlyingType>(a.inputBits >> shift)}; }
inline FunctionInput operator<<(FunctionInput a, size_t shift) { return FunctionInput{static_cast<FunctionInput::underlyingType>(a.inputBits << shift)}; }

inline bool operator==(FunctionInput a, FunctionInput b) {
	return a.inputBits == b.inputBits;
}
inline bool operator!=(FunctionInput a, FunctionInput b) {
	return a.inputBits != b.inputBits;
}
// returns true if all true inputs in a are also true in b (a <= b)
inline bool isSubSet(FunctionInput a, FunctionInput b) {
	return (a.inputBits & b.inputBits) == a.inputBits;
}
inline bool overlaps(FunctionInput a, FunctionInput b) {
	return (a.inputBits & b.inputBits) != 0;
}
