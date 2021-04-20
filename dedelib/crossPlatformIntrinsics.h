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
inline int popcnt64(uint64_t x) {
	return __builtin_popcountll(x);
}
inline int popcnt32(uint32_t x) {
	return __builtin_popcountl(x);
}
inline int popcnt16(uint16_t x) {
	return __builtin_popcount(x);
}
inline int popcnt8(uint8_t x) {
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
inline int ctz64(uint64_t x) {
	return __builtin_ctzll(x);
}
inline int ctz32(uint32_t x) {
	return __builtin_ctzl(x);
}
inline int ctz16(uint16_t x) {
	return __builtin_ctz(x);
}
inline int ctz8(uint8_t x) {
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
inline int clz64(uint64_t x) {
	return sizeof(long long) * 8 - __builtin_clzll(x);
}
inline int clz32(uint32_t x) {
	return sizeof(long) * 8 - __builtin_clzl(x);
}
inline int clz16(uint16_t x) {
	return sizeof(int) * 8 - __builtin_clz(x);
}
inline int clz8(uint8_t x) {
	return sizeof(int) * 8 - __builtin_clz(x);
}
#endif

