#pragma once

#include <cstdint>

#ifdef _MSC_VER
#include <intrin.h>
inline int countZeros(uint64_t x) {
	unsigned long ret;
	_BitScanForward64(&ret, x);
	return static_cast<int>(ret);
}
inline int countZeros(uint32_t x) {
	unsigned long ret;
	_BitScanForward(&ret, x);
	return static_cast<int>(ret);
}
#else
inline int countZeros(uint64_t x) {
	return __builtin_ctzll(x);
}
inline int countZeros(uint32_t x) {
	return __builtin_ctz(x);
}
#endif

inline int countZeros(uint16_t x) {
	return countZeros(static_cast<uint32_t>(x));
}
inline int countZeros(uint8_t x) {
	return countZeros(static_cast<uint32_t>(x));
}
