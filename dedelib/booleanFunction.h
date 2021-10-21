#pragma once

#include <algorithm>
#include <iostream>
#include <array>

#include "functionInput.h"
#include "functionInputSet.h"
#include "bitSet.h"

#include "crossPlatformIntrinsics.h"
#include "knownData.h"

template<unsigned int Variables>
class BooleanFunction;

// todo Optimization
/*template<unsigned int V1, unsigned int V2>
inline __m128i swapConstExpr(__m128i data) {
	_mm_bslli_si128()
}

template<typename Func>
inline void forEachSubPermFor7From3(const BooleanFunction<7>& bf, const Func& funcToRun) {
	// a-b-c-d
	funcToRun(bf);

	__m128i abcd = bf.bitset.data;

	// swap 3-4
	__m128i bacd = 
	// b-a-c-d

	
	// swap 4-5
	// b-c-a-d

	// swap 3-4
	// b-c-d-a

	// swap 3-4

	// swap 3-4

}*/

template<unsigned int Variables, unsigned int Current, typename Func>
void forEachPermutationImpl(BooleanFunction<Variables> cur, const Func& func) {
	if constexpr(Current == Variables - 1) {
		func(cur);
	} else if constexpr(Current == Variables - 2) {
		func(cur); // 1 2
		cur.swap(Current, Current + 1);
		func(cur); // 2 1
	} else if constexpr(Current == Variables - 3) {
		func(cur); // 1 2 3
		cur.swap(Current, Current + 1);
		func(cur); // 2 1 3
		cur.swap(Current, Current + 2);
		func(cur); // 3 1 2
		cur.swap(Current, Current + 1);
		func(cur); // 1 3 2
		cur.swap(Current, Current + 2);
		func(cur); // 2 3 1
		cur.swap(Current, Current + 1);
		func(cur); // 3 2 1
	} else {
		forEachPermutationImpl<Variables, Current + 1, Func>(cur, func);
		for(unsigned int swapping = Current + 1; swapping < Variables; swapping++) {
			cur.swap(Current, swapping);
			forEachPermutationImpl<Variables, Current + 1, Func>(cur, func);
		}
	}
}

template<unsigned int Variables>
class BooleanFunction {
	static_assert(Variables >= 1, "Cannot make 0 variable BooleanFunction");

public:
	using Bits = BitSet<(size_t(1) << Variables)>;


	Bits bitset;

	BooleanFunction() : bitset() {}
	BooleanFunction(const Bits& bitset) : bitset(bitset) {}
	BooleanFunction(const FunctionInputSet& inputSet) : bitset() {
		for(FunctionInput fi : inputSet) {
			bitset.set(fi.inputBits);
		}
	}

	template<unsigned int OtherVariables>
	BooleanFunction(const BooleanFunction<OtherVariables>& other) : bitset(other.bitset) {}

