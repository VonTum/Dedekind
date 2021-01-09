#pragma once

#include <algorithm>
#include <iostream>

#include "functionInput.h"
#include "bitSet.h"

template<unsigned int Variables>
class FunctionInputBitSet {
	static_assert(Variables >= 1, "Cannot make 0 variable FunctionInputBitSet");

public:
	BitSet<size_t(1) << Variables> bitset;

	FunctionInputBitSet() : bitset() {}
	FunctionInputBitSet(const BitSet<size_t(1) << Variables>& bitset) : bitset(bitset) {}
	FunctionInputBitSet(const FunctionInputSet& inputSet) : bitset() {
		for(FunctionInput fi : inputSet) {
			bitset.set(fi.inputBits);
		}
	}

	static constexpr FunctionInput::underlyingType maxSize() {
		return FunctionInput::underlyingType(1) << Variables;
	}

	FunctionInput::underlyingType size() const {
		return bitset.count();
	}
	uint64_t hash() const {
		return this->bitset.hash();
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
		return FunctionInputBitSet(BitSet<size_t(1) << Variables>::empty());
	}

	static constexpr FunctionInputBitSet full() {
		return FunctionInputBitSet(BitSet<size_t(1) << Variables>::full());
	}

	struct VarMaskCache {
		BitSet<(1 << Variables)> masks[Variables];

		constexpr VarMaskCache() {
			constexpr uint64_t varPattern[6]{0xaaaaaaaaaaaaaaaa, 0xcccccccccccccccc, 0xF0F0F0F0F0F0F0F0, 0xFF00FF00FF00FF00, 0xFFFF0000FFFF0000, 0xFFFFFFFF00000000};
			
			if constexpr(Variables <= 6) {
				for(unsigned int var = 0; var < Variables; var++) {
					masks[var].data = static_cast<decltype(masks[var].data)>(varPattern[var]);
				}
			} else if constexpr(Variables == 7) {
				for(unsigned int var = 0; var < 6; var++) {
					masks[var].data = _mm_set1_epi64x(varPattern[var]);
				}
				masks[6].data = _mm_set_epi64x(0xFFFFFFFFFFFFFFFF, 0x0000000000000000);
			} else {
				for(unsigned int var = 0; var < 6; var ++) {
					for(size_t j = 0; j < BitSet<(1 << Variables)>::BLOCK_COUNT; j++) {
						masks[var].data[j] = varPattern[var];
					}
				}
				for(unsigned int var = 6; var < Variables; var++) {
					size_t stepSize = size_t(1) << (var - 6);
					assert(stepSize > 0);
					for(size_t curIndex = 0; curIndex < BitSet<(1 << Variables)>::BLOCK_COUNT; curIndex += stepSize * 2) {
						for(size_t indexInStep = 0; indexInStep < stepSize; indexInStep++) {
							masks[var].data[curIndex + indexInStep] = 0x0000000000000000;
						}
						for(size_t indexInStep = 0; indexInStep < stepSize; indexInStep++) {
							masks[var].data[curIndex + stepSize + indexInStep] = 0xFFFFFFFFFFFFFFFF;
						}
					}
				}
			}
		}
	};
	static inline const VarMaskCache varMaskCache = VarMaskCache();

	static constexpr BitSet<(1 << Variables)> varMask(unsigned int var) {
		assert(var < Variables);

		return FunctionInputBitSet<Variables>::varMaskCache.masks[var];
	}

	size_t countVariableOccurences(unsigned int var) const {
		return (this->bitset & FunctionInputBitSet::varMask(var)).count();
	}

	bool isEmpty() const {
		return this->bitset.isEmpty();
	}
	bool isFull() const {
		return this->bitset.isFull();
	}

	bool hasVariable(unsigned int var) const {
		FunctionInputBitSet mask = FunctionInputBitSet::varMask(var);

		FunctionInputBitSet checkVar = *this & mask;

		return !checkVar.isEmpty();
	}

	unsigned int span() const {
		unsigned int result = 0;
		for(unsigned int var = 0; var < Variables; var++) {
			if(hasVariable(var)) {
				result |= 1 << var;
			}
		}
		assert(result < 128);
		return result;
	}

	void move(unsigned int original, unsigned int destination) {
		assert(!this->hasVariable(destination));

		BitSet<(1 << Variables)> originalMask = FunctionInputBitSet::varMask(original);

		// shift amount
		// assuming source < destination (moving a lower variable to a higher spot)
		// so for example, every source element will be 000s0000 -> 0d000000
		// + 1<<d - 1<<s

		if(original < destination) {
			unsigned int shift = (1 << destination) - (1 << original);

			bitset = ((bitset & originalMask) << shift) | (bitset & ~originalMask);
		} else {
			unsigned int shift = (1 << original) - (1 << destination);

			bitset = ((bitset & originalMask) >> shift) | (bitset & ~originalMask);
		}
	}

