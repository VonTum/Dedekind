#pragma once

#include <cassert>
#include <cstdint>
#include <immintrin.h>

#include "crossPlatformIntrinsics.h"

inline constexpr uint64_t reverse64(uint64_t v) {
	v = (v << 32) | (v >> 32);
	constexpr uint64_t mask16 = 0xFFFF0000FFFF0000;
	v = ((v & mask16) >> 16) | ((v & ~mask16) << 16);
	constexpr uint64_t mask8 = 0xFF00FF00FF00FF00;
	v = ((v & mask8) >> 8) | ((v & ~mask8) << 8);
	constexpr uint64_t mask4 = 0xF0F0F0F0F0F0F0F0;
	v = ((v & mask4) >> 4) | ((v & ~mask4) << 4);
	constexpr uint64_t mask2 = 0xCCCCCCCCCCCCCCCC;
	v = ((v & mask2) >> 2) | ((v & ~mask2) << 2);
	constexpr uint64_t mask1 = 0xAAAAAAAAAAAAAAAA;
	v = ((v & mask1) >> 1) | ((v & ~mask1) << 1);
	return v;
}
inline constexpr uint32_t reverse32(uint32_t v) {
	v = (v << 16) | (v >> 16);
	constexpr uint32_t mask8 = 0xFF00FF00;
	v = ((v & mask8) >> 8) | ((v & ~mask8) << 8);
	constexpr uint32_t mask4 = 0xF0F0F0F0;
	v = ((v & mask4) >> 4) | ((v & ~mask4) << 4);
	constexpr uint32_t mask2 = 0xCCCCCCCC;
	v = ((v & mask2) >> 2) | ((v & ~mask2) << 2);
	constexpr uint32_t mask1 = 0xAAAAAAAA;
	v = ((v & mask1) >> 1) | ((v & ~mask1) << 1);
	return v;
}
inline constexpr uint16_t reverse16(uint16_t v) {
	v = (v << 8) | (v >> 8);
	constexpr uint16_t mask4 = 0xF0F0;
	v = ((v & mask4) >> 4) | ((v & ~mask4) << 4);
	constexpr uint16_t mask2 = 0xCCCC;
	v = ((v & mask2) >> 2) | ((v & ~mask2) << 2);
	constexpr uint16_t mask1 = 0xAAAA;
	v = ((v & mask1) >> 1) | ((v & ~mask1) << 1);
	return v;
}
inline constexpr uint8_t reverse8(uint8_t v) {
	v = (v << 4) | (v >> 4);
	constexpr uint8_t mask2 = 0xCC;
	v = ((v & mask2) >> 2) | ((v & ~mask2) << 2);
	constexpr uint8_t mask1 = 0xAA;
	v = ((v & mask1) >> 1) | ((v & ~mask1) << 1);
	return v;
}
inline constexpr uint8_t reverse4(uint8_t v) {
	v = (v << 2) | (v >> 2);
	constexpr uint8_t mask1 = 0xA;
	v = ((v & mask1) >> 1) | ((v & ~mask1) << 1);
	return v;
}
inline constexpr uint8_t reverse2(uint8_t v) {
	return (v << 1) | (v >> 1);
}

// kindly croudsourced from StackOverflow https://stackoverflow.com/questions/66091420/how-to-best-emulate-the-logical-meaning-of-mm-slli-si128-128-bit-bit-shift-n
template<int i>
inline __m128i slli_epi128(__m128i vec) {
	static_assert(i >= 0 && i < 128);
	// Handle couple trivial cases
	if constexpr(0 == i)
		return vec;
	if constexpr(0 == (i % 8))
		return _mm_slli_si128(vec, i / 8);

	if constexpr(i > 64) {
		// Shifting by more than 8 bytes, the lowest half will be all zeros
		vec = _mm_slli_si128(vec, 8);
		return _mm_slli_epi64(vec, i - 64);
	} else {
		// Shifting by less than 8 bytes.
		// Need to propagate a few bits across 64-bit lanes.
		__m128i low = _mm_slli_si128(vec, 8);
		__m128i high = _mm_slli_epi64(vec, i);
		low = _mm_srli_epi64(low, 64 - i);
		return _mm_or_si128(low, high);
	}
}

// kindly croudsourced from StackOverflow https://stackoverflow.com/questions/66091420/how-to-best-emulate-the-logical-meaning-of-mm-slli-si128-128-bit-bit-shift-n
template<int i>
inline __m128i srli_epi128(__m128i vec) {
	static_assert(i >= 0 && i < 128);
	// Handle couple trivial cases
	if constexpr(0 == i)
		return vec;
	if constexpr(0 == (i % 8))
		return _mm_srli_si128(vec, i / 8);

	if constexpr(i > 64) {
		// Shifting by more than 8 bytes, the lowest half will be all zeros
		vec = _mm_srli_si128(vec, 8);
		return _mm_srli_epi64(vec, i - 64);
	} else {
		// Shifting by less than 8 bytes.
		// Need to propagate a few bits across 64-bit lanes.
		__m128i low = _mm_srli_si128(vec, 8);
		__m128i high = _mm_srli_epi64(vec, i);
		low = _mm_slli_epi64(low, 64 - i);
		return _mm_or_si128(low, high);
	}
}

template<size_t Size>
class BitSet {
public:
	static constexpr size_t BLOCK_COUNT = (Size + 63) / 64;
	uint64_t data[BLOCK_COUNT]; // 6 bits per uint64_t, smaller ones should have more specific implementations
private:
	constexpr uint64_t& getBlock(size_t index) {
		assert(index >> 6 < BLOCK_COUNT);
		return this->data[index >> 6];
	}
	constexpr uint64_t getBlock(size_t index) const {
		assert(index >> 6 < BLOCK_COUNT);
		return this->data[index >> 6];
	}
	static constexpr uint64_t makeMask(size_t index) {
		return uint64_t(1) << (index & 0x3F);
	}
public:

	constexpr BitSet() : data{} {
		for(size_t i = 0; i < BLOCK_COUNT; i++) {
			this->data[i] = 0;
		}
	}

