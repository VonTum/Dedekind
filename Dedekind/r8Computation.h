#pragma once

#include <cstdint>

#include "functionInputBitSet.h"

template<unsigned int Variables>
bool isUniqueExtention(const FunctionInputBitSet<Variables>& bs, size_t bit) {
	BitSet<(1 << Variables)> possibleBits(andnot(bs.bitset, bs.prev().bitset));

	return possibleBits.getFirstOnBit() == bit;
}

template<unsigned int Variables>
uint64_t getNumberOfOwnedChildClasses(const FunctionInputBitSet<Variables>& cur) {
	uint64_t total = 1;

	FunctionInputBitSet<Variables> newBits(andnot(cur.next().bitset, cur.bitset));

	newBits.forEachOne([&](size_t bit) {
		FunctionInputBitSet<Variables> newMBF = cur;
		newMBF.add(FunctionInput::underlyingType(bit));

		if(isUniqueExtention(newMBF, bit)) {
			total += getNumberOfOwnedChildClasses(newMBF);
		}
	});

	return total;
}

template<unsigned int Variables>
uint64_t getD() {
	return getNumberOfOwnedChildClasses(FunctionInputBitSet<Variables>::empty());
}


template<unsigned int Variables>
uint64_t getNumberOfCanonicalOwnedChildClasses(const FunctionInputBitSet<Variables>& cur) {
	uint64_t total = 1;

	FunctionInputBitSet<Variables> newBits(andnot(cur.next().bitset, cur.bitset));

	newBits.forEachOne([&](size_t bit) {
		FunctionInputBitSet<Variables> newMBF = cur;
		newMBF.add(FunctionInput::underlyingType(bit));

		// incorrect still work to do
		if(newMBF == newMBF.canonize()) {
			if(isUniqueExtention(newMBF, bit)) {
				total += getNumberOfCanonicalOwnedChildClasses(newMBF);
			}
		}
	});

	return total;
}

template<unsigned int Variables>
uint64_t getR() {
	return getNumberOfCanonicalOwnedChildClasses(FunctionInputBitSet<Variables>::empty());
}
