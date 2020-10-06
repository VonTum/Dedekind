#pragma once

#include <vector>
#include <intrin.h>

uint32_t lowerBitsOnes(int n) {
	return (1U << n) - 1;
}
uint32_t oneBitInterval(int n, int offset) {
	return lowerBitsOnes(n) << offset;
}

struct FunctionInput {
	uint32_t inputBits;

	bool operator[](int index) const {
		return (inputBits & (1 << index)) != 0;
	}

	void enableInput(int index) {
		inputBits |= (1 << index);
	}
	void disableInput(int index) {
		inputBits &= !(1 << index);
	}
	int getNumberEnabled() const {
		return __popcnt(inputBits);
	}
	bool empty() const {
		return inputBits == 0;
	}
	FunctionInput swizzle(const std::vector<int>& swiz) const {
		FunctionInput result{0};

		for(int i = 0; i < swiz.size(); i++) {
			if((*this)[swiz[i]]) {
				result.enableInput(i);
			}
		}

		return result;
	}

	FunctionInput& operator&=(FunctionInput f) { this->inputBits &= f.inputBits; return *this; }
	FunctionInput& operator|=(FunctionInput f) { this->inputBits |= f.inputBits; return *this; }
	FunctionInput& operator^=(FunctionInput f) { this->inputBits ^= f.inputBits; return *this; }
	FunctionInput& operator>>=(size_t shift) { this->inputBits >>= shift; return *this; }
	FunctionInput& operator<<=(size_t shift) { this->inputBits <<= shift; return *this; }
};

FunctionInput operator&(FunctionInput a, FunctionInput b) { return FunctionInput{a.inputBits & b.inputBits}; }
FunctionInput operator|(FunctionInput a, FunctionInput b) { return FunctionInput{a.inputBits | b.inputBits}; }
FunctionInput operator^(FunctionInput a, FunctionInput b) { return FunctionInput{a.inputBits ^ b.inputBits}; }
FunctionInput operator>>(FunctionInput a, size_t shift) { return FunctionInput{a.inputBits >> shift}; }
FunctionInput operator<<(FunctionInput a, size_t shift) { return FunctionInput{a.inputBits << shift}; }

bool operator==(FunctionInput a, FunctionInput b) {
	return a.inputBits == b.inputBits;
}
bool operator!=(FunctionInput a, FunctionInput b) {
	return a.inputBits != b.inputBits;
}
// returns true if all true inputs in a are also true in b
bool isSubSet(FunctionInput a, FunctionInput b) {
	return (a.inputBits & b.inputBits) == a.inputBits;
}
bool overlaps(FunctionInput a, FunctionInput b) {
	return (a.inputBits & b.inputBits) != 0;
}

FunctionInput span(const std::vector<FunctionInput>& inputSet) {
	FunctionInput result{0};

	for(FunctionInput f : inputSet) {
		result |= f;
	}

	return result;
}

void swizzleVector(const std::vector<int>& permutation, const std::vector<FunctionInput>& input, std::vector<FunctionInput>& output) {
	for(size_t i = 0; i < input.size(); i++) {
		output[i] = input[i].swizzle(permutation);
	}
}

std::vector<FunctionInput> removeSuperInputs(const std::vector<FunctionInput>& layer, const std::vector<FunctionInput>& inputSet) {
	std::vector<FunctionInput> result;
	for(FunctionInput f : layer) {
		for(FunctionInput i : inputSet) {
			if(isSubSet(i, f)) {
				goto skip;
			}
		}
		result.push_back(f);
		skip:;
	}
	return result;
}