	template<size_t OtherSize>
	BitSet(const BitSet<OtherSize>& other) : data{} {
		if constexpr(OtherSize <= 64) {
			this->data[0] = static_cast<uint64_t>(other.data);
			for(size_t i = 1; i < BLOCK_COUNT; i++) {
				this->data[i] = 0;
			}
		} else if constexpr(OtherSize == 128) {
			this->data[0] = _mm_extract_epi64(other.data, 0);
			this->data[1] = _mm_extract_epi64(other.data, 1);
			for(size_t i = 2; i < BLOCK_COUNT; i++) {
				this->data[i] = 0;
			}
		} else {
			if(this->BLOCK_COUNT >= other.BLOCK_COUNT) {
				for(size_t i = 0; i < other.BLOCK_COUNT; i++) {
					this->data[i] = 0;
				}
				for(size_t i = other.BLOCK_COUNT; i < BLOCK_COUNT; i++) {
					this->data[i] = 0;
				}
			} else {
				for(size_t i = 0; i < this->BLOCK_COUNT; i++) {
					this->data[i] = 0;
				}
			}
		}
	}

	template<size_t OtherSize>
	BitSet& operator=(const BitSet<OtherSize>& other) {
		*this = BitSet(other);
	}

	static constexpr size_t size() {
		return Size;
	}

	uint64_t hash() const {
		uint64_t result = 0;
		for(uint64_t item : this->data) {
			result ^= item;
		}
		return result;
	}
	size_t count() const {
		size_t result = 0;
		for(uint64_t item : this->data) {
			result += static_cast<size_t>(popcnt64(item));
		}
		return result;
	}
	size_t getFirstOnBit() const {
		for(size_t curBlockI = 0; curBlockI < BLOCK_COUNT; curBlockI++) {
			uint64_t curBlock = data[curBlockI];
			if(curBlock != 0) {
				return ctz64(curBlock) + 64 * curBlockI;
			}
		}
		return Size;
	}
	size_t getLastOnBit() const {
		for(size_t curBlockI = BLOCK_COUNT; curBlockI-- > 0;) { // decrement is done after the compare!
			uint64_t curBlock = data[curBlockI];
			if(curBlock != 0) {
				return clz64(curBlock) + 64 * curBlockI;
			}
		}
		return 0;
	}

	constexpr bool get(size_t index) const {
		assert(index < size());
		uint64_t subPart = getBlock(index);
		return (subPart & makeMask(index)) != 0;
	}

	constexpr void set(size_t index) {
		assert(index < size());
		uint64_t& subPart = getBlock(index);
		subPart |= makeMask(index);
	}

	constexpr void reset(size_t index) {
		assert(index < size());
		uint64_t& subPart = getBlock(index);
		subPart &= ~makeMask(index);
	}

	constexpr void toggle(size_t index) {
		assert(index < size());
		uint64_t& subPart = getBlock(index);
		subPart ^= makeMask(index);
	}

	constexpr BitSet reverse() const {
		BitSet result;

		for(size_t i = 0; i < BLOCK_COUNT; i++) {
			result.data[BLOCK_COUNT - 1 - i] = reverse64(this->data[i]);
		}
		return result;
	}

	constexpr BitSet& operator|=(const BitSet& other) {
		for(size_t i = 0; i < BLOCK_COUNT; i++) {
			this->data[i] |= other.data[i];
		}
		return *this;
	}
	constexpr BitSet& operator&=(const BitSet& other) {
		for(size_t i = 0; i < BLOCK_COUNT; i++) {
			this->data[i] &= other.data[i];
		}
		return *this;
	}
	constexpr BitSet& operator^=(const BitSet& other) {
		for(size_t i = 0; i < BLOCK_COUNT; i++) {
			this->data[i] ^= other.data[i];
		}
		return *this;
	}
	constexpr BitSet operator~() const {
		BitSet result;
		for(size_t block = 0; block < BLOCK_COUNT; block++) {
			result.data[block] = ~this->data[block];
		}
		return result;
	}
	constexpr BitSet& operator<<=(size_t shift) {
		size_t blockOffset = shift >> 6;

		size_t relativeShift = shift & 0x3F;

		if(relativeShift != 0) {
			for(size_t i = BLOCK_COUNT - 1; i > blockOffset; i--) {
				this->data[i] = this->data[i - blockOffset] << relativeShift | this->data[i - blockOffset - 1] >> (64 - relativeShift);
			}

			this->data[blockOffset] = this->data[0] << relativeShift;
		} else {
			for(size_t i = BLOCK_COUNT - 1; i >= blockOffset; i--) {
				this->data[i] = this->data[i - blockOffset];
			}
		}

		for(size_t i = 0; i < blockOffset; i++) {
			this->data[i] = uint64_t(0);
		}

		return *this;
	}

	constexpr BitSet& operator>>=(size_t shift) {
		size_t blockOffset = shift >> 6;

		size_t relativeShift = shift & 0x3F;

		if(relativeShift != 0) {
			for(size_t i = 0; i < BLOCK_COUNT - blockOffset - 1; i++) {
				this->data[i] = this->data[i + blockOffset] >> relativeShift | this->data[i + blockOffset + 1] << (64 - relativeShift);
			}

			this->data[BLOCK_COUNT - blockOffset - 1] = this->data[BLOCK_COUNT - 1] >> relativeShift;
		} else {
			for(size_t i = 0; i < BLOCK_COUNT - blockOffset; i++) {
				this->data[i] = this->data[i + blockOffset];
			}
		}

		for(size_t i = BLOCK_COUNT - blockOffset; i < BLOCK_COUNT; i++) {
			this->data[i] = uint64_t(0);
		}

		return *this;
	}

	constexpr bool operator==(const BitSet& other) const {
		for(size_t block = 0; block < BLOCK_COUNT; block++) {
			if(this->data[block] != other.data[block]) {
				return false;
			}
		}
		return true;
	}
	constexpr bool operator!=(const BitSet& other) const {
		for(size_t block = 0; block < BLOCK_COUNT; block++) {
			if(this->data[block] != other.data[block]) {
				return true;
			}
		}
		return false;
	}
	constexpr bool operator>(const BitSet& other) const {
		for(size_t i = BLOCK_COUNT; i > 0; i--) { // check Most Significant Bits first
			if(this->data[i-1] != other.data[i-1]) {
				return this->data[i-1] > other.data[i-1];
			}
		}
		return false; // equality
	}
	constexpr bool operator<(const BitSet& other) const { return other > *this; }
	constexpr bool operator<=(const BitSet& other) const { return !(*this >other); }
	constexpr bool operator>=(const BitSet& other) const { return !(other > *this); }

