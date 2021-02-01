#pragma once

#include <cstdint>

#ifdef _MSC_VER
#include <intrin.h>
inline int countZeros64(uint64_t x) {
	unsigned long ret;
	_BitScanForward64(&ret, x);
	return static_cast<int>(ret);
}
inline int countZeros32(uint32_t x) {
	unsigned long ret;
	_BitScanForward(&ret, x);
	return static_cast<int>(ret);
}
#else
inline int countZeros64(uint64_t x) {
	return __builtin_ctzll(x);
}
inline int countZeros32(uint32_t x) {
	return __builtin_ctz(x);
}
#endif

inline int countZeros16(uint16_t x) {
	return countZeros32(static_cast<uint32_t>(x));
}
inline int countZeros8(uint8_t x) {
	return countZeros32(static_cast<uint32_t>(x));
}

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

