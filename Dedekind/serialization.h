#pragma once


#include "booleanFunction.h"
#include "funcTypes.h"

#include <iostream>
#include <cstdint>


inline void serializeU32(uint32_t value, uint8_t* outputBuf) {
	for(size_t i = 0; i < 4; i++) {
		*outputBuf++ = static_cast<uint8_t>(value >> (24 - i * 8));
	}
}

inline uint32_t deserializeU32(const uint8_t* outputBuf) {
	uint32_t result = 0;
	for(size_t i = 0; i < 4; i++) {
		result |= static_cast<uint32_t>(*outputBuf++) << (24 - i * 8);
	}
	return result;
}

inline void serializeU64(uint64_t value, uint8_t* outputBuf) {
	for(size_t i = 0; i < 8; i++) {
		*outputBuf++ = static_cast<uint8_t>(value >> (56 - i * 8));
	}
}

inline uint64_t deserializeU64(const uint8_t* outputBuf) {
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
uint8_t* serializeBooleanFunction(const BooleanFunction<Variables>& func, uint8_t* outputBuf) {
	typename BooleanFunction<Variables>::Bits bs = func.bitset;
	if constexpr(Variables <= 3) {
		*outputBuf++ = bs.data;
	} else if constexpr(Variables <= 6) {
		for(size_t i = BooleanFunction<Variables>::Bits::size(); i > 0; i -= 8) {
			*outputBuf++ = static_cast<uint8_t>(bs.data >> (i - 8));
		}
	} else if constexpr(Variables == 7) {
		serializeU64(_mm_extract_epi64(bs.data, 1), outputBuf);
		outputBuf += 8;
		serializeU64(_mm_extract_epi64(bs.data, 0), outputBuf);
		outputBuf += 8;
	} else {
		for(size_t i = 0; i < bs.BLOCK_COUNT; i++) {
			serializeU64(bs.data[bs.BLOCK_COUNT - i - 1], outputBuf);
			outputBuf += 8;
		}
	}
	return outputBuf;
}

// returns the end of the buffer
template<unsigned int Variables>
uint8_t* serializeMBF(const Monotonic<Variables>& m, uint8_t* outputBuf) {
	return serializeBooleanFunction<Variables>(m.func, outputBuf);
}

// returns the end of the buffer
template<unsigned int Variables>
uint8_t* serializeAC(const AntiChain<Variables>& m, uint8_t* outputBuf) {
	return serializeBooleanFunction<Variables>(m.func, outputBuf);
}

// returns the end of the buffer
template<unsigned int Variables>
BooleanFunction<Variables> deserializeBooleanFunction(const uint8_t* inputBuf) {
	BooleanFunction<Variables> result = BooleanFunction<Variables>::empty();
	typename BooleanFunction<Variables>::Bits& bs = result.bitset;
	if constexpr(Variables <= 3) {
		bs.data = *inputBuf;
	} else if constexpr(Variables <= 6) {
		for(size_t i = BooleanFunction<Variables>::Bits::size(); i > 0; i -= 8) {
			bs.data |= static_cast<decltype(bs.data)>(*inputBuf++) << (i - 8);
		}
	} else if constexpr(Variables == 7) {
		uint64_t part1 = deserializeU64(inputBuf);
		inputBuf += 8;
		uint64_t part0 = deserializeU64(inputBuf);
		inputBuf += 8;
		bs.data = _mm_set_epi64x(part1, part0);
	} else {
		for(size_t i = 0; i < bs.BLOCK_COUNT; i++) {
			bs.data[bs.BLOCK_COUNT - i - 1] = deserializeU64(inputBuf);
			inputBuf += 8;
		}
	}
	return result;
}
// returns the end of the buffer
template<unsigned int Variables>
Monotonic<Variables> deserializeMBF(const uint8_t* inputBuf) {
	return Monotonic<Variables>(deserializeBooleanFunction<Variables>(inputBuf));
}
// returns the end of the buffer
template<unsigned int Variables>
AntiChain<Variables> deserializeAC(const uint8_t* inputBuf) {
	return AntiChain<Variables>(deserializeBooleanFunction<Variables>(inputBuf));
}

#define MAKE_SERIALIZER(TYPE, SERIALIZE, DESERIALIZE, SIZE) \
inline void SERIALIZE(TYPE val, std::ostream& os) { uint8_t buf[SIZE]; SERIALIZE(val, buf); os.write(reinterpret_cast<const char*>(buf), SIZE);}\
inline TYPE DESERIALIZE(std::istream& is) { uint8_t buf[SIZE]; is.read(reinterpret_cast<char*>(buf), SIZE); return DESERIALIZE(buf);}

#define MAKE_VARIABLE_SERIALIZER(TYPE, SERIALIZE, DESERIALIZE, SIZE) \
template<unsigned int Variables> void SERIALIZE(TYPE val, std::ostream& os) { uint8_t buf[SIZE]; SERIALIZE<Variables>(val, buf); os.write(reinterpret_cast<const char*>(buf), SIZE);}\
template<unsigned int Variables> TYPE DESERIALIZE(std::istream& is) { uint8_t buf[SIZE]; is.read(reinterpret_cast<char*>(buf), SIZE); return DESERIALIZE<Variables>(buf);}

MAKE_SERIALIZER(uint32_t, serializeU32, deserializeU32, 4);
MAKE_SERIALIZER(uint64_t, serializeU64, deserializeU64, 8);

MAKE_VARIABLE_SERIALIZER(BooleanFunction<Variables>, serializeBooleanFunction, deserializeBooleanFunction, getMBFSizeInBytes<Variables>());
MAKE_VARIABLE_SERIALIZER(Monotonic<Variables>, serializeMBF, deserializeMBF, getMBFSizeInBytes<Variables>());
MAKE_VARIABLE_SERIALIZER(AntiChain<Variables>, serializeAC, deserializeAC, getMBFSizeInBytes<Variables>());

