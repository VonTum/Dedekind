#pragma once

#include <cassert>
#include <cstdint>
#include <immintrin.h>

#include "crossPlatformIntrinsics.h"

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

	size_t count() const {
		size_t result = 0;
		for(uint64_t item : this->data) {
			result += static_cast<size_t>(__popcnt64(item));
		}
		return result;
	}
	uint64_t hash() const {
		uint64_t result = 0;
		for(uint64_t item : this->data) {
			result ^= item;
		}
		return result;
	}
	size_t getFirstOnBit() const {
		for(size_t curBlockI = 0; curBlockI < BLOCK_COUNT; curBlockI++) {
			uint64_t curBlock = data[curBlockI];
			if(curBlock != 0) {
				return countZeros(curBlock) + 64 * curBlockI;
			}
		}
		return Size;
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
	constexpr BitSet& operator<<=(unsigned int shift) {
		int blockOffset = shift >> 6;

		int relativeShift = shift & 0x3F;

		if(relativeShift != 0) {
			for(int i = BLOCK_COUNT - 1; i > blockOffset; i--) {
				this->data[i] = this->data[i - blockOffset] << relativeShift | this->data[i - blockOffset - 1] >> (64 - relativeShift);
			}

			this->data[blockOffset] = this->data[0] << relativeShift;
		} else {
			for(int i = BLOCK_COUNT - 1; i >= blockOffset; i--) {
				this->data[i] = this->data[i - blockOffset];
			}
		}

		for(int i = 0; i < blockOffset; i++) {
			this->data[i] = uint64_t(0);
		}

		return *this;
	}

	constexpr BitSet& operator>>=(unsigned int shift) {
		int blockOffset = shift >> 6;

		int relativeShift = shift & 0x3F;

		if(relativeShift != 0) {
			for(int i = 0; i < BLOCK_COUNT - blockOffset - 1; i++) {
				this->data[i] = this->data[i + blockOffset] >> relativeShift | this->data[i + blockOffset + 1] << (64 - relativeShift);
			}

			this->data[BLOCK_COUNT - blockOffset - 1] = this->data[BLOCK_COUNT - 1] >> relativeShift;
		} else {
			for(int i = 0; i < BLOCK_COUNT - blockOffset; i++) {
				this->data[i] = this->data[i + blockOffset];
			}
		}

		for(int i = BLOCK_COUNT - blockOffset; i < BLOCK_COUNT; i++) {
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
	}

	static constexpr size_t size() {
		return 128;
	}
	size_t count() const {
		return __popcnt64(_mm_extract_epi64(this->data, 0)) + __popcnt64(_mm_extract_epi64(this->data, 1));
	}
	uint64_t hash() const {
		return _mm_extract_epi64(this->data, 0) ^ _mm_extract_epi64(this->data, 1);
	}
	size_t getFirstOnBit() const {
		uint64_t firstPart = _mm_extract_epi64(this->data, 0);
		if(firstPart != 0) {
			return countZeros(firstPart);
		}
		uint64_t secondPart = _mm_extract_epi64(this->data, 1);
		if(secondPart != 0) {
			return countZeros(secondPart) + 64;
		}
		return 128;
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
	BitSet& operator<<=(unsigned int shift) {
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
	BitSet& operator>>=(unsigned int shift) {
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
	}

	static constexpr size_t size() {
		return 64;
	}
	size_t count() const {
		return __popcnt64(this->data);
	}
	uint64_t hash() const {
		return static_cast<uint64_t>(this->data);
	}
	size_t getFirstOnBit() const {
		return countZeros(this->data);
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
	constexpr BitSet& operator<<=(unsigned int shift) {
		assert(shift < size());
		this->data <<= shift;
		return *this;
	}
	constexpr BitSet& operator>>=(unsigned int shift) {
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
	}

	static constexpr size_t size() {
		return 32;
	}
	size_t count() const {
		return __popcnt(this->data);
	}
	uint64_t hash() const {
		return static_cast<uint64_t>(this->data);
	}
	size_t getFirstOnBit() const {
		return countZeros(this->data);
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
	constexpr BitSet& operator<<=(unsigned int shift) {
		assert(shift < size());
		this->data <<= shift;
		return *this;
	}
	constexpr BitSet& operator>>=(unsigned int shift) {
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
	}

	static constexpr size_t size() {
		return 16;
	}
	size_t count() const {
		return __popcnt16(this->data);
	}
	uint64_t hash() const {
		return static_cast<uint64_t>(this->data);
	}
	size_t getFirstOnBit() const {
		return countZeros(this->data);
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

	constexpr BitSet& operator<<=(unsigned int shift) {
		assert(shift < size());
		this->data <<= shift;
		return *this;
	}
	constexpr BitSet& operator>>=(unsigned int shift) {
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
	}

	static constexpr size_t size() {
		return 8;
	}
	size_t count() const {
		return __popcnt16(static_cast<uint16_t>(this->data));
	}
	uint64_t hash() const {
		return static_cast<uint64_t>(this->data);
	}
	size_t getFirstOnBit() const {
		return countZeros(this->data);
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
	constexpr BitSet& operator<<=(unsigned int shift) {
		assert(shift < size());
		this->data <<= shift;
		return *this;
	}
	constexpr BitSet& operator>>=(unsigned int shift) {
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
	}

	static constexpr size_t size() {
		return 4;
	}
	size_t count() const {
		return __popcnt16(static_cast<uint16_t>(this->data));
	}
	uint64_t hash() const {
		return static_cast<uint64_t>(this->data);
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
	constexpr BitSet& operator<<=(unsigned int shift) {
		assert(shift < size());
		this->data <<= shift;
		return *this;
	}
	constexpr BitSet& operator>>=(unsigned int shift) {
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
	}

	static constexpr size_t size() {
		return 2;
	}
	size_t count() const {
		return __popcnt16(static_cast<uint16_t>(this->data));
	}
	uint64_t hash() const {
		return static_cast<uint64_t>(this->data);
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
	constexpr BitSet& operator<<=(unsigned int shift) {
		assert(shift < size());
		this->data <<= shift;
		return *this;
	}
	constexpr BitSet& operator>>=(unsigned int shift) {
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
