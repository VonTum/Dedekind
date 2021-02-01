#pragma once

#include <algorithm>
#include <iostream>

#include "functionInput.h"
#include "bitSet.h"

#include "crossPlatformIntrinsics.h"

template<unsigned int Variables>
class FunctionInputBitSet {
	static_assert(Variables >= 1, "Cannot make 0 variable FunctionInputBitSet");

public:
	using Bits = BitSet<(size_t(1) << Variables)>;


	Bits bitset;

	FunctionInputBitSet() : bitset() {}
	FunctionInputBitSet(const Bits& bitset) : bitset(bitset) {}
	FunctionInputBitSet(const FunctionInputSet& inputSet) : bitset() {
		for(FunctionInput fi : inputSet) {
			bitset.set(fi.inputBits);
		}
	}

	template<unsigned int OtherVariables>
	FunctionInputBitSet(const FunctionInputBitSet<OtherVariables>& other) : bitset(other.bitset) {}

	template<unsigned int OtherVariables>
	FunctionInputBitSet& operator=(const FunctionInputBitSet<OtherVariables>& other) { this->bitset = other.bitset; }

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
		return FunctionInputBitSet(Bits::empty());
	}

	static constexpr FunctionInputBitSet full() {
		return FunctionInputBitSet(Bits::full());
	}

	struct VarMaskCache {
		Bits masks[Variables];

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
					for(size_t j = 0; j < Bits::BLOCK_COUNT; j++) {
						masks[var].data[j] = varPattern[var];
					}
				}
				for(unsigned int var = 6; var < Variables; var++) {
					size_t stepSize = size_t(1) << (var - 6);
					assert(stepSize > 0);
					for(size_t curIndex = 0; curIndex < Bits::BLOCK_COUNT; curIndex += stepSize * 2) {
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
	
	struct LayerMaskCache {
		Bits masks[Variables+1];

		LayerMaskCache() {
			for(Bits& b : masks) {
				b = Bits::empty();
			}

			for(unsigned int i = 0; i < (1 << Variables); i++) {
				unsigned int layer = popcnt32(i);
				masks[layer].set(i);
			}
		}
	};

	static inline const VarMaskCache varMaskCache = VarMaskCache();
	static inline const LayerMaskCache layerMaskCache = LayerMaskCache();

	static constexpr Bits varMask(unsigned int var) {
		assert(var < Variables);

		return FunctionInputBitSet<Variables>::varMaskCache.masks[var];
	}
	static constexpr Bits layerMask(unsigned int layer) {
		assert(layer < Variables + 1);

		return FunctionInputBitSet<Variables>::layerMaskCache.masks[layer];
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

		Bits originalMask = FunctionInputBitSet::varMask(original);

		// shift amount
		// assuming source < destination (moving a lower variable to a higher spot)
		// so for example, every source element will be 000s0000 -> 0d000000
		// + 1<<d - 1<<s

		if(original < destination) {
			unsigned int shift = (1 << destination) - (1 << original);

			bitset = ((bitset & originalMask) << shift) | andnot(bitset, originalMask);
		} else {
			unsigned int shift = (1 << original) - (1 << destination);

			bitset = ((bitset & originalMask) >> shift) | andnot(bitset, originalMask);
		}
	}

	void swap(unsigned int var1, unsigned int var2) {
		if(var1 > var2) std::swap(var1, var2);
		// var1 <= var2
		Bits mask1 = FunctionInputBitSet::varMask(var1);
		if constexpr(Variables == 7) {
			if(var2 != 6) {
				Bits mask2 = FunctionInputBitSet::varMask(var2);

				// andnot is a more efficient operation for SIMD than complement
				Bits stayingElems = andnot(bitset, andnot(mask1 | mask2, mask1 & mask2));

				unsigned int shift = (1 << var2) - (1 << var1);
				
				__m128i shiftReg = _mm_set1_epi64x(shift);

				__m128i shiftedLeft = _mm_sll_epi64(_mm_and_si128(bitset.data, _mm_andnot_si128(mask2.data, mask1.data)), shiftReg);
				__m128i shiftedRight = _mm_srl_epi64(_mm_and_si128(bitset.data, _mm_andnot_si128(mask1.data, mask2.data)), shiftReg);

				bitset.data = _mm_or_si128(shiftedRight, _mm_or_si128(shiftedLeft, stayingElems.data));
			} else {
				// mask2 is just the upper 64 bits of the register, can just shift
				
				unsigned int relshift = 1 << var1; // shift == 64 - relshift
				
				__m128i shiftReg = _mm_set1_epi64x(relshift);

				__m128i stayingElems = _mm_blend_epi32(_mm_and_si128(mask1.data, bitset.data), _mm_andnot_si128(mask1.data, bitset.data), 0b0011);
				
				__m128i shiftedLeft = _mm_srl_epi64(_mm_slli_si128(_mm_and_si128(mask1.data, bitset.data), 8), shiftReg);
				__m128i shiftedRight = _mm_sll_epi64(_mm_srli_si128(_mm_andnot_si128(mask1.data, bitset.data), 8), shiftReg);

				bitset.data = _mm_or_si128(shiftedRight, _mm_or_si128(shiftedLeft, stayingElems));
			}
		} else {
			Bits mask2 = FunctionInputBitSet::varMask(var2);
			Bits stayingElems = bitset & (~(mask1 | mask2) | mask1 & mask2);
			unsigned int shift = (1 << var2) - (1 << var1);
			bitset = ((bitset & andnot(mask1, mask2)) << shift) | ((bitset & andnot(mask2, mask1)) >> shift) | stayingElems;
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

	bool isLayer() const {
		for(unsigned int i = 0; i < Variables + 1; i++) {
			Bits everythingInLayer = this->bitset & FunctionInputBitSet<Variables>::layerMask(i);

			if(!everythingInLayer.isEmpty()) {
				Bits everythingButLayer = andnot(this->bitset, FunctionInputBitSet<Variables>::layerMask(i));
				return everythingButLayer.isEmpty();
			}
		}
		// empty fis can also be layer, layer is unknown
		return true;
	}

	FunctionInputBitSet getLayer(unsigned int layer) const {
		assert(layer < Variables + 1);
		return FunctionInputBitSet(this->bitset & layerMask(layer));
	}

	// returns a FIBS where all elements which have a '0' where there is no superset above them that is '1'
	FunctionInputBitSet prev() const {
		// remove a variable for every item and OR the results

		Bits forced = Bits::empty();

		for(unsigned int var = 0; var < Variables; var++) {
			Bits whereVarActive = this->bitset & varMask(var);
			Bits varRemoved = whereVarActive >> (1 << var);
			forced |= varRemoved;
		}

		return FunctionInputBitSet(forced);
	}

	// returns a FIBS where all elements which have a '1' where there is no subset below them that is '0'
	FunctionInputBitSet next() const {
		// add a variable to every item and AND the results

		Bits forced = Bits::full();

		for(unsigned int var = 0; var < Variables; var++) {
			Bits whereVarNotActive = (~this->bitset) & ~varMask(var);
			Bits varAdded = whereVarNotActive << (1 << var);
			forced &= ~varAdded; // anding by zeros is the forced spaces
		}

		return FunctionInputBitSet(forced);
	}

	// checks if the given FIBS represents a Monotonic Function, where higher layers are towards '0' and lower layers towards '1'
	bool isMonotonic() const {
		FunctionInputBitSet p = prev();
		FunctionInputBitSet n = next();
		n.bitset.set(0);
		return ((p | *this) == *this) && ((n & *this) == *this);
	}


	// takes a function of the form void(size_t bits)
	template<typename Func>
	void forEachOne(const Func& func) const {
		for(size_t i = 0; i < (size_t(1) << Variables); i++) {
			if(bitset.get(i) == true) {
				func(i);
			}
		}
	}

	// takes a function of the form void(size_t bits)
	template<typename Func>
	void forEachZero(const Func& func) const {
		for(size_t i = 0; i < (size_t(1) << Variables); i++) {
			if(bitset.get(i) == false) {
				func(i);
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
			Bits mask;
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
				Bits compareToMask = varMask(*g->associatedVariables);
				for(Group* otherGroup = groups; otherGroup < groupsEnd; otherGroup++) {
					if(otherGroup == g) continue;
					g->value = (copy.bitset & compareToMask & otherGroup->mask).count();
					Group* newGroups = groupsEnd;
					Group* newGroupsEnd = newGroups;
					for(unsigned int* varInGroup = g->associatedVariables + 1; varInGroup < g->associatedVariables + g->groupSize; ) {
						Bits vm = varMask(*varInGroup);
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
		unsigned int numberOfSize1Groups = 0;
		for(; groups + numberOfSize1Groups < groupsEnd; numberOfSize1Groups++) {
			if(groups[numberOfSize1Groups].groupSize != 1) break;
		}

		/*
			Possible splits for Variables==7:

			7
			1-6
			2-5
			1-1-5
			3-4
			1-2-4
			1-1-1-4
			1-3-3
			2-2-3
			1-1-2-3
			1-1-1-1-3
			1-2-2-2
			1-1-1-2-2
			1-1-1-1-1-2
			1-1-1-1-1-1-1
		*/


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

inline void serializeU32(uint32_t value, uint8_t*& outputBuf) {
	for(size_t i = 0; i < 4; i++) {
		*outputBuf++ = static_cast<uint8_t>(value >> (24 - i * 8));
	}
}

inline uint32_t deserializeU32(const uint8_t*& outputBuf) {
	uint32_t result = 0;
	for(size_t i = 0; i < 4; i++) {
		result |= static_cast<uint32_t>(*outputBuf++) << (24 - i * 8);
	}
	return result;
}

inline void serializeU64(uint64_t value, uint8_t*& outputBuf) {
	for(size_t i = 0; i < 8; i++) {
		*outputBuf++ = static_cast<uint8_t>(value >> (56 - i * 8));
	}
}

inline uint64_t deserializeU64(const uint8_t*& outputBuf) {
	uint64_t result = 0;
	for(size_t i = 0; i < 8; i++) {
		result |= static_cast<uint64_t>(*outputBuf++) << (56 - i * 8);
	}
	return result;
}

template<unsigned int Variables>
constexpr size_t getMBFSizeInBytes() {
	return ((1 << Variables) + 7) / 8;
}

// returns the end of the buffer
template<unsigned int Variables>
uint8_t* serializeMBFToBuf(const FunctionInputBitSet<Variables>& fibs, uint8_t* outputBuf) {
	typename FunctionInputBitSet<Variables>::Bits bs = fibs.bitset;
	if constexpr(Variables <= 3) {
		*outputBuf++ = bs.data;
	} else if constexpr(Variables <= 6) {
		for(size_t i = FunctionInputBitSet<Variables>::Bits::size(); i > 0; i-=8) {
			*outputBuf++ = static_cast<uint8_t>(bs.data >> (i - 8));
		}
	} else if constexpr(Variables == 7) {
		serializeU64(_mm_extract_epi64(bs.data, 1), outputBuf);
		serializeU64(_mm_extract_epi64(bs.data, 0), outputBuf);
	} else {
		for(size_t i = 0; i < bs.BLOCK_COUNT; i++) {
			serializeU64(bs.data[bs.BLOCK_COUNT - i - 1], outputBuf);
		}
	}
	return outputBuf;
}

// returns the end of the buffer
template<unsigned int Variables>
FunctionInputBitSet<Variables> deserializeMBFFromBuf(const uint8_t* inputBuf) {
	FunctionInputBitSet<Variables> result = FunctionInputBitSet<Variables>::empty();
	typename FunctionInputBitSet<Variables>::Bits& bs = result.bitset;
	if constexpr(Variables <= 3) {
		bs.data = *inputBuf;
	} else if constexpr(Variables <= 6) {
		for(size_t i = FunctionInputBitSet<Variables>::Bits::size(); i > 0; i -= 8) {
			bs.data |= static_cast<decltype(bs.data)>(*inputBuf++) << (i - 8);
		}
	} else if constexpr(Variables == 7) {
		uint64_t part1 = deserializeU64(inputBuf);
		uint64_t part0 = deserializeU64(inputBuf);
		bs.data = _mm_set_epi64x(part1, part0);
	} else {
		for(size_t i = 0; i < bs.BLOCK_COUNT; i++) {
			bs.data[bs.BLOCK_COUNT - i - 1] = deserializeU64(inputBuf);
		}
	}
	return result;
}

template<unsigned int Variables>
void serializeMBF(const FunctionInputBitSet<Variables>& mbf, std::ostream& os) {
	uint8_t fibsBuf[getMBFSizeInBytes<Variables>()];

	serializeMBFToBuf(mbf, fibsBuf);

	os.write(reinterpret_cast<const char*>(fibsBuf), getMBFSizeInBytes<Variables>());
}
template<unsigned int Variables>
FunctionInputBitSet<Variables> deserializeMBF(std::istream& is) {
	uint8_t fibsBuf[getMBFSizeInBytes<Variables>()];

	is.read(reinterpret_cast<char*>(fibsBuf), getMBFSizeInBytes<Variables>());

	return deserializeMBFFromBuf<Variables>(fibsBuf);
}