	template<unsigned int OtherVariables>
	BooleanFunction& operator=(const BooleanFunction<OtherVariables>& other) { this->bitset = other.bitset; }

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
		//assert(bitset.get(index) == false); // assert bit was off
		return bitset.set(index);
	}

	constexpr void remove(FunctionInput::underlyingType index) {
		//assert(bitset.get(index) == true); // assert bit was on
		return bitset.reset(index);
	}

	constexpr BooleanFunction& operator|=(const BooleanFunction& other) {
		this->bitset |= other.bitset;
		return *this;
	}
	constexpr BooleanFunction& operator&=(const BooleanFunction& other) {
		this->bitset &= other.bitset;
		return *this;
	}
	constexpr BooleanFunction& operator^=(const BooleanFunction& other) {
		this->bitset ^= other.bitset;
		return *this;
	}
	constexpr BooleanFunction operator~() const {
		return BooleanFunction(~this->bitset);
	}
	constexpr BooleanFunction& operator<<=(unsigned int shift) {
		this->bitset <<= shift;
		return *this;
	}
	constexpr BooleanFunction& operator>>=(unsigned int shift) {
		this->bitset >>= shift;
		return *this;
	}
	bool operator==(const BooleanFunction& other) const {
		return this->bitset == other.bitset;
	}
	bool operator!=(const BooleanFunction& other) const {
		return this->bitset != other.bitset;
	}

	static constexpr BooleanFunction empty() {
		return BooleanFunction(Bits::empty());
	}

	static constexpr BooleanFunction full() {
		return BooleanFunction(Bits::full());
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

		return BooleanFunction::varMaskCache.masks[var];
	}
	static constexpr Bits multiVarMask(unsigned int mask) {
		assert(mask < (1 << Variables));

		Bits totalMask = Bits::empty();

		for(unsigned int var = 0; var < Variables; var++) {
			if(mask & (1 << var)) {
				totalMask |= varMask(var);
			}
		}

		return totalMask;
	}
	static constexpr Bits layerMask(unsigned int layer) {
		assert(layer < Variables + 1);

		return BooleanFunction::layerMaskCache.masks[layer];
	}

	size_t countVariableOccurences(unsigned int var) const {
		return (this->bitset & BooleanFunction::varMask(var)).count();
	}

	bool isEmpty() const {
		return this->bitset.isEmpty();
	}
	bool isFull() const {
		return this->bitset.isFull();
	}

	bool hasVariable(unsigned int var) const {
		BooleanFunction mask = BooleanFunction::varMask(var);

		BooleanFunction checkVar = *this & mask;

		return !checkVar.isEmpty();
	}

	unsigned int getUniverse() const {
		unsigned int result = 0;
		for(unsigned int var = 0; var < Variables; var++) {
			if(this->hasVariable(var)) {
				result |= (1 << var);
			}
		}
		return result;
	}

	bool isSubSetOf(const BooleanFunction& other) const {
		return isSubSet(this->bitset, other.bitset);
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

		Bits originalMask = BooleanFunction::varMask(original);

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
		Bits mask1 = BooleanFunction::varMask(var1);
		if constexpr(Variables == 7) {
			if(var2 != 6) {
				Bits mask2 = BooleanFunction::varMask(var2);

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

				__m128i stayingElems = _mm_blend_epi16(_mm_and_si128(mask1.data, bitset.data), _mm_andnot_si128(mask1.data, bitset.data), 0b00001111);
				
				__m128i shiftedLeft = _mm_srl_epi64(_mm_slli_si128(_mm_and_si128(mask1.data, bitset.data), 8), shiftReg);
				__m128i shiftedRight = _mm_sll_epi64(_mm_srli_si128(_mm_andnot_si128(mask1.data, bitset.data), 8), shiftReg);

				bitset.data = _mm_or_si128(shiftedRight, _mm_or_si128(shiftedLeft, stayingElems));
			}
		} else {
			Bits mask2 = BooleanFunction::varMask(var2);
			Bits stayingElems = bitset & (~(mask1 | mask2) | (mask1 & mask2));
			unsigned int shift = (1 << var2) - (1 << var1);
			bitset = ((bitset & andnot(mask1, mask2)) << shift) | ((bitset & andnot(mask2, mask1)) >> shift) | stayingElems;
		}
	}

	BooleanFunction moved(unsigned int original, unsigned int destination) const {
		BooleanFunction copy = *this;
		copy.move(original, destination);
		return copy;
	}
	BooleanFunction swapped(unsigned int var1, unsigned int var2) const {
		BooleanFunction copy = *this;
		copy.swap(var1, var2);
		return copy;
	}

	template<typename Func>
	void forEachPermutation(const Func& func) const {
		forEachPermutationImpl<Variables, 0, Func>(*this, func);
	}

	template<size_t Size, typename Func, unsigned int CurVar = 0>
	static void forEachCoPermutation(std::array<BooleanFunction, Size>& cur, const Func& func) {
		if constexpr(CurVar == Variables) {
			func(cur);
		} else {
			forEachCoPermutation<Size, Func, CurVar + 1>(cur, func);
			for(unsigned int swapping = CurVar + 1; swapping < Variables; swapping++) {
				for(BooleanFunction& item : cur) {
					item.swap(CurVar, swapping);
				}
				forEachCoPermutation<Size, Func, CurVar + 1>(cur, func);
				for(BooleanFunction& item : cur) {
					item.swap(CurVar, swapping);
				}
			}
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
			Bits everythingInLayer = this->bitset & BooleanFunction::layerMask(i);

			if(!everythingInLayer.isEmpty()) {
				Bits everythingButLayer = andnot(this->bitset, BooleanFunction::layerMask(i));
				return everythingButLayer.isEmpty();
			}
		}
		// empty fis can also be layer, layer is unknown
		return true;
	}

	BooleanFunction getLayer(unsigned int layer) const {
		assert(layer < Variables + 1);
		return BooleanFunction(this->bitset & layerMask(layer));
	}

	BooleanFunction getTopLayer() const {
		if(this->isEmpty()) {
			return BooleanFunction::empty();
		}
		for(int l = Variables;; l--) {
			BooleanFunction subLayer = this->getLayer(l);
			if(!subLayer.isEmpty()) return subLayer;
		}
		assert(false);
	}

	BooleanFunction orDown() const {
		// remove a variable for every item and OR the results

		if constexpr(Variables == 7) {
			__m128i bs = this->bitset.data;

			__m128i forced = _mm_setzero_si128();

			__m128i v0Removed = _mm_srli_epi64(_mm_and_si128(bs, _mm_set1_epi8(0b10101010)), 1);
			forced = _mm_or_si128(forced, v0Removed);
			__m128i v1Removed = _mm_srli_epi64(_mm_and_si128(bs, _mm_set1_epi8(0b11001100)), 2);
			forced = _mm_or_si128(forced, v1Removed);
			__m128i v2Removed = _mm_srli_epi64(_mm_and_si128(bs, _mm_set1_epi8(0b11110000)), 4);
			forced = _mm_or_si128(forced, v2Removed);
			__m128i v3Removed = _mm_srli_epi16(bs, 8);
			forced = _mm_or_si128(forced, v3Removed);
			__m128i v4Removed = _mm_srli_epi32(bs, 16);
			forced = _mm_or_si128(forced, v4Removed);
			__m128i v5Removed = _mm_srli_epi64(bs, 32);
			forced = _mm_or_si128(forced, v5Removed);
			__m128i v6Removed = _mm_srli_si128(bs, 8);
			forced = _mm_or_si128(forced, v6Removed);

			BooleanFunction result;
			result.bitset.data = forced;
			return result;
		} else {
			Bits forced = Bits::empty();
			for(unsigned int var = 0; var < Variables; var++) {
				Bits whereVarActive = this->bitset & varMask(var);
				Bits varRemoved = whereVarActive >> (1 << var);
				forced |= varRemoved;
			}
			return BooleanFunction(forced);
		}
	}

	BooleanFunction orUp() const {
		// remove a variable for every item and OR the results

		if constexpr(Variables == 7) {
			__m128i bs = this->bitset.data;

			__m128i forced = _mm_setzero_si128();

			__m128i v0Added = _mm_slli_epi64(_mm_andnot_si128(_mm_set1_epi8(0b10101010), bs), 1);
			forced = _mm_or_si128(v0Added, forced);
			__m128i v1Added = _mm_slli_epi64(_mm_andnot_si128(_mm_set1_epi8(0b11001100), bs), 2);
			forced = _mm_or_si128(v1Added, forced);
			__m128i v2Added = _mm_slli_epi64(_mm_andnot_si128(_mm_set1_epi8(0b11110000), bs), 4);
			forced = _mm_or_si128(v2Added, forced);
			__m128i v3Added = _mm_slli_epi16(bs, 8);
			forced = _mm_or_si128(v3Added, forced);
			__m128i v4Added = _mm_slli_epi32(bs, 16);
			forced = _mm_or_si128(v4Added, forced);
			__m128i v5Added = _mm_slli_epi64(bs, 32);
			forced = _mm_or_si128(v5Added, forced);
			__m128i v6Added = _mm_slli_si128(bs, 8);
			forced = _mm_or_si128(v6Added, forced);

			BooleanFunction result;
			result.bitset.data = forced;
			return result;
		} else {
			Bits forced = Bits::empty();

			Bits bs = this->bitset;

			for(unsigned int var = 0; var < Variables; var++) {
				Bits whereVarNotActive = (bs & ~varMask(var));
				Bits varAdded = whereVarNotActive << (1 << var);
				forced |= varAdded; // orring by 1s is adding 1s
			}

			return BooleanFunction(forced);
		}
	}

	BooleanFunction andDown() const {
		// remove a variable for every item and OR the results

		if constexpr(Variables == 7) {
			__m128i bs = (~this->bitset).data;

			__m128i forced = _mm_cmpeq_epi16(_mm_setzero_si128(), _mm_setzero_si128()); // all 1s

			__m128i v0Removed = _mm_srli_epi64(_mm_and_si128(bs, _mm_set1_epi8(0b10101010)), 1);
			forced = _mm_andnot_si128(forced, v0Removed);
			__m128i v1Removed = _mm_srli_epi64(_mm_and_si128(bs, _mm_set1_epi8(0b11001100)), 2);
			forced = _mm_andnot_si128(forced, v1Removed);
			__m128i v2Removed = _mm_srli_epi64(_mm_and_si128(bs, _mm_set1_epi8(0b11110000)), 4);
			forced = _mm_andnot_si128(forced, v2Removed);
			__m128i v3Removed = _mm_srli_epi16(bs, 8);
			forced = _mm_andnot_si128(forced, v3Removed);
			__m128i v4Removed = _mm_srli_epi32(bs, 16);
			forced = _mm_andnot_si128(forced, v4Removed);
			__m128i v5Removed = _mm_srli_epi64(bs, 32);
			forced = _mm_andnot_si128(forced, v5Removed);
			__m128i v6Removed = _mm_srli_si128(bs, 8);
			forced = _mm_andnot_si128(forced, v6Removed);

			BooleanFunction result;
			result.bitset.data = forced;
			return result;
		} else {
			Bits bs = ~this->bitset;

			Bits forced = Bits::full();

			for(unsigned int var = 0; var < Variables; var++) {
				Bits whereVarActive = bs & varMask(var);
				Bits varRemoved = whereVarActive >> (1 << var);
				forced &= ~varRemoved;
			}
			return BooleanFunction(forced);
		}
	}

	// returns a FIBS where all elements which have a '1' where there is no subset below them that is '0'
	BooleanFunction andUp() const {
		if constexpr(Variables == 7) {
			__m128i bs = (~this->bitset).data;

			__m128i forced = _mm_cmpeq_epi16(_mm_setzero_si128(), _mm_setzero_si128()); // all 1s

			__m128i v0Added = _mm_slli_epi64(_mm_andnot_si128(_mm_set1_epi8(0b10101010), bs), 1);
			forced = _mm_andnot_si128(v0Added, forced);
			__m128i v1Added = _mm_slli_epi64(_mm_andnot_si128(_mm_set1_epi8(0b11001100), bs), 2);
			forced = _mm_andnot_si128(v1Added, forced);
			__m128i v2Added = _mm_slli_epi64(_mm_andnot_si128(_mm_set1_epi8(0b11110000), bs), 4);
			forced = _mm_andnot_si128(v2Added, forced);
			__m128i v3Added = _mm_slli_epi16(bs, 8);
			forced = _mm_andnot_si128(v3Added, forced);
			__m128i v4Added = _mm_slli_epi32(bs, 16);
			forced = _mm_andnot_si128(v4Added, forced);
			__m128i v5Added = _mm_slli_epi64(bs, 32);
			forced = _mm_andnot_si128(v5Added, forced);
			__m128i v6Added = _mm_slli_si128(bs, 8);
			forced = _mm_andnot_si128(v6Added, forced);

			BooleanFunction result;
			result.bitset.data = forced;
			return result;
		} else {
			Bits bs = ~this->bitset;

			Bits forced = Bits::full();

			for(unsigned int var = 0; var < Variables; var++) {
				Bits whereVarNotActive = bs & ~varMask(var);
				Bits varAdded = whereVarNotActive << (1 << var);
				forced &= ~varAdded; // anding by zeros is the forced spaces
			}

			return BooleanFunction(forced);
		}
	}

	// returns a FIBS where all elements which have a '0' where there is no superset above them that is '1'
	BooleanFunction pred() const {
		return this->orDown();
	}

	// returns a FIBS where all elements which have a '1' where there is no subset below them that is '0'
	BooleanFunction succ() const {
		return this->andUp();
	}

	BooleanFunction reverse() const {
		return BooleanFunction(this->bitset.reverse());
	}

	BooleanFunction dual() const {
		return BooleanFunction(~this->bitset.reverse());
	}

	// checks if the given FIBS represents a Monotonic Function, where higher layers are towards '0' and lower layers towards '1'
	bool isAntiChain() const {
		BooleanFunction p = pred();
		//n.bitset.set(0);
		return (*this & p.monotonizeDown()).isEmpty();
	}

	// checks if the given FIBS represents a Monotonic Function, where higher layers are towards '0' and lower layers towards '1'
	bool isMonotonic() const {
		BooleanFunction p = pred();
		BooleanFunction n = succ();
		//n.bitset.set(0);
		return ((p | *this) == *this) && ((n & *this) == *this);
	}

	// returns a new Monotonic BooleanFunction, with added 1s where needed
	// this->isSubSetOf(result)
	BooleanFunction monotonizeDown() const {
		if constexpr(Variables == 7) {
			__m128i bs = this->bitset.data;

			__m128i v0Removed = _mm_srli_epi64(_mm_and_si128(bs, _mm_set1_epi8(0b10101010)), 1);
			bs = _mm_or_si128(bs, v0Removed);
			__m128i v1Removed = _mm_srli_epi64(_mm_and_si128(bs, _mm_set1_epi8(0b11001100)), 2);
			bs = _mm_or_si128(bs, v1Removed);
			__m128i v2Removed = _mm_srli_epi64(_mm_and_si128(bs, _mm_set1_epi8(0b11110000)), 4);
			bs = _mm_or_si128(bs, v2Removed);
			__m128i v3Removed = _mm_srli_epi16(bs, 8);
			bs = _mm_or_si128(bs, v3Removed);
			__m128i v4Removed = _mm_srli_epi32(bs, 16);
			bs = _mm_or_si128(bs, v4Removed);
			__m128i v5Removed = _mm_srli_epi64(bs, 32);
			bs = _mm_or_si128(bs, v5Removed);
			__m128i v6Removed = _mm_srli_si128(bs, 8);
			bs = _mm_or_si128(bs, v6Removed);

			BooleanFunction result;
			result.bitset.data = bs;
			assert(result.isMonotonic());
			return result;
		} else {
			Bits resultbits = this->bitset;

			for(unsigned int var = 0; var < Variables; var++) {
				Bits whereVarActive = resultbits & varMask(var);
				Bits varRemoved = whereVarActive >> (1 << var);
				resultbits |= varRemoved;
			}

			BooleanFunction result(resultbits);
			assert(result.isMonotonic());
			return result;
		}
	}

	// returns a new Monotonic BooleanFunction, with added 1s where needed
	// this->isSubSetOf(result)
	BooleanFunction monotonizeUp() const {
		if constexpr(Variables == 7) {
			__m128i bs = this->bitset.data;

			__m128i v0Added = _mm_slli_epi64(_mm_andnot_si128(_mm_set1_epi8(0b10101010), bs), 1);
			bs = _mm_or_si128(bs, v0Added);
			__m128i v1Added = _mm_slli_epi64(_mm_andnot_si128(_mm_set1_epi8(0b11001100), bs), 2);
			bs = _mm_or_si128(bs, v1Added);
			__m128i v2Added = _mm_slli_epi64(_mm_andnot_si128(_mm_set1_epi8(0b11110000), bs), 4);
			bs = _mm_or_si128(bs, v2Added);
			__m128i v3Added = _mm_slli_epi16(bs, 8);
			bs = _mm_or_si128(bs, v3Added);
			__m128i v4Added = _mm_slli_epi32(bs, 16);
			bs = _mm_or_si128(bs, v4Added);
			__m128i v5Added = _mm_slli_epi64(bs, 32);
			bs = _mm_or_si128(bs, v5Added);
			__m128i v6Added = _mm_slli_si128(bs, 8);
			bs = _mm_or_si128(bs, v6Added);

			BooleanFunction result;
			result.bitset.data = bs;
			assert((~result).isMonotonic());
			return result;
		} else {
			Bits resultbits = this->bitset;

			for(unsigned int var = 0; var < Variables; var++) {
				Bits whereVarNotActive = andnot(resultbits, varMask(var));
				Bits varAdded = whereVarNotActive << (1 << var);
				resultbits |= varAdded; // anding by zeros is the forced spaces
			}

			BooleanFunction result(resultbits);
			assert((~result).isMonotonic());
			return result;
		}
	}

	// takes a function of the form void(size_t bits)
	template<typename Func>
	void forEachOne(const Func& func) const {
		this->bitset.forEachOne(func);
	}

	// takes a function of the form void(size_t bits)
	template<typename Func>
	void forEachZero(const Func& func) const {
		(~this->bitset).forEachOne(func);
	}

	BooleanFunction getNextBits() const {
		return BooleanFunction(andnot(this->succ().bitset, this->bitset));
	}
	BooleanFunction getNextBits(const BooleanFunction& mustBeSubSetOf) const {
		return BooleanFunction(andnot(this->succ().bitset, this->bitset) & mustBeSubSetOf.bitset);
	}

	BooleanFunction getPrevBits() const {
		return BooleanFunction(andnot(this->bitset, this->pred().bitset));
	}
	BooleanFunction getPrevBits(const BooleanFunction& mustBeSuperSetOf) const {
		return BooleanFunction(andnot(andnot(this->bitset, this->pred().bitset), mustBeSuperSetOf.bitset));
	}

	size_t getFirst() const {
		return bitset.getFirstOnBit();
	}

	size_t getLast() const {
		return bitset.getLastOnBit();
	}

	// takes a function of the form void(const BooleanFunction& expanded)
	template<typename Func>
	void forEachUpExpansion(const Func& func) const {
		Bits bitset = andnot(this->succ().bitset, this->bitset);

		bitset.forEachOne([&](size_t bit) {
			BooleanFunction expanded = *this;
			expanded.add(bit);
			func(expanded);
		});
	}
	// takes a function of the form void(const BooleanFunction& expanded)
	template<typename Func>
	void forEachUpExpansion(const BooleanFunction& mustBeSubSetOf, const Func& func) const {
		Bits bitset = andnot(this->succ().bitset, this->bitset) & mustBeSubSetOf.bitset;

		bitset.forEachOne([&](size_t bit) {
			BooleanFunction expanded = *this;
			expanded.add(bit);
			func(expanded);
		});
	}

	// takes a function of the form void(const BooleanFunction& subSet)
	template<typename Func>
	void forEachSubSet(const Func& func) const {
		this->bitset.forEachSubSet([&](const Bits& bits) {
			BooleanFunction subSet(bits);
			func(subSet);
		});
	}

	BooleanFunction asAntiChain() const {
		return BooleanFunction(andnot(this->bitset, this->monotonizeDown().pred().bitset));
	}

	std::array<unsigned int, Variables> countAllVarOccurences() const {
		std::array<unsigned int, Variables> result;
		for(unsigned int v = 0; v < Variables; v++) {
			result[v] = this->countVariableOccurences(v);
		}
		return result;
	}

	BooleanFunction canonize() const {
		if(this->bitset.isEmpty() || ((this->bitset.get(0) == 1) && (this->bitset.count() == 1))) return *this;

		BooleanFunction copy = *this;

		std::array<unsigned int, Variables> counts = copy.countAllVarOccurences();

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

			BooleanFunction best = copy;
			copy.forEachPermutationInGroups(numberOfSize1Groups, numberOfSize1Groups + groupSizeBuffer[numberOfSize1Groups], groupSizeBuffer + 1 + numberOfSize1Groups, groupSizeBuffer + groupCount, [&best](const BooleanFunction& permut) {
				if(permut.bitset < best.bitset) {
					best = permut;
				}
			});
			copy = best;
		}

		return copy;
	}


	BooleanFunction canonizePreserving(const BooleanFunction& referenceToMatch) const {
		if(this->bitset.isEmpty() || ((this->bitset.get(0) == 1) && (this->bitset.count() == 1))) return *this;

		std::array<BooleanFunction, 2> combo;
		combo[0] = referenceToMatch;
		combo[1] = *this;

		BooleanFunction best = *this;

		forEachCoPermutation(combo, [&](const std::array<BooleanFunction, 2>& permutation) {
			if(permutation[0] == referenceToMatch) {
				// only try if these match
				if(permutation[1].bitset < best.bitset) {
					best = permutation[1];
				}
			}
		});

		return best;
	}

	unsigned int countDuplicatePermutations() const {
		unsigned int totalEqual = 0;

		BooleanFunction copy = *this;
		copy.forEachPermutation([this, &totalEqual](const BooleanFunction& permuted) {
			if(permuted == *this) {
				totalEqual++;
			}
		});

		return totalEqual;
	}

	unsigned int countNonDuplicatePermutations() const {
		constexpr unsigned int totalPermutations = factorial(Variables);
		return totalPermutations / countDuplicatePermutations();
	}
};


