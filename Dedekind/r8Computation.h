#pragma once

#include <cstdint>

#include "booleanFunction.h"

template<unsigned int Variables>
uint64_t getNumberOfOwnedChildClasses(const Monotonic<Variables>& cur) {
	uint64_t total = 1;

	BooleanFunction<Variables> newBits = andnot(cur.succ().bf, cur.bf);

	newBits.forEachOne([&](size_t bit) {
		Monotonic<Variables> newMBF = cur;
		newMBF.add(bit);

		if(isUniqueExtention(newMBF, bit)) {
			total += getNumberOfOwnedChildClasses(newMBF);
		}
	});

	return total;
}

template<unsigned int Variables>
uint64_t getD() {
	return getNumberOfOwnedChildClasses(Monotonic<Variables>::getBot());
}


template<unsigned int Variables>
uint64_t getNumberOfCanonicalOwnedChildClasses(const Monotonic<Variables>& cur) {
	uint64_t total = 1;

	BooleanFunction<Variables> newBits = andnot(cur.succ().bf, cur.bf);

	newBits.forEachOne([&](size_t bit) {
		Monotonic<Variables> newMBF = cur;
		newMBF.add(bit);

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
	return getNumberOfCanonicalOwnedChildClasses(Monotonic<Variables>::getBot());
}

