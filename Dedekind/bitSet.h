#pragma once

#include <cassert>
#include <cstdint>
#include <immintrin.h>

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
	constexpr bool operator<(const BitSet& other) const {
		for(size_t i = BLOCK_COUNT; i > 0; i--) { // check Most Significant Bits first
			if(this->data[i-1] != other.data[i-1]) {
				return this->data[i-1] < other.data[i-1];
			}
		}
		return false; // equality
	}
	constexpr bool operator>(const BitSet& other) const {
		for(size_t i = BLOCK_COUNT; i > 0; i--) { // check Most Significant Bits first
			if(this->data[i-1] != other.data[i-1]) {
				return this->data[i-1] > other.data[i-1];
			}
		}
		return false; // equality
	}
	constexpr bool operator<=(const BitSet& other) const {
		for(size_t i = BLOCK_COUNT; i > 0; i--) { // check Most Significant Bits first
			if(this->data[i] != other.data[i]) {
				return this->data[i] < other.data[i];
			}
		}
		return true; // equality
	}
	constexpr bool operator>=(const BitSet& other) const {
		for(size_t i = BLOCK_COUNT; i > 0; i--) { // check Most Significant Bits first
			if(this->data[i] != other.data[i]) {
				return this->data[i] > other.data[i];
			}
		}
		return true; // equality
	}

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

template<>
class BitSet<64> {
	static constexpr uint64_t makeMask(size_t index) {
		return uint64_t(1) << index;
	}
public:
	uint64_t data;

	constexpr BitSet() : data{0} {}

	static constexpr size_t size() {
		return 64;
	}
	size_t count() const {
		return __popcnt64(this->data);
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

	static constexpr size_t size() {
		return 32;
	}
	size_t count() const {
		return __popcnt(this->data);
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

	static constexpr size_t size() {
		return 16;
	}
	size_t count() const {
		return __popcnt16(this->data);
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

	static constexpr size_t size() {
		return 8;
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
		return this->data == 0b0000;
	}
	constexpr bool isFull() const {
		return this->data == 0b1111;
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
class BitSet<2> {
	static constexpr uint8_t makeMask(size_t index) {
		return uint8_t(1) << index;
	}
public:
	uint8_t data;

	constexpr BitSet() : data{0} {}

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
		return this->data == 0b00;
	}
	constexpr bool isFull() const {
		return this->data == 0b11;
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