	void swap(unsigned int var1, unsigned int var2) {
		BitSet<(1 << Variables)> mask1 = FunctionInputBitSet::varMask(var1);
		BitSet<(1 << Variables)> mask2 = FunctionInputBitSet::varMask(var2);

		BitSet<(1 << Variables)> stayingElems = bitset & (~mask1 & ~mask2 | mask1 & mask2);

		if(var1 < var2) {
			unsigned int shift = (1 << var2) - (1 << var1);

			bitset = ((bitset & mask1 & ~mask2) << shift) | ((bitset & mask2 & ~mask1) >> shift) | stayingElems;
		} else {
			unsigned int shift = (1 << var1) - (1 << var2);

			bitset = ((bitset & mask2 & ~mask1) << shift) | ((bitset & mask1 & ~mask2) >> shift) | stayingElems;
		}
	}


	template<typename Func>
	void forEachPermutation(unsigned int fromVar, unsigned int toVar, const Func& func) {
		if(fromVar == toVar) {
			func(*this);
		} else {
			this->forEachPermutation(fromVar + 1, toVar, func);
			for(unsigned int swapping = fromVar + 1; swapping < toVar; swapping++) {
				this->swap(fromVar, swapping);
				this->forEachPermutation(fromVar + 1, toVar, func);
				this->swap(fromVar, swapping);
			}
		}
	}

	template<typename Func>
	void forEachPermutationInGroups(unsigned int fromVar, unsigned int toVar, unsigned int* nextGroup, unsigned int* groupsEnd, const Func& func) {
		assert(fromVar <= toVar && toVar <= Variables);
		if(fromVar == toVar) {
			if(nextGroup == groupsEnd) {
				func(*this);
			} else {
				forEachPermutationInGroups(toVar, toVar + *nextGroup, nextGroup + 1, groupsEnd, func);
			}
		} else {
			this->forEachPermutationInGroups(fromVar + 1, toVar, nextGroup, groupsEnd, func);
			for(unsigned int swapping = fromVar + 1; swapping < toVar; swapping++) {
				this->swap(fromVar, swapping);
				this->forEachPermutationInGroups(fromVar + 1, toVar, nextGroup, groupsEnd, func);
				this->swap(fromVar, swapping);
			}
		}
	}