template<unsigned int Variables>
BooleanFunction<Variables> operator|(BooleanFunction<Variables> result, const BooleanFunction<Variables>& b) {
	result |= b;
	return result;
}
template<unsigned int Variables>
BooleanFunction<Variables> operator&(BooleanFunction<Variables> result, const BooleanFunction<Variables>& b) {
	result &= b;
	return result;
}
template<unsigned int Variables>
BooleanFunction<Variables> operator^(BooleanFunction<Variables> result, const BooleanFunction<Variables>& b) {
	result ^= b;
	return result;
}
template<unsigned int Variables>
BooleanFunction<Variables> andnot(const BooleanFunction<Variables>& a, const BooleanFunction<Variables>& b) {
	return BooleanFunction<Variables>(andnot(a.bitset, b.bitset));
}
template<unsigned int Variables>
BooleanFunction<Variables> operator<<(BooleanFunction<Variables> result, unsigned int shift) {
	result <<= shift;
	return result;
}
template<unsigned int Variables>
BooleanFunction<Variables> operator>>(BooleanFunction<Variables> result, unsigned int shift) {
	result >>= shift;
	return result;
}

template<unsigned int Variables>
uint64_t getFormationCount(const BooleanFunction<Variables>& func, const BooleanFunction<Variables>& base) {
	uint64_t total = 1;
	for(unsigned int v = 0; v < Variables; v++) {
		uint32_t count = func.getLayer(v).size() - base.getLayer(v).size();
		if(count != 0) {
			total *= count; // for matching the factorial, 0! == 1, so take it to be 1 if it's 0
		}
	}
	return total;
}
template<unsigned int Variables>
uint64_t getFormationCountWithout(const BooleanFunction<Variables>& func, const BooleanFunction<Variables>& base, unsigned int skip) {
	uint64_t total = 1;
	for(unsigned int v = 0; v < Variables; v++) {
		if(v == skip) continue;
		uint32_t count = func.getLayer(v).size() - base.getLayer(v).size();
		if(count != 0) {
			total *= count; // for matching the factorial, 0! == 1, so take it to be 1 if it's 0
		}
	}
	return total;
}

template<unsigned int Variables>
unsigned int getModifiedLayer(const BooleanFunction<Variables>& a, const BooleanFunction<Variables>& b) {
	for(unsigned int l = 0; l <= Variables; l++) {
		if(a.getLayer(l).size() != b.getLayer(l).size()) return l;
	}
	throw "No difference";
}


template<unsigned int Variables, typename RandomEngine>
void permuteRandom(BooleanFunction<Variables>& bf, RandomEngine& generator) {
	for(unsigned int i = 0; i < Variables; i++) {
		unsigned int selectedIndex = std::uniform_int_distribution<unsigned int>(i, Variables - 1)(generator);
		if(selectedIndex == i) continue; // leave i where it is
		bf.swap(i, selectedIndex); // put selectedIndex in position i
	}
}

