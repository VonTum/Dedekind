#include "testsMain.h"
#include "testUtils.h"
#include "generators.h"
#include "../knownData.h"
#include "../toString.h"

#include "../interval.h"
#include "../intervalSizeCache.h"
#include "../MBFDecomposition.h"


template<unsigned int Variables>
struct BotToTopIntervalSize {
	static void run() {
		Interval<Variables> i(Monotonic<Variables>::getBot(), Monotonic<Variables>::getTop());

		ASSERT(intervalSizeFast(i.bot, i.top) == dedekindNumbers[Variables]);
	}
};

TEST_CASE(testBotToTopIntervalSize) {
	runFunctionRange<1, 6, BotToTopIntervalSize>();
}

template<unsigned int Variables>
struct IntervalSizeVsNaive {
	static void run() {
		for(int iter = 0; iter < LARGE_ITER; iter++) {
			//std::cout << ".";
			Interval<Variables> i = generateInterval<Variables>();

			/*prettyFibs(std::cout, i.bot.asAntiChain());
			std::cout << " - ";
			prettyFibs(std::cout, i.top.asAntiChain());
			std::cout << "\n";*/

			ASSERT(i.intevalSizeNaive() == getIntervalSizeForNonNormal(i.bot, i.top));
		}
	}
};

TEST_CASE(testIntervalSize) {
	runFunctionRange<1, 5, IntervalSizeVsNaive>();
}

template<unsigned int Variables>
struct IntervalSizeFast {
	static void run() {
		for(int iter = 0; iter < (Variables == 6 ? 5 : SMALL_ITER * 10); iter++) {
			if(iter % 100 == 1) std::cout << '.';
			Interval<Variables> i = generateInterval<Variables>();

			ASSERT(i.intevalSizeNaive() == intervalSizeFast(i.bot, i.top));
		}
	}
};

TEST_CASE_SLOW(testIntervalSizeFast) {
	runFunctionRange<1, 6, IntervalSizeFast>();
}


template<unsigned int Variables>
struct IntervalSizeCacheTest {
	static void run() {
		using AC = AntiChain<Variables>;
		using MBF = Monotonic<Variables>;
		using INT = Interval<Variables>;
		IntervalSizeCache<Variables> intervalSizes = IntervalSizeCache<Variables>::generate();

		MBF e = MBF::getBot();
		MBF a = MBF::getTop();

		INT(e, a).forEach([&](const MBF& top) {
			INT(e, top).forEach([&](const MBF& bot) {
				logStream << "bot: " << bot.func.bitset << ", top: " << top.func.bitset << "\n";
				uint64_t correctSize = intervalSizeFast(bot, top);
				ASSERT(intervalSizes.getIntervalSize(bot, top) == correctSize);
			});
		});
		INT(e, a).forEach([&](const MBF& top) {
			uint64_t correctSize = intervalSizeFast(e, top);
			ASSERT(intervalSizes.getIntervalSizeFromBot(top) == correctSize);
		});
	}
};

TEST_CASE(testIntervalSizeCache) {
	runFunctionRange<1, 4, IntervalSizeCacheTest>();
}




TEST_CASE_SLOW(benchIntervalSizeNaive) {
	size_t totalSize = 0;
	for(int iter = 0; iter < 50; iter++) {
		if(iter % 100 == 0) std::cout << '.';
		Interval<6> i = generateInterval<6>();

		totalSize += i.intevalSizeNaive();
	}
	std::cout << totalSize;
	//std::cout << intervalSizeFast(getBot<7>(), getTop<7>());
}
TEST_CASE_SLOW(benchIntervalSizeFast) {
	size_t totalSize = 0;
	for(int iter = 0; iter < 10000; iter++) {
		if(iter % 100 == 0) std::cout << '.';
		Interval<6> i = generateInterval<6>();

		totalSize += intervalSizeFast(i.bot, i.top);
	}
	std::cout << totalSize;
	//std::cout << intervalSizeFast(getBot<7>(), getTop<7>());
}


