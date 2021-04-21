#include "testsMain.h"
#include "testUtils.h"
#include "../dedelib/generators.h"

#include "../dedelib/connectGraph.h"
#include "../dedelib/pcoeff.h"

template<unsigned int Variables>
struct TestConnectCountVeryFast {
	static void run() {
		for(size_t iter = 0; iter < 10; iter++) {
			Monotonic<Variables> top(generateMBF<Variables>());
			forEachMonotonicFunctionUpTo(top, [&](Monotonic<Variables> bot) {
				BooleanFunction<Variables> diff = andnot(top.bf, bot.bf);

				//std::cout << "top: " << top << "    bot: " << bot << "    diff: " << diff << " => ";

				if(top == bot) return; // continue
				size_t originalCount = countConnected(top.asAntiChain() - bot, bot);
				size_t newCount = countConnectedVeryFast(diff);

				//std::cout << newCount << "\n";

				ASSERT(newCount == originalCount);
			});
			std::cout << '.';
		}
	}
};


TEST_CASE(testConnectCountVeryFast) {
	rand();
	runFunctionRange<5, 6, TestConnectCountVeryFast>();
}

template<unsigned int Variables>
struct TestPCoeffSumFast {
	static void run() {
		for(size_t iter = 0; iter < 10000; iter++) {
			again:
			Monotonic<Variables> top(generateMBF<Variables>());
			Monotonic<Variables> bot(generateMBF<Variables>());
			if(!(bot <= top)) goto again;
			if(bot == top) return; // continue;

			BooleanFunction<Variables> diff = andnot(top.bf, bot.bf);

			//std::cout << "top: " << top << "    bot: " << bot << "    diff: " << diff << " => ";

			size_t originalCount = computePermutationPCoeffSum(splitAC(top.asAntiChain()), top, bot);
			size_t newCount = computePermutationPCoeffSumFast(splitAC(top.asAntiChain()), top, bot);

			//std::cout << newCount << "\n";

			ASSERT(newCount == originalCount);
			if(iter % 1000 == 0) std::cout << '.';
		}
	}
};

TEST_CASE(testTestPCoeffSumFast) {
	rand();
	runFunctionRange<6, 7, TestPCoeffSumFast>();
}

TEST_CASE(testGroupingMaskOptimisation) {
	constexpr int Variables = 7;
	for(size_t iter = 0; iter < 10000; iter++) {
		BooleanFunction<Variables> bf = generateBF<Variables>() & generateBF<Variables>() & generateBF<Variables>(); // reduce the number of 1s

		BooleanFunction<Variables> correctMask = getGroupingMaskNaive(bf);
		BooleanFunction<Variables> foundMask = getGroupingMask(bf);

		ASSERT(correctMask == foundMask);
	}
}
