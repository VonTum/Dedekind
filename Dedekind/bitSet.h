#pragma once

#include <cassert>
#include <cstdint>

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
BitSet<Size> operator|(BitSet<Size> result, const BitSet<Size>& b) {
	result |= b;
	return result;
}
template<size_t Size>
BitSet<Size> operator&(BitSet<Size> result, const BitSet<Size>& b) {
	result &= b;
	return result;
}
template<size_t Size>
BitSet<Size> operator^(BitSet<Size> result, const BitSet<Size>& b) {
	result ^= b;
	return result;
}
template<size_t Size>
BitSet<Size> operator<<(BitSet<Size> result, unsigned int shift) {
	result <<= shift;
	return result;
}
template<size_t Size>
BitSet<Size> operator>>(BitSet<Size> result, unsigned int shift) {
	result >>= shift;
	return result;
}
