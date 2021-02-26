#pragma once

#include <cstdint>

#ifdef _MSC_VER
#include <intrin.h>
#endif

#ifdef _MSC_VER
struct u128 {
	uint64_t low;
	uint64_t high;

	u128() = default;
	u128(uint64_t num) : low(num), high(0) {}
	u128(uint64_t low, uint64_t high) : low(low), high(high) {}

	operator uint64_t() const {
		return low;
	}
};
#else
typedef __uint128_t u128
#endif

struct u192 {
	uint64_t low;
	uint64_t mid;
	uint64_t high;

	u192() = default;
	u192(uint64_t num) : low(num), mid(0), high(0) {}
#ifdef _MSC_VER
	u192(u128 num) : low(num.low), mid(num.high), high(0) {}
#else
	u192(u128 num) : low(num), mid(num >> 64), high(0) {}
#endif
	u192(uint64_t low, uint64_t mid, uint64_t high) : low(low), mid(mid), high(high) {}
};


inline u128 umul128(uint64_t a, uint64_t b) {
#ifdef _MSC_VER
	u128 result;
	result.low = _umul128(a, b, &result.high);
	return result;
#else
	return static_cast<__uint128_t>(a) * static_cast<__uint128_t>(b);
#endif
}
inline u192 umul192(u128 a, uint64_t b) {
	u192 result;
#ifdef _MSC_VER
	result.low = _umul128(a.low, b, &result.mid);
	uint64_t midAdd = _umul128(a.high, b, &result.high);
	unsigned char c = _addcarry_u64(0, result.mid, midAdd, &result.mid);
	if(c) result.high++;

	return result;
#else
#error "Not implemented!"
#endif
}

#ifdef _MSC_VER
inline u128& operator+=(u128& a, u128 b) {
	unsigned char c = 0;
	c = _addcarry_u64(c, a.low, b.low, &a.low);
	_addcarry_u64(c, a.high, b.high, &a.high);
	return a;
}
inline u128 operator+(u128 a, u128 b) {
	a += b;
	return a;
}
inline u128& operator*=(u128& a, uint64_t b) {
	unsigned char c = 0;
	uint64_t mulHigh;
	a.low = _umul128(a.low, b, &mulHigh);
	a.high = a.high * b + mulHigh;
	return a;
}
inline u128 operator*(u128 a, uint64_t b) {
	a *= b;
	return a;
}
#endif

#ifdef _MSC_VER
inline u192& operator+=(u192& a, u192 b) {
	unsigned char c = 0;
	c = _addcarry_u64(c, a.low, b.low, &a.low);
	c = _addcarry_u64(c, a.mid, b.mid, &a.mid);
	_addcarry_u64(c, a.high, b.high, &a.high);
	return a;
}
#else
inline u192& operator+=(u192& a, u192 b) {
#error "Not implemented!"
}
#endif
inline u192 operator+(u192 a, u192 b) {
	a += b;
	return a;
}


#include "bigint/uint256_t.h"

inline uint128_t asBigInt(u128 v) {
#ifdef _MSC_VER
	return (uint128_t(v.low) | (uint128_t(v.high) << 64));
#else
	return (uint128_t(uint64_t(v)) | (uint128_t(v >> 64) << 64));
#endif
}
inline uint256_t asBigInt(u192 v) {
	return uint256_t(v.low) | (uint256_t(v.mid) << 64) | (uint256_t(v.high) << 128);
}

#include <ostream>
inline std::ostream& operator<<(std::ostream& os, u128 v) {
	os << asBigInt(v);
	return os;
}
inline std::ostream& operator<<(std::ostream& os, u192 v) {
	os << asBigInt(v);
	return os;
}
