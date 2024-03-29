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

