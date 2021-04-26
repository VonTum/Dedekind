#include "testsMain.h"

#include "../dedelib/bigint/uint128_t.h"
#include "../dedelib/bigint/uint128_t.h"
#include "../dedelib/u192.h"

#include "../dedelib/generators.h"

#include <iostream>

TEST_PROPERTY(testMul128) {
	GEN(a, genU64);
	GEN(b, genU64);

	ASSERT(asBigInt(umul128(a, b)) == uint128_t(a) * uint128_t(b));
}

TEST_PROPERTY(testMul192) {
	GEN(a, genU128);
	GEN(b, genU64);

	uint256_t mask192 = uint256_t(0xFFFFFFFFFFFFFFFF) | (uint256_t(0xFFFFFFFFFFFFFFFF) << 64) | (uint256_t(0xFFFFFFFFFFFFFFFF) << 128);

	uint256_t correct = uint256_t(asBigInt(a) * uint256_t(b));
	
	ASSERT(asBigInt(umul192(a, b)) == (correct & mask192));
}

TEST_PROPERTY(testAdd128) {
	GEN(a, genU128);
	GEN(b, genU128);

	ASSERT(asBigInt(a + b) == asBigInt(a) + asBigInt(b));
}

TEST_PROPERTY(testAdd192) {
	GEN(a, genU192);
	GEN(b, genU192);

	uint256_t mask192 = uint256_t(0xFFFFFFFFFFFFFFFF) | (uint256_t(0xFFFFFFFFFFFFFFFF) << 64) | (uint256_t(0xFFFFFFFFFFFFFFFF) << 128);

	uint256_t correct = asBigInt(a) + asBigInt(b);

	ASSERT(asBigInt(a + b) == (correct & mask192));
}