	constexpr bool isEmpty() const {
		for(uint64_t item : this->data) {
			if(item != 0x0000000000000000) return false;
		}
		return true;
	}
	constexpr bool isFull() const {
		for(uint64_t item : this->data) {
			if(item != 0xFFFFFFFFFFFFFFFF) return false;
		}
		return true;
	}

	static constexpr BitSet empty() {
		BitSet result;
		for(uint64_t& item : result.data) {
			item = 0x0000000000000000;
		}
		return result;
	}

	static constexpr BitSet full() {
		BitSet result;
		for(uint64_t& item : result.data) {
			item = 0xFFFFFFFFFFFFFFFF;
		}
		return result;
	}

	// expects a function of the form void(size_t i)
	template<typename Func>
	void forEachOne(const Func& func) const {
		for(size_t block = 0; block < BLOCK_COUNT; block++) {
			uint64_t curBlock = this->data[block];
			while(curBlock != 0) {
				int firstBit = ctz64(curBlock);
				func(block * 64 + firstBit);
				curBlock &= ~(uint64_t(1) << firstBit);
			}
		}
	}
private:
	template<size_t CurBlock, typename Func>
	void forEachSubSetRecurse(BitSet& currentlyWorkingOn, const Func& func) const {
		currentlyWorkingOn.data[CurBlock] = this->data[CurBlock];
		while(true) {
			if constexpr(CurBlock == 0) {
				func(currentlyWorkingOn);
			} else {
				forEachSubSetRecurse<CurBlock - 1>(currentlyWorkingOn, func);
			}
			if(currentlyWorkingOn.data[CurBlock] == 0) break;
			currentlyWorkingOn.data[CurBlock]--;
			currentlyWorkingOn.data[CurBlock] &= this->data[CurBlock];
		}
	}
public:
	// iterates over every subset of the 1 bits in this bitset. 
	// Example: forEachSubSet of 0b1011 would run the function for: {0b1011, 0b1010, 0b1001, 0b1000, 0b0011, 0b0010, 0b0001, 0b0000}
	// expects a function of the form void(const BitSet<Size>& bs)
	template<typename Func>
	void forEachSubSet(const Func& func) const {
		BitSet result = *this;
		forEachSubSetRecurse<BLOCK_COUNT - 1>(result, func);
	}
};

// SSE implementation
template<>
class BitSet<128> {
	static __m128i makeMask(size_t index) {
		if(index >= 64) {
			return _mm_set_epi64x(uint64_t(1) << (index & 0b111111U), uint64_t(0));
		} else {
			return _mm_set_epi64x(uint64_t(0), uint64_t(1) << index);
		}
	}
public:
	__m128i data;


	BitSet() : data(_mm_setzero_si128()) {}

	template<size_t OtherSize>
	BitSet(const BitSet<OtherSize>& other) : data{} {
		if constexpr(OtherSize <= 64) {
			this->data = _mm_set_epi64(0, other.data);
		} else if constexpr(OtherSize == 128) {
			this->data = other.data;
		} else {
			this->data = _mm_set_epi64(other.data[1], other.data[0]);
		}
	}

	template<size_t OtherSize>
	BitSet& operator=(const BitSet<OtherSize>& other) {
		*this = BitSet(other);
		return *this;
	}

	static constexpr size_t size() {
		return 128;
	}
	uint64_t hash() const {
		return _mm_extract_epi64(this->data, 0) ^ _mm_extract_epi64(this->data, 1);
	}
	size_t count() const {
		return popcnt64(_mm_extract_epi64(this->data, 0)) + popcnt64(_mm_extract_epi64(this->data, 1));
	}
	size_t getFirstOnBit() const {
		uint64_t firstPart = _mm_extract_epi64(this->data, 0);
		if(firstPart != 0) {
			return ctz64(firstPart);
		}
		uint64_t secondPart = _mm_extract_epi64(this->data, 1);
		if(secondPart != 0) {
			return ctz64(secondPart) + size_t(64);
		}
		return 128;
	}
	size_t getLastOnBit() const {
		uint64_t secondPart = _mm_extract_epi64(this->data, 1);
		if(secondPart != 0) {
			return clz64(secondPart) + size_t(64);
		}
		uint64_t firstPart = _mm_extract_epi64(this->data, 0);
		if(firstPart != 0) {
			return clz64(firstPart);
		}
		return 0;
	}

	bool get(size_t index) const {
		assert(index < size());
		if(index >= 64) {
			return (_mm_extract_epi64(this->data, 1) & (uint64_t(1) << (index & 0b111111U))) != 0;
		} else {
			return (_mm_extract_epi64(this->data, 0) & (uint64_t(1) << index)) != 0;
		}
	}

	void set(size_t index) {
		assert(index < size());
		this->data = _mm_or_si128(makeMask(index), this->data);
	}

	void reset(size_t index) {
		assert(index < size());
		this->data = _mm_andnot_si128(makeMask(index), this->data);
	}

	void toggle(size_t index) {
		assert(index < size());
		this->data = _mm_xor_si128(makeMask(index), this->data);
	}

