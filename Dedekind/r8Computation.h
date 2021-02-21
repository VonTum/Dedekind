#pragma once

#include <cstdint>

#include "booleanFunction.h"

template<unsigned int Variables>
uint64_t getNumberOfOwnedChildClasses(const BooleanFunction<Variables>& cur) {
	uint64_t total = 1;

	BooleanFunction<Variables> newBits = andnot(cur.next(), cur);

	newBits.forEachOne([&](size_t bit) {
		BooleanFunction<Variables> newMBF = cur;
		newMBF.add(FunctionInput::underlyingType(bit));

		if(isUniqueExtention(newMBF, bit)) {
			total += getNumberOfOwnedChildClasses(newMBF);
		}
	});

	return total;
}

template<unsigned int Variables>
uint64_t getD() {
	return getNumberOfOwnedChildClasses(BooleanFunction<Variables>::empty());
}


template<unsigned int Variables>
uint64_t getNumberOfCanonicalOwnedChildClasses(const BooleanFunction<Variables>& cur) {
	uint64_t total = 1;

	BooleanFunction<Variables> newBits = andnot(cur.next(), cur);

	newBits.forEachOne([&](size_t bit) {
		BooleanFunction<Variables> newMBF = cur;
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
	return getNumberOfCanonicalOwnedChildClasses(BooleanFunction<Variables>::empty());
}

