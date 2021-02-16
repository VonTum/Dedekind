#include "../toString.h"

#include "testsMain.h"
#include "testUtils.h"
#include "generators.h"
#include "../knownData.h"

#include "../interval.h"


template<unsigned int Variables>
struct BotToTopIntervalSize {
	static void run() {
		Interval<Variables> i(getBot<Variables>(), getTop<Variables>());

		ASSERT(i.intevalSizeNaive() == dedekindNumbers[Variables]);
	}
};

template<unsigned int Variables>
struct IntervalSizeVeryNaive {
	static void run() {
		for(int iter = 0; iter < SMALL_ITER; iter++) {
			//std::cout << ".";
			Interval<Variables> i = generateInterval<Variables>();

			ASSERT(i.intevalSizeVeryNaive() == i.intevalSizeNaive());
		}
	}
};

template<unsigned int Variables>
struct BotToTopIntervalSizeFast {
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

TEST_CASE(testBotToTopIntervalSize) {
	runFunctionRange<1, 5, BotToTopIntervalSize>();
}

TEST_CASE(testIntervalSizeVeryNaive) {
	runFunctionRange<1, 4, IntervalSizeVeryNaive>();
}

TEST_CASE(testBotToTopIntervalSizeFast) {
	runFunctionRange<1, 4, BotToTopIntervalSizeFast>();
}