	BitSet reverse() const {
		BitSet result;
		__m128i reverse8 = _mm_shuffle_epi8(this->data, _mm_set_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15));
		__m128i mask4 = _mm_set1_epi8(0xF0);
		__m128i reverse4 = _mm_or_si128(_mm_slli_epi16(_mm_andnot_si128(mask4, reverse8), 4), _mm_srli_epi16(_mm_and_si128(mask4, reverse8), 4));
		__m128i mask2 = _mm_set1_epi8(0b11001100);
		__m128i reverse2 = _mm_or_si128(_mm_slli_epi16(_mm_andnot_si128(mask2, reverse4), 2), _mm_srli_epi16(_mm_and_si128(mask2, reverse4), 2));
		__m128i mask1 = _mm_set1_epi8(0b10101010);
		result.data = _mm_or_si128(_mm_slli_epi16(_mm_andnot_si128(mask1, reverse2), 1), _mm_srli_epi16(_mm_and_si128(mask1, reverse2), 1));
		return result;
	}

	BitSet& operator|=(const BitSet& other) {
		this->data = _mm_or_si128(this->data, other.data);
		return *this;
	}
	BitSet& operator&=(const BitSet& other) {
		this->data = _mm_and_si128(this->data, other.data);
		return *this;
	}
	BitSet& operator^=(const BitSet& other) {
		this->data = _mm_xor_si128(this->data, other.data);
		return *this;
	}
	BitSet operator~() const {
		BitSet result;
		// very weird, there is no intrinsic for 'not'
		result.data = _mm_xor_si128(this->data, _mm_cmpeq_epi32(_mm_setzero_si128(), _mm_setzero_si128()));
		return result;
	}
	BitSet& operator<<=(size_t shift) {
		assert(shift < size());

		__m128i shifted64 = _mm_slli_si128(this->data, 8);
		if(shift < 64) {
			__m128i shiftedInParts = _mm_sll_epi64(this->data, _mm_set1_epi64x(shift));
			__m128i backShifted = _mm_srl_epi64(shifted64, _mm_set1_epi64x(64 - int(shift)));
			this->data = _mm_or_si128(shiftedInParts, backShifted);
		} else {
			this->data = _mm_sll_epi64(shifted64, _mm_set1_epi64x(shift & 0b111111U));
		}
		return *this;
	}
	BitSet& operator>>=(size_t shift) {
		assert(shift < size());
		__m128i shifted64 = _mm_srli_si128(this->data, 8);
		if(shift < 64) {
			__m128i shiftedInParts = _mm_srl_epi64(this->data, _mm_set1_epi64x(shift));
			__m128i backShifted = _mm_sll_epi64(shifted64, _mm_set1_epi64x(64 - int(shift)));
			this->data = _mm_or_si128(shiftedInParts, backShifted);
		} else {
			this->data = _mm_srl_epi64(shifted64, _mm_set1_epi64x(shift & 0b111111U));
		}
		return *this;
	}
	bool operator==(const BitSet& other) const {
		__m128i isEq = _mm_cmpeq_epi64(this->data, other.data);
		return _mm_movemask_epi8(isEq) == 0xFFFF;
	}
	bool operator!=(const BitSet& other) const {
		__m128i isEq = _mm_cmpeq_epi64(this->data, other.data);
		return _mm_movemask_epi8(isEq) != 0xFFFF;
	}
	bool operator>(const BitSet& other) const {
		__m128i isGt = _mm_cmpgt_epi64(this->data, other.data);
		if(_mm_extract_epi64(isGt, 1)) {
			return true;
		} else {
			__m128i isEq = _mm_cmpeq_epi64(this->data, other.data);
			if(_mm_extract_epi64(isEq, 1)) {
				return _mm_extract_epi64(isGt, 0);
			} else {
				return false;
			}
		}
	}
	bool operator<(const BitSet& other) const { return other > *this; }
	bool operator<=(const BitSet& other) const { return !(*this > other); }
	bool operator>=(const BitSet& other) const { return !(other > *this); }
	
	bool isEmpty() const {
		__m128i isEq = _mm_cmpeq_epi64(this->data, _mm_setzero_si128());
		return _mm_movemask_epi8(isEq) == 0xFFFF;
	}
	bool isFull() const {
		__m128i isEq = _mm_cmpeq_epi64(this->data, _mm_cmpeq_epi32(_mm_setzero_si128(), _mm_setzero_si128()));
		return _mm_movemask_epi8(isEq) == 0xFFFF;
	}

	static BitSet empty() {
		BitSet result;
		result.data = _mm_setzero_si128();
		return result;
	}

	static BitSet full() {
		BitSet result;
		result.data = _mm_cmpeq_epi32(_mm_setzero_si128(), _mm_setzero_si128());
		return result;
	}

	// expects a function of the form void(size_t i)
	template<typename Func>
	void forEachOne(const Func& func) const {
		uint64_t part0 = _mm_extract_epi64(this->data, 0);
		uint64_t part1 = _mm_extract_epi64(this->data, 1);
		
		while(part0 != 0) {
			int firstBit = ctz64(part0);
			func(firstBit);
			part0 &= ~(uint64_t(1) << firstBit);
		}

		while(part1 != 0) {
			int firstBit = ctz64(part1);
			func(64 + firstBit);
			part1 &= ~(uint64_t(1) << firstBit);
		}
	}

	// iterates over every subset of the 1 bits in this bitset. 
	// Example: forEachSubSet of 0b1011 would run the function for: {0b1011, 0b1010, 0b1001, 0b1000, 0b0011, 0b0010, 0b0001, 0b0000}
	// expects a function of the form void(const BitSet<Size>& bs)
	template<typename Func>
	void forEachSubSet(const Func& func) const {
		uint64_t mask0 = _mm_extract_epi64(this->data, 0);
		uint64_t mask1 = _mm_extract_epi64(this->data, 1);
		uint64_t data1 = mask1;

		while(true) {
			uint64_t data0 = mask0;
			while(true) {
				BitSet result;
				result.data = _mm_set_epi64x(data1, data0);
				func(result);
				if(data0 == 0) break;
				data0--;
				data0 &= mask0;
			}
			if(data1 == 0) break;
			data1--;
			data1 &= mask1;
		}
	}
};

template<>
class BitSet<64> {
	static constexpr uint64_t makeMask(size_t index) {
		return uint64_t(1) << index;
	}
public:
	uint64_t data;

	constexpr BitSet() : data{0} {}

	template<size_t OtherSize>
	BitSet(const BitSet<OtherSize>& other) : data{} {
		if constexpr(OtherSize <= 64) {
			this->data = static_cast<uint64_t>(other.data);
		} else if constexpr(OtherSize == 128) {
			this->data = static_cast<uint64_t>(_mm_extract_epi64(other.data, 0));
		} else {
			this->data = static_cast<uint64_t>(other.data[0]);
		}
	}

	template<size_t OtherSize>
	BitSet& operator=(const BitSet<OtherSize>& other) {
		*this = BitSet(other);
		return *this;
	}

	static constexpr size_t size() {
		return 64;
	}
	uint64_t hash() const {
		return static_cast<uint64_t>(this->data);
	}
	size_t count() const {
		return popcnt64(this->data);
	}
	size_t getFirstOnBit() const {
		return ctz64(this->data);
	}
	size_t getLastOnBit() const {
		return clz64(this->data);
	}

	constexpr bool get(size_t index) const {
		assert(index < size());
		return (this->data & makeMask(index)) != 0;
	}

	constexpr void set(size_t index) {
		assert(index < size());
		this->data |= makeMask(index);
	}

	constexpr void reset(size_t index) {
		assert(index < size());
		this->data &= ~makeMask(index);
	}

