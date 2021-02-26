#include "testsMain.h"

#include "../bigint/uint128_t.h"
#include "../bigint/uint128_t.h"
#include "../u192.h"

#include "generators.h"

#include <iostream>

uint256_t mask192 = uint256_t(0xFFFFFFFFFFFFFFFF) | uint256_t(0xFFFFFFFFFFFFFFFF) << 64 | uint256_t(0xFFFFFFFFFFFFFFFF) << 128;

TEST_PROPERTY(testMul128) {
	GEN(a, genU64);
	GEN(b, genU64);

	ASSERT(asBigInt(umul128(a, b)) == uint128_t(a) * uint128_t(b));
}

TEST_PROPERTY(testMul192) {
	GEN(a, genU128);
	GEN(b, genU64);

	ASSERT(asBigInt(umul192(a, b)) == (uint256_t(asBigInt(a)) * uint256_t(b) & mask192));
}

TEST_PROPERTY(testAdd128) {
	GEN(a, genU128);
	GEN(b, genU128);

	ASSERT(asBigInt(a + b) == asBigInt(a) + asBigInt(b));
}

TEST_PROPERTY(testAdd192) {
	GEN(a, genU192);
	GEN(b, genU192);

	ASSERT(asBigInt(a + b) == ((asBigInt(a) + asBigInt(b)) & mask192));
}