	FunctionInputBitSet canonize() const {
		if(this->bitset.isEmpty() || this->bitset.get(0) == 1 && this->bitset.count() == 1) return *this;

		FunctionInputBitSet copy = *this;

		unsigned int counts[Variables];
		for(unsigned int v = 0; v < Variables; v++) {
			counts[v] = copy.countVariableOccurences(v);
		}

		// Fill all empty spots
		unsigned int varIdx = 0;
		for(; varIdx < Variables; varIdx++) {
			if(counts[varIdx] == 0) {
				for(unsigned int finderFromBack = Variables - 1; finderFromBack > varIdx; finderFromBack--) {
					if(counts[finderFromBack] != 0) {
						copy.move(finderFromBack, varIdx); // Fill in variables 0->#vars
						counts[varIdx] = counts[finderFromBack];
						counts[finderFromBack] = 0;
						goto nextVar;
					}
				}
				break; // all variables covered
			}
			nextVar:;
		}
		// varIdx is now number of variables

		// Assign groups
		struct Group {
			unsigned int value; // represents 'occurences' in the first half, and 'cooccurences' in the second. Just a temporary to sort by
			unsigned int groupSize;
			BitSet<(1 << Variables)> mask;
			unsigned int associatedVariables[Variables];
		} groups[Variables];
		Group* groupsEnd = groups;

		// assign groups for values
		for(unsigned int i = 0; i < varIdx; i++) {
			unsigned int occ = counts[i];
			for(Group* grp = groups; grp < groupsEnd; grp++) {
				if(grp->value == occ) {
					grp->associatedVariables[grp->groupSize] = i;
					grp->groupSize++;
					grp->mask |= varMask(i);
					goto groupFound; // group found
				}
			}
			// group not found, add new
			*groupsEnd = Group{occ, 1, varMask(i), {i}};
			groupsEnd++;
			groupFound:;
		}

		// unique sorting
		std::sort(groups, groupsEnd, [](const Group& a, const Group& b) {
			if(a.groupSize != b.groupSize) {
				return a.groupSize < b.groupSize;
			} else {
				return a.value < b.value;
			}
		});

		// all groups should be deterministic now
		
		Group* recheckUpTo = groups;
		Group* g = groups;
		do {
			// can't split group of size 1
			if(g->groupSize > 1) {
				BitSet<(1 << Variables)> compareToMask = varMask(*g->associatedVariables);
				for(Group* otherGroup = groups; otherGroup < groupsEnd; otherGroup++) {
					if(otherGroup == g) continue;
					g->value = (copy.bitset & compareToMask & otherGroup->mask).count();
					Group* newGroups = groupsEnd;
					Group* newGroupsEnd = newGroups;
					for(unsigned int* varInGroup = g->associatedVariables + 1; varInGroup < g->associatedVariables + g->groupSize; ) {
						BitSet<(1 << Variables)> vm = varMask(*varInGroup);
						unsigned int cooccurences = (copy.bitset & vm & otherGroup->mask).count();
						if(g->value != cooccurences) {
							// split off
							for(Group* newGroup = newGroups; newGroup < newGroupsEnd; newGroup++) {
								// add to new group
								if(newGroup->value == cooccurences) {
									// add to this group
									newGroup->associatedVariables[newGroup->groupSize] = *varInGroup;
									newGroup->groupSize++;
									newGroup->mask |= vm;

									goto newGroupFound;
								}
							}
							// create new group
							*newGroupsEnd = Group{cooccurences, 1, vm, {*varInGroup}};
							groupsEnd++;
							newGroupsEnd++;
							newGroupFound:;
							g->groupSize--;
							*varInGroup = g->associatedVariables[g->groupSize];

						} else {
							varInGroup++;
						}
					}

					if(newGroups != newGroupsEnd) {
						// a split has happened
						// recalculate mask
						g->mask = varMask(g->associatedVariables[0]);
						for(unsigned int* varInGroup = g->associatedVariables + 1; varInGroup < g->associatedVariables + g->groupSize; varInGroup++) {
							g->mask |= varMask(*varInGroup);
						}

						// include split group into sort, smallest cooccur will be new original group. Deterministic as all cooccur are unique
						Group* minimalCoOccurGroup = newGroups;
						for(Group* testGroup = newGroups + 1; testGroup < newGroupsEnd; testGroup++) {
							if(testGroup->value < minimalCoOccurGroup->value) {
								minimalCoOccurGroup = testGroup;
							}
						}
						if(minimalCoOccurGroup->value < g->value) {
							// swap out the original group
							std::swap(*g, *minimalCoOccurGroup);
						}

						// sort groups
						std::sort(newGroups, newGroupsEnd, [](const Group& a, const Group& b) {
							return a.value < b.value;
						});

						// all groups should be deterministic now

						recheckUpTo = g;
						break;
					}
				}
			}
			g++;
			if(g == groupsEnd) {
				g = groups;
			}
		} while(g != recheckUpTo);

		// all groups should be deterministic now

		std::stable_sort(groups, groupsEnd, [](const Group& a, const Group& b) {
			return a.groupSize < b.groupSize;
		});
		
		unsigned int newVariableOrder[Variables];
		unsigned int* newVariableOrderEnd = newVariableOrder;
		for(Group* grp = groups; grp < groupsEnd; grp++) {
			for(unsigned int* v = grp->associatedVariables; v < grp->associatedVariables + grp->groupSize; v++) {
				*newVariableOrderEnd = *v;
				newVariableOrderEnd++;
			}
		}

		for(unsigned int v = 0; v < varIdx; v++){
			// skip if already in place
			if(newVariableOrder[v] != v) {
				for(unsigned int foundV = v + 1; foundV < varIdx; foundV++) {
					if(newVariableOrder[foundV] == v) {
						copy.swap(v, newVariableOrder[v]);
						std::swap(newVariableOrder[v], newVariableOrder[foundV]);
						goto foundTheV;
					}
				}
				throw "unreachable";
				foundTheV:;
			}
		}
	
		unsigned int groupCount = groupsEnd - groups;
		/*unsigned int sortedTop = 0;
		for(unsigned int g = 0; g < groupCount; g++) {
			unsigned int occ = groups[g].value;
			for(unsigned int v = sortedTop; v < varIdx; v++) {
				if(counts[v] == occ) {
					// skip if already in place
					if(v != sortedTop) {
						std::swap(counts[v], counts[sortedTop]);
						copy.swap(v, sortedTop);
					}
					sortedTop++;
				}
			}
		}*/

		unsigned int numberOfSize1Groups = 0;
		for(; groups + numberOfSize1Groups < groupsEnd; numberOfSize1Groups++) {
			if(groups[numberOfSize1Groups].groupSize != 1) break;
		}

		if(numberOfSize1Groups < groupCount) {
			unsigned int groupSizeBuffer[Variables];
			for(unsigned int i = 0; i < groupCount; i++) groupSizeBuffer[i] = groups[i].groupSize;

			FunctionInputBitSet best = copy;
			copy.forEachPermutationInGroups(numberOfSize1Groups, numberOfSize1Groups + groupSizeBuffer[numberOfSize1Groups], groupSizeBuffer + 1 + numberOfSize1Groups, groupSizeBuffer + groupCount, [&best](const FunctionInputBitSet& permut) {
				if(permut.bitset < best.bitset) {
					best = permut;
				}
			});
			copy = best;
		}

		return copy;
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