	constexpr void toggle(size_t index) {
		assert(index < size());
		this->data ^= makeMask(index);
	}

	constexpr BitSet reverse() const {
		BitSet result;
		result.data = reverse64(this->data);
		return result;
	}

	constexpr BitSet& operator|=(const BitSet& other) {
		this->data |= other.data;
		return *this;
	}
	constexpr BitSet& operator&=(const BitSet& other) {
		this->data &= other.data;
		return *this;
	}
	constexpr BitSet& operator^=(const BitSet& other) {
		this->data ^= other.data;
		return *this;
	}
	constexpr BitSet operator~() const {
		BitSet result;
		result.data = ~this->data;
		return result;
	}
	constexpr BitSet& operator<<=(size_t shift) {
		assert(shift < size());
		this->data <<= shift;
		return *this;
	}
	constexpr BitSet& operator>>=(size_t shift) {
		assert(shift < size());
		this->data >>= shift;
		return *this;
	}
	constexpr bool operator==(const BitSet& other) const {
		return this->data == other.data;
	}
	constexpr bool operator!=(const BitSet& other) const {
		return this->data != other.data;
	}
	constexpr bool operator<(const BitSet& other) const {
		return this->data < other.data;
	}
	constexpr bool operator>(const BitSet& other) const {
		return this->data > other.data;
	}
	constexpr bool operator<=(const BitSet& other) const {
		return this->data <= other.data;
	}
	constexpr bool operator>=(const BitSet& other) const {
		return this->data >= other.data;
	}

	constexpr bool isEmpty() const {
		return this->data == 0x0000000000000000;
	}
	constexpr bool isFull() const {
		return this->data == 0xFFFFFFFFFFFFFFFF;
	}

	static constexpr BitSet empty() {
		BitSet result;
		result.data = 0x0000000000000000;
		return result;
	}

	static constexpr BitSet full() {
		BitSet result;
		result.data = 0xFFFFFFFFFFFFFFFF;
		return result;
	}
	
	// expects a function of the form void(size_t i)
	template<typename Func>
	void forEachOne(const Func& func) const {
		uint64_t buf = this->data;
		while(buf != 0) {
			int firstBit = ctz64(buf);
			func(firstBit);
			buf &= ~(uint64_t(1) << firstBit);
		}
	}

	// iterates over every subset of the 1 bits in this bitset. 
	// Example: forEachSubSet of 0b1011 would run the function for: {0b1011, 0b1010, 0b1001, 0b1000, 0b0011, 0b0010, 0b0001, 0b0000}
	// expects a function of the form void(const BitSet<Size>& bs)
	template<typename Func>
	void forEachSubSet(const Func& func) const {
		BitSet subSet = *this;
		while(true) {
			func(subSet);
			if(subSet.data == 0) break;
			subSet.data--;
			subSet.data &= this->data;
		}
	}
};

template<>
class BitSet<32> {
	static constexpr uint32_t makeMask(size_t index) {
		return uint32_t(1) << index;
	}
public:
	uint32_t data;

	constexpr BitSet() : data{0} {}

	template<size_t OtherSize>
	BitSet(const BitSet<OtherSize>& other) : data{} {
		if constexpr(OtherSize <= 64) {
			this->data = static_cast<uint32_t>(other.data);
		} else if constexpr(OtherSize == 128) {
			this->data = static_cast<uint32_t>(_mm_extract_epi64(other.data, 0));
		} else {
			this->data = static_cast<uint32_t>(other.data[0]);
		}
	}

	template<size_t OtherSize>
	BitSet& operator=(const BitSet<OtherSize>& other) {
		*this = BitSet(other);
		return *this;
	}

	static constexpr size_t size() {
		return 32;
	}
	uint64_t hash() const {
		return static_cast<uint64_t>(this->data);
	}
	size_t count() const {
		return popcnt32(this->data);
	}
	size_t getFirstOnBit() const {
		return ctz32(this->data);
	}
	size_t getLastOnBit() const {
		return clz32(this->data);
	}

	constexpr bool get(size_t index) const {
		assert(index < size());
		return (this->data & makeMask(index)) != 0;
	}

	constexpr void set(size_t index) {
		assert(index < size());
		this->data |= makeMask(index);
	}

	constexpr void reset(size_t index) {
		assert(index < size());
		this->data &= ~makeMask(index);
	}

	constexpr void toggle(size_t index) {
		assert(index < size());
		this->data ^= makeMask(index);
	}

	constexpr BitSet reverse() const {
		BitSet result;
		result.data = reverse32(this->data);
		return result;
	}

	constexpr BitSet& operator|=(const BitSet& other) {
		this->data |= other.data;
		return *this;
	}
	constexpr BitSet& operator&=(const BitSet& other) {
		this->data &= other.data;
		return *this;
	}
	constexpr BitSet& operator^=(const BitSet& other) {
		this->data ^= other.data;
		return *this;
	}
	constexpr BitSet operator~() const {
		BitSet result;
		result.data = ~this->data;
		return result;
	}
	constexpr BitSet& operator<<=(size_t shift) {
		assert(shift < size());
		this->data <<= shift;
		return *this;
	}
	constexpr BitSet& operator>>=(size_t shift) {
		assert(shift < size());
		this->data >>= shift;
		return *this;
	}
	constexpr bool operator==(const BitSet& other) const {
		return this->data == other.data;
	}
	constexpr bool operator!=(const BitSet& other) const {
		return this->data != other.data;
	}
	constexpr bool operator<(const BitSet& other) const {
		return this->data < other.data;
	}
	constexpr bool operator>(const BitSet& other) const {
		return this->data > other.data;
	}
	constexpr bool operator<=(const BitSet& other) const {
		return this->data <= other.data;
	}
	constexpr bool operator>=(const BitSet& other) const {
		return this->data >= other.data;
	}

	constexpr bool isEmpty() const {
		return this->data == 0x00000000;
	}
	constexpr bool isFull() const {
		return this->data == 0xFFFFFFFF;
	}

	static constexpr BitSet empty() {
		BitSet result;
		result.data = 0x00000000;
		return result;
	}

	static constexpr BitSet full() {
		BitSet result;
		result.data = 0xFFFFFFFF;
		return result;
	}

	// expects a function of the form void(size_t i)
	template<typename Func>
	void forEachOne(const Func& func) const {
		uint32_t buf = this->data;
		while(buf != 0) {
			int firstBit = ctz32(buf);
			func(firstBit);
			buf &= ~(uint32_t(1) << firstBit);
		}
	}

