#include "testsMain.h"
#include "testUtils.h"
#include "generators.h"
#include "../knownData.h"
#include "../toString.h"

#include "../interval.h"


template<unsigned int Variables>
struct BotToTopIntervalSize {
	static void run() {
		Interval<Variables> i(Monotonic<Variables>::getBot(), Monotonic<Variables>::getTop());

		ASSERT(intervalSizeFast(i.bot, i.top) == dedekindNumbers[Variables]);
	}
};

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

template<unsigned int Variables>
struct IntervalSizeFast {
	static void run() {
		for(int iter = 0; iter < (Variables == 6? 5 : SMALL_ITER * 10); iter++) {
			if(iter % 100 == 1) std::cout << '.';
			Interval<Variables> i = generateInterval<Variables>();

			ASSERT(i.intevalSizeNaive() == intervalSizeFast(i.bot, i.top));
		}
	}
};

TEST_CASE(testBotToTopIntervalSize) {
	runFunctionRange<1, 6, BotToTopIntervalSize>();
}

TEST_CASE(testIntervalSize) {
	runFunctionRange<1, 5, IntervalSizeVsNaive>();
}

TEST_CASE_SLOW(testIntervalSizeFast) {
	runFunctionRange<1, 6, IntervalSizeFast>();
}
TEST_CASE_SLOW(benchIntervalSizeFast) {
	size_t totalSize = 0;
	for(int iter = 0; iter < 10000; iter++) {
		if(iter % 100 == 0) std::cout << '.';
		Interval<6> i = generateInterval<6>();

		totalSize += intervalSizeFast(i.bot, i.top);
	}
	logStream << totalSize;
	//std::cout << intervalSizeFast(getBot<7>(), getTop<7>());
}
