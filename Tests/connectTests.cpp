#include "testsMain.h"
#include "testUtils.h"
#include "../dedelib/generators.h"

#include "../dedelib/connectGraph.h"

template<unsigned int Variables>
struct TestConnectCountVeryFast {
	static void run() {
		for(size_t iter = 0; iter < SMALL_ITER; iter++) {
			Monotonic<Variables> top(generateMBF<Variables>());
			forEachMonotonicFunctionUpTo(top, [&](Monotonic<Variables> bot) {
				BooleanFunction<Variables> diff = andnot(top.bf, bot.bf);

				std::cout << "top: " << top << "    bot: " << bot << "    diff: " << diff << " => ";

				if(top == bot) return; // continue
				size_t originalCount = countConnected(top.asAntiChain() - bot, bot);
				size_t newCount = countConnectedVeryFast(diff);

				std::cout << newCount << "\n";

				ASSERT(newCount == originalCount);
			});
		}
	}
};


TEST_CASE(testConnectCountVeryFast) {
	rand();
	runFunctionRange<5, 7, TestConnectCountVeryFast>();
}