	// iterates over every subset of the 1 bits in this bitset. 
	// Example: forEachSubSet of 0b1011 would run the function for: {0b1011, 0b1010, 0b1001, 0b1000, 0b0011, 0b0010, 0b0001, 0b0000}
	// expects a function of the form void(const BitSet<Size>& bs)
	template<typename Func>
	void forEachSubSet(const Func& func) const {
		BitSet subSet = *this;
		while(true) {
			func(subSet);
			if(subSet.data == 0) break;
			subSet.data--;
			subSet.data &= this->data;
		}
	}
};

template<>
class BitSet<16> {
	static constexpr uint16_t makeMask(size_t index) {
		return uint16_t(1) << index;
	}
public:
	uint16_t data;

	constexpr BitSet() : data{0} {}

	template<size_t OtherSize>
	BitSet(const BitSet<OtherSize>& other) : data{} {
		if constexpr(OtherSize <= 64) {
			this->data = static_cast<uint16_t>(other.data);
		} else if constexpr(OtherSize == 128) {
			this->data = static_cast<uint16_t>(_mm_extract_epi64(other.data, 0));
		} else {
			this->data = static_cast<uint16_t>(other.data[0]);
		}
	}

	template<size_t OtherSize>
	BitSet& operator=(const BitSet<OtherSize>& other) {
		*this = BitSet(other);
		return *this;
	}

	static constexpr size_t size() {
		return 16;
	}
	uint64_t hash() const {
		return static_cast<uint64_t>(this->data);
	}
	size_t count() const {
		return popcnt16(this->data);
	}
	size_t getFirstOnBit() const {
		return ctz16(this->data);
	}
	size_t getLastOnBit() const {
		return clz16(this->data);
	}

	constexpr bool get(size_t index) const {
		assert(index < size());
		return (this->data & makeMask(index)) != 0;
	}

	constexpr void set(size_t index) {
		assert(index < size());
		this->data |= makeMask(index);
	}

	constexpr void reset(size_t index) {
		assert(index < size());
		this->data &= ~makeMask(index);
	}

	constexpr void toggle(size_t index) {
		assert(index < size());
		this->data ^= makeMask(index);
	}

	constexpr BitSet reverse() const {
		BitSet result;
		result.data = reverse16(this->data);
		return result;
	}

	constexpr BitSet& operator|=(const BitSet& other) {
		this->data |= other.data;
		return *this;
	}
	constexpr BitSet& operator&=(const BitSet& other) {
		this->data &= other.data;
		return *this;
	}
	constexpr BitSet& operator^=(const BitSet& other) {
		this->data ^= other.data;
		return *this;
	}
	constexpr BitSet operator~() const {
		BitSet result;
		result.data = ~this->data;
		return result;
	}

	constexpr BitSet& operator<<=(size_t shift) {
		assert(shift < size());
		this->data <<= shift;
		return *this;
	}
	constexpr BitSet& operator>>=(size_t shift) {
		assert(shift < size());
		this->data >>= shift;
		return *this;
	}
	constexpr bool operator==(const BitSet& other) const {
		return this->data == other.data;
	}
	constexpr bool operator!=(const BitSet& other) const {
		return this->data != other.data;
	}
	constexpr bool operator<(const BitSet& other) const {
		return this->data < other.data;
	}
	constexpr bool operator>(const BitSet& other) const {
		return this->data > other.data;
	}
	constexpr bool operator<=(const BitSet& other) const {
		return this->data <= other.data;
	}
	constexpr bool operator>=(const BitSet& other) const {
		return this->data >= other.data;
	}

	constexpr bool isEmpty() const {
		return this->data == 0x0000;
	}
	constexpr bool isFull() const {
		return this->data == 0xFFFF;
	}

	static constexpr BitSet empty() {
		BitSet result;
		result.data = 0x0000;
		return result;
	}

	static constexpr BitSet full() {
		BitSet result;
		result.data = 0xFFFF;
		return result;
	}

	// expects a function of the form void(size_t i)
	template<typename Func>
	void forEachOne(const Func& func) const {
		uint16_t buf = this->data;
		while(buf != 0) {
			int firstBit = ctz16(buf);
			func(firstBit);
			buf &= ~(uint16_t(1) << firstBit);
		}
	}

	// iterates over every subset of the 1 bits in this bitset. 
	// Example: forEachSubSet of 0b1011 would run the function for: {0b1011, 0b1010, 0b1001, 0b1000, 0b0011, 0b0010, 0b0001, 0b0000}
	// expects a function of the form void(const BitSet<Size>& bs)
	template<typename Func>
	void forEachSubSet(const Func& func) const {
		BitSet subSet = *this;
		while(true) {
			func(subSet);
			if(subSet.data == 0) break;
			subSet.data--;
			subSet.data &= this->data;
		}
	}
};

template<>
class BitSet<8> {
	static constexpr uint8_t makeMask(size_t index) {
		return uint8_t(1) << index;
	}
public:
	uint8_t data;

	constexpr BitSet() : data{0} {}

	template<size_t OtherSize>
	BitSet(const BitSet<OtherSize>& other) : data{} {
		if constexpr(OtherSize <= 64) {
			this->data = static_cast<uint8_t>(other.data);
		} else if constexpr(OtherSize == 128) {
			this->data = static_cast<uint8_t>(_mm_extract_epi64(other.data, 0));
		} else {
			this->data = static_cast<uint8_t>(other.data[0]);
		}
	}

	template<size_t OtherSize>
	BitSet& operator=(const BitSet<OtherSize>& other) {
		*this = BitSet(other);
		return *this;
	}

	static constexpr size_t size() {
		return 8;
	}
	uint64_t hash() const {
		return static_cast<uint64_t>(this->data);
	}
	size_t count() const {
		return popcnt8(static_cast<uint16_t>(this->data));
	}
	size_t getFirstOnBit() const {
		return ctz8(this->data);
	}
	size_t getLastOnBit() const {
		return clz8(this->data);
	}

	constexpr bool get(size_t index) const {
		assert(index < size());
		return (this->data & makeMask(index)) != 0;
	}

	constexpr void set(size_t index) {
		assert(index < size());
		this->data |= makeMask(index);
	}

