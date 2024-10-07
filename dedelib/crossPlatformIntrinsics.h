#pragma once

#include <cstdint>


#ifdef _MSC_VER
#include <intrin.h>
#endif

#ifdef _MSC_VER
inline int popcnt64(uint64_t x) {
	return __popcnt64(x);
}
inline int popcnt32(uint32_t x) {
	return __popcnt(x);
}
inline int popcnt16(uint16_t x) {
	return __popcnt16(x);
}
inline int popcnt8(uint8_t x) {
	return __popcnt16(x);
}
#else
__always_inline int popcnt64(uint64_t x) {
	return __builtin_popcountll(x);
}
__always_inline int popcnt32(uint32_t x) {
	return __builtin_popcountl(x);
}
__always_inline int popcnt16(uint16_t x) {
	return __builtin_popcount(x);
}
__always_inline int popcnt8(uint8_t x) {
	return __builtin_popcount(x);
}
#endif

#ifdef _MSC_VER
inline int ctz64(uint64_t x) {
	unsigned long ret;
	_BitScanForward64(&ret, x);
	return static_cast<int>(ret);
}
inline int ctz32(uint32_t x) {
	unsigned long ret;
	_BitScanForward(&ret, x);
	return static_cast<int>(ret);
}
inline int ctz16(uint16_t x) {
	unsigned long ret;
	_BitScanForward(&ret, x);
	return static_cast<int>(ret);
}
inline int ctz8(uint8_t x) {
	unsigned long ret;
	_BitScanForward(&ret, x);
	return static_cast<int>(ret);
}
#else
__always_inline int ctz64(uint64_t x) {
	return __builtin_ctzll(x);
}
__always_inline int ctz32(uint32_t x) {
	return __builtin_ctzl(x);
}
__always_inline int ctz16(uint16_t x) {
	return __builtin_ctz(x);
}
__always_inline int ctz8(uint8_t x) {
	return __builtin_ctz(x);
}
#endif

#ifdef _MSC_VER
inline int clz64(uint64_t x) {
	unsigned long ret;
	_BitScanReverse64(&ret, x);
	return static_cast<int>(ret);
}
inline int clz32(uint32_t x) {
	unsigned long ret;
	_BitScanReverse(&ret, x);
	return static_cast<int>(ret);
}
inline int clz16(uint16_t x) {
	unsigned long ret;
	_BitScanReverse(&ret, x);
	return static_cast<int>(ret);
}
inline int clz8(uint8_t x) {
	unsigned long ret;
	_BitScanReverse(&ret, x);
	return static_cast<int>(ret);
}
#else
__always_inline int clz64(uint64_t x) {
	return __builtin_clzll(x) ^ (sizeof(unsigned long long) * 8 - 1); // gcc can optimize these into a single 'bsr' instruction
}
__always_inline int clz32(uint32_t x) {
	return __builtin_clzl(x) ^ (sizeof(unsigned long) * 8 - 1);
}
__always_inline int clz16(uint16_t x) {
	return __builtin_clz(x) ^ (sizeof(unsigned int) * 8 - 1);
}
__always_inline int clz8(uint8_t x) {
	return __builtin_clz(x) ^ (sizeof(unsigned int) * 8 - 1);
}
#endif

#ifdef __SSE__
#include <immintrin.h>
inline uint64_t popcnt256(__m128i v) {
	return popcnt64(_mm_extract_epi64(v, 0))
		+ popcnt64(_mm_extract_epi64(v, 1));
}

#endif
#ifdef __AVX__
#include <immintrin.h>
inline uint64_t popcnt256(__m256i v) {
	return popcnt64(_mm256_extract_epi64(v, 0))
		+ popcnt64(_mm256_extract_epi64(v, 1))
		+ popcnt64(_mm256_extract_epi64(v, 2))
		+ popcnt64(_mm256_extract_epi64(v, 3));
}

#endif

#ifdef __AVX512F__
#include <immintrin.h>
inline uint64_t popcnt512(__m512i v) {
	uint64_t data[8];
	#ifdef __AVX512VPOPCNTDQ__
	__m512i cnt = _mm512_popcnt_epi64(v);
	_mm512_store_epi64(&data, cnt);
	#else
	_mm512_store_epi64(&data, v);
	for(int i = 0; i < 8; i++) {
		data[i] = popcnt64(data[i]);
	}
	#endif

	return data[0] + data[1] + data[2] + data[3] + data[4] + data[5] + data[6] + data[7];
}
#endif
