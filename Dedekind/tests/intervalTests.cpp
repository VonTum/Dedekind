#include "testsMain.h"
#include "testUtils.h"
#include "generators.h"
#include "../knownData.h"
#include "../toString.h"

#include "../interval.h"
#include "../intervalSizeCache.h"
#include "../intervalSizeFromBottom.h"
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
				logStream << "bot: " << bot.bf.bitset << ", top: " << top.bf.bitset << "\n";
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

TEST_PROPERTY(testIntervalSizeExtentionBetter) {
	constexpr unsigned int Variables = 7;
	using MBF = Monotonic<Variables>;
	using BF = BooleanFunction<Variables>;
	using INT = Interval<Variables>;
	using AC = AntiChain<Variables>;

	MBF mbf(generateMBF<Variables>());

	INT base(MBF::getBot(), mbf);
	uint64_t correctExtendedSize = intervalSizeFast(base.bot, base.top);

	std::cout << mbf << ": ";

	AC possibleExtentions = mbf.asAntiChain();

	possibleExtentions.getTopLayer().forEachOne([&](size_t newBit) {
		std::cout << FunctionInput{unsigned int(newBit)} << " ";
		MBF smaller = mbf;
		smaller.remove(newBit);

		INT subInterval(MBF::getBot(), smaller);

		uint64_t subIntervalSize = intervalSizeFast(subInterval.bot, subInterval.top);
		std::cout << subIntervalSize << ", ";
		uint64_t extendedSize = computeIntervalSizeExtention(smaller, subIntervalSize, newBit);

		if(extendedSize != correctExtendedSize) {
			std::cout << mbf << ": ";
			std::cout << FunctionInput{unsigned int(newBit)} << " ";
			std::cout << subIntervalSize << ", ";

			std::cout << extendedSize << " != " << correctExtendedSize << "!\n";
			__debugbreak();
		}
	});

	std::cout << "\n";
}

TEST_CASE(testIntervalSizeExtentionABC_ABD) {
	constexpr unsigned int Variables = 4;
	using MBF = Monotonic<Variables>;
	using BF = BooleanFunction<Variables>;
	using INT = Interval<Variables>;
	using AC = AntiChain<Variables>;

	MBF mbf = AC{0b0111, 0b1011}.asMonotonic();

	INT base(MBF::getBot(), mbf);
	uint64_t correctExtendedSize = intervalSizeFast(base.bot, base.top);

	//std::cout << mbf << ": ";

	AC possibleExtentions = mbf.asAntiChain();

	possibleExtentions.getTopLayer().forEachOne([&](size_t newBit) {
		//std::cout << FunctionInput{unsigned int(newBit)} << " ";
		MBF smaller = mbf;
		smaller.remove(newBit);

		INT subInterval(MBF::getBot(), smaller);

		uint64_t subIntervalSize = intervalSizeFast(subInterval.bot, subInterval.top);
		//std::cout << subIntervalSize << ", ";
		uint64_t extendedSize = computeIntervalSizeExtention(smaller, subIntervalSize, newBit);

		ASSERT(extendedSize == correctExtendedSize);
	});

	//std::cout << "\n";
}


TEST_CASE(testIntervalSizeExtentionABC_ABD_E) {
	constexpr unsigned int Variables = 5;
	using MBF = Monotonic<Variables>;
	using BF = BooleanFunction<Variables>;
	using INT = Interval<Variables>;
	using AC = AntiChain<Variables>;
	std::cout << "\n";

	MBF mbf = AC{0b00111, 0b01011, 0b10000}.asMonotonic();

	INT base(MBF::getBot(), mbf);
	uint64_t correctExtendedSize = intervalSizeFast(base.bot, base.top);

	//std::cout << mbf << ": ";

	AC possibleExtentions = mbf.asAntiChain();

	possibleExtentions.getTopLayer().forEachOne([&](size_t newBit) {
		//std::cout << FunctionInput{unsigned int(newBit)} << " ";
		MBF smaller = mbf;
		smaller.remove(newBit);

		INT subInterval(MBF::getBot(), smaller);

		uint64_t subIntervalSize = intervalSizeFast(subInterval.bot, subInterval.top);
		//std::cout << subIntervalSize << ", ";
		uint64_t extendedSize = computeIntervalSizeExtention(smaller, subIntervalSize, newBit);
		
		if(extendedSize != correctExtendedSize) {
			std::cout << "\n";
			std::cout << mbf << ": ";
			std::cout << FunctionInput{unsigned int(newBit)} << " ";
			std::cout << subIntervalSize << ", ";

			std::cout << extendedSize << " != " << correctExtendedSize << "!\n";
			__debugbreak();
		}
	});

	//std::cout << "\n";
}