	constexpr void reset(size_t index) {
		assert(index < size());
		this->data &= ~makeMask(index);
	}

	constexpr void toggle(size_t index) {
		assert(index < size());
		this->data ^= makeMask(index);
	}

	constexpr BitSet reverse() const {
		BitSet result;
		result.data = reverse8(this->data);
		return result;
	}

	constexpr BitSet& operator|=(const BitSet& other) {
		this->data |= other.data;
		return *this;
	}
	constexpr BitSet& operator&=(const BitSet& other) {
		this->data &= other.data;
		return *this;
	}
	constexpr BitSet& operator^=(const BitSet& other) {
		this->data ^= other.data;
		return *this;
	}
	constexpr BitSet operator~() const {
		BitSet result;
		result.data = ~this->data;
		return result;
	}
	constexpr BitSet& operator<<=(size_t shift) {
		assert(shift < size());
		this->data <<= shift;
		return *this;
	}
	constexpr BitSet& operator>>=(size_t shift) {
		assert(shift < size());
		this->data >>= shift;
		return *this;
	}
	constexpr bool operator==(const BitSet& other) const {
		return this->data == other.data;
	}
	constexpr bool operator!=(const BitSet& other) const {
		return this->data != other.data;
	}
	constexpr bool operator<(const BitSet& other) const {
		return this->data < other.data;
	}
	constexpr bool operator>(const BitSet& other) const {
		return this->data > other.data;
	}
	constexpr bool operator<=(const BitSet& other) const {
		return this->data <= other.data;
	}
	constexpr bool operator>=(const BitSet& other) const {
		return this->data >= other.data;
	}

	constexpr bool isEmpty() const {
		return this->data == 0x00;
	}
	constexpr bool isFull() const {
		return this->data == 0xFF;
	}

	static constexpr BitSet empty() {
		BitSet result;
		result.data = 0x00;
		return result;
	}

	static constexpr BitSet full() {
		BitSet result;
		result.data = 0xFF;
		return result;
	}

	// expects a function of the form void(size_t i)
	template<typename Func>
	void forEachOne(const Func& func) const {
		uint8_t buf = this->data;
		while(buf != 0) {
			int firstBit = ctz8(buf);
			func(firstBit);
			buf &= ~(uint8_t(1) << firstBit);
		}
	}

	// iterates over every subset of the 1 bits in this bitset. 
	// Example: forEachSubSet of 0b1011 would run the function for: {0b1011, 0b1010, 0b1001, 0b1000, 0b0011, 0b0010, 0b0001, 0b0000}
	// expects a function of the form void(const BitSet<Size>& bs)
	template<typename Func>
	void forEachSubSet(const Func& func) const {
		BitSet subSet = *this;
		while(true) {
			func(subSet);
			if(subSet.data == 0) break;
			subSet.data--;
			subSet.data &= this->data;
		}
	}
};

template<>
class BitSet<4> {
	static constexpr uint8_t makeMask(size_t index) {
		return uint8_t(1) << index;
	}
public:
	uint8_t data;

	constexpr BitSet() : data{0} {}

	template<size_t OtherSize>
	BitSet(const BitSet<OtherSize>& other) : data{} {
		if constexpr(OtherSize <= 64) {
			this->data = static_cast<uint8_t>(other.data) & 0b1111;
		} else if constexpr(OtherSize == 128) {
			this->data = static_cast<uint8_t>(_mm_extract_epi64(other.data, 0)) & 0b1111;
		} else {
			this->data = static_cast<uint8_t>(other.data[0]) & 0b1111;
		}
	}

	template<size_t OtherSize>
	BitSet& operator=(const BitSet<OtherSize>& other) {
		*this = BitSet(other);
		return *this;
	}

	static constexpr size_t size() {
		return 4;
	}
	uint64_t hash() const {
		return static_cast<uint64_t>(this->data);
	}
	size_t count() const {
		return popcnt8(static_cast<uint16_t>(this->data));
	}
	size_t getFirstOnBit() const {
		return ctz8(this->data);
	}
	size_t getLastOnBit() const {
		return clz8(this->data);
	}

	constexpr bool get(size_t index) const {
		assert(index < size());
		return (this->data & makeMask(index)) != 0;
	}

	constexpr void set(size_t index) {
		assert(index < size());
		this->data |= makeMask(index);
	}

	constexpr void reset(size_t index) {
		assert(index < size());
		this->data &= ~makeMask(index);
	}

	constexpr void toggle(size_t index) {
		assert(index < size());
		this->data ^= makeMask(index);
	}

	constexpr BitSet reverse() const {
		BitSet result;
		result.data = reverse4(this->data);
		return result;
	}

	constexpr BitSet& operator|=(const BitSet& other) {
		this->data |= other.data;
		return *this;
	}
	constexpr BitSet& operator&=(const BitSet& other) {
		this->data &= other.data;
		return *this;
	}
	constexpr BitSet& operator^=(const BitSet& other) {
		this->data ^= other.data;
		return *this;
	}
	constexpr BitSet operator~() const {
		BitSet result;
		result.data = ~this->data & 0b1111;
		return result;
	}
	constexpr BitSet& operator<<=(size_t shift) {
		assert(shift < size());
		this->data <<= shift;
		return *this;
	}
	constexpr BitSet& operator>>=(size_t shift) {
		assert(shift < size());
		this->data >>= shift;
		return *this;
	}
	constexpr bool operator==(const BitSet& other) const {
		return this->data == other.data;
	}
	constexpr bool operator!=(const BitSet& other) const {
		return this->data != other.data;
	}
	constexpr bool operator<(const BitSet& other) const {
		return this->data < other.data;
	}
	constexpr bool operator>(const BitSet& other) const {
		return this->data > other.data;
	}
	constexpr bool operator<=(const BitSet& other) const {
		return this->data <= other.data;
	}
	constexpr bool operator>=(const BitSet& other) const {
		return this->data >= other.data;
	}

	constexpr bool isEmpty() const {
		return this->data == 0b0000;
	}
	constexpr bool isFull() const {
		return this->data == 0b1111;
	}

	static constexpr BitSet empty() {
		BitSet result;
		result.data = 0b0000;
		return result;
	}

	static constexpr BitSet full() {
		BitSet result;
		result.data = 0b1111;
		return result;
	}

	// expects a function of the form void(size_t i)
	template<typename Func>
	void forEachOne(const Func& func) const {
		uint8_t buf = this->data;
		while(buf != 0) {
			int firstBit = ctz8(buf);
			func(firstBit);
			buf &= ~(uint8_t(1) << firstBit);
		}
	}

	// iterates over every subset of the 1 bits in this bitset. 
	// Example: forEachSubSet of 0b1011 would run the function for: {0b1011, 0b1010, 0b1001, 0b1000, 0b0011, 0b0010, 0b0001, 0b0000}
	// expects a function of the form void(const BitSet<Size>& bs)
	template<typename Func>
	void forEachSubSet(const Func& func) const {
		BitSet subSet = *this;
		while(true) {
			func(subSet);
			if(subSet.data == 0) break;
			subSet.data--;
			subSet.data &= this->data;
		}
	}
};

template<>
class BitSet<2> {
	static constexpr uint8_t makeMask(size_t index) {
		return uint8_t(1) << index;
	}
public:
	uint8_t data;

	constexpr BitSet() : data{0} {}

	template<size_t OtherSize>
	BitSet(const BitSet<OtherSize>& other) : data{} {
		if constexpr(OtherSize <= 64) {
			this->data = static_cast<uint8_t>(other.data) & 0b11;
		} else if constexpr(OtherSize == 128) {
			this->data = static_cast<uint8_t>(_mm_extract_epi64(other.data, 0)) & 0b11;
		} else {
			this->data = static_cast<uint8_t>(other.data[0]) & 0b11;
		}
	}

	template<size_t OtherSize>
	BitSet& operator=(const BitSet<OtherSize>& other) {
		*this = BitSet(other);
		return *this;
	}

	static constexpr size_t size() {
		return 2;
	}
	uint64_t hash() const {
		return static_cast<uint64_t>(this->data);
	}
	size_t count() const {
		return popcnt8(static_cast<uint16_t>(this->data));
	}
	size_t getFirstOnBit() const {
		return ctz8(this->data);
	}
	size_t getLastOnBit() const {
		return clz8(this->data);
	}

	constexpr bool get(size_t index) const {
		assert(index < size());
		return (this->data & makeMask(index)) != 0;
	}

	constexpr void set(size_t index) {
		assert(index < size());
		this->data |= makeMask(index);
	}

	constexpr void reset(size_t index) {
		assert(index < size());
		this->data &= ~makeMask(index);
	}

	constexpr void toggle(size_t index) {
		assert(index < size());
		this->data ^= makeMask(index);
	}

	constexpr BitSet reverse() const {
		BitSet result;
		result.data = reverse2(this->data);
		return result;
	}

	constexpr BitSet& operator|=(const BitSet& other) {
		this->data |= other.data;
		return *this;
	}
	constexpr BitSet& operator&=(const BitSet& other) {
		this->data &= other.data;
		return *this;
	}
	constexpr BitSet& operator^=(const BitSet& other) {
		this->data ^= other.data;
		return *this;
	}
	constexpr BitSet operator~() const {
		BitSet result;
		result.data = ~this->data & 0b11;
		return result;
	}
	constexpr BitSet& operator<<=(size_t shift) {
		assert(shift < size());
		this->data <<= shift;
		return *this;
	}
	constexpr BitSet& operator>>=(size_t shift) {
		assert(shift < size());
		this->data >>= shift;
		return *this;
	}
	constexpr bool operator==(const BitSet& other) const {
		return this->data == other.data;
	}
	constexpr bool operator!=(const BitSet& other) const {
		return this->data != other.data;
	}
	constexpr bool operator<(const BitSet& other) const {
		return this->data < other.data;
	}
	constexpr bool operator>(const BitSet& other) const {
		return this->data > other.data;
	}
	constexpr bool operator<=(const BitSet& other) const {
		return this->data <= other.data;
	}
	constexpr bool operator>=(const BitSet& other) const {
		return this->data >= other.data;
	}

	constexpr bool isEmpty() const {
		return this->data == 0b00;
	}
	constexpr bool isFull() const {
		return this->data == 0b11;
	}

	static constexpr BitSet empty() {
		BitSet result;
		result.data = 0b00;
		return result;
	}

	static constexpr BitSet full() {
		BitSet result;
		result.data = 0b11;
		return result;
	}

	// expects a function of the form void(size_t i)
	template<typename Func>
	void forEachOne(const Func& func) const {
		uint8_t buf = this->data;
		while(buf != 0) {
			int firstBit = ctz8(buf);
			func(firstBit);
			buf &= ~(uint8_t(1) << firstBit);
		}
	}

	// iterates over every subset of the 1 bits in this bitset. 
	// Example: forEachSubSet of 0b1011 would run the function for: {0b1011, 0b1010, 0b1001, 0b1000, 0b0011, 0b0010, 0b0001, 0b0000}
	// expects a function of the form void(const BitSet<Size>& bs)
	template<typename Func>
	void forEachSubSet(const Func& func) const {
		BitSet subSet = *this;
		while(true) {
			func(subSet);
			if(subSet.data == 0) break;
			subSet.data--;
			subSet.data &= this->data;
		}
	}
};

template<size_t Size>
constexpr BitSet<Size> operator|(BitSet<Size> result, const BitSet<Size>& b) {
	result |= b;
	return result;
}
template<size_t Size>
constexpr BitSet<Size> operator&(BitSet<Size> result, const BitSet<Size>& b) {
	result &= b;
	return result;
}
template<size_t Size>
constexpr BitSet<Size> operator^(BitSet<Size> result, const BitSet<Size>& b) {
	result ^= b;
	return result;
}
// computes a & ~b
template<size_t Size>
constexpr BitSet<Size> andnot(const BitSet<Size>& a, const BitSet<Size>& b) {
	if constexpr(Size == 128) {
		BitSet<Size> result;
		result.data = _mm_andnot_si128(b.data, a.data); // for some reason this does ~first & second, but the methods swaps this around as it is more logical
		return result;
	} else {
		return a & ~b;
	}
}

template<size_t Size>
constexpr BitSet<Size> operator<<(BitSet<Size> result, unsigned int shift) {
	result <<= shift;
	return result;
}
template<size_t Size>
constexpr BitSet<Size> operator>>(BitSet<Size> result, unsigned int shift) {
	result >>= shift;
	return result;
}

template<size_t Size>
bool isSubSet(const BitSet<Size>& smaller, const BitSet<Size>& larger) {
	return andnot(smaller, larger).isEmpty();
}
