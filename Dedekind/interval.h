/**
* The algorithms described in this file are the work of Patrick De Causmaeker, Stefan De Wannemacker, and Jay Yellen. 
* 
* https://www.kuleuven.be/wieiswie/en/person/00003471
* https://arxiv.org/pdf/1103.2877v1.pdf
* https://arxiv.org/pdf/1407.4288.pdf
* https://arxiv.org/pdf/1602.04675v1.pdf
*/



#pragma once

#define printAC(AC) do { std::cout << #AC << " : "; prettyFibs(std::cout, (AC).asAntiChain()); std::cout << "\n"; } while(false)

#include "booleanFunction.h"
#include "bufferedMap.h"
#include "knownData.h"
#include "funcTypes.h"

#include <vector>

template<unsigned int Variables>
struct Interval {
	using MBF = Monotonic<Variables>;
	using AC = AntiChain<Variables>;

	Monotonic<Variables> bot;
	Monotonic<Variables> top;

	Interval(Monotonic<Variables> bot, Monotonic<Variables> top) : bot(bot), top(top) {
		assert(bot <= top);
	}

	bool contains(const Monotonic<Variables>& item) const {
		return this->bot <= item && item <= this->top;
	}

	// expects a function of type void(const MBF&)
	template<typename Func>
	void forEach(const Func& func) const {
		forEachMonotonicFunctionBetween<Variables>(this->bot, this->top, func);
	}

	uint64_t intevalSizeNaive() const {
		uint64_t total = 0;

		/*forEachMonotonicFunction<Variables>([this, &total](const Monotonic<Variables>& bf) {
			if(this->bot <= bf && bf <= this->top) {
				total++;
			}
		});*/

		forEachMonotonicFunctionBetween<Variables>(this->bot, this->top, [this, &total](const Monotonic<Variables>& bf) {
			total++;
		});

		return total;
	}

	uint64_t intervalSize() const {
		if(bot == top) {
			return 1;
		}

		AC nextBits = top.asAntiChain();
		// arbitrarily chosen extention of bot
		MBF b1ac = AC{nextBits.getFirst()}.asMonotonic();

		uint64_t intervalSize1 = getIntervalSizeForNonNormal(b1ac | bot, top);
		uint64_t intervalSize2 = getIntervalSizeForNonNormal(bot, b1ac.pred() | bot | (top.asAntiChain() - b1ac.asAntiChain()).asMonotonic());
		return intervalSize1 + intervalSize2;
	}
};

template<unsigned int Variables>
uint64_t getIntervalSizeForNonNormal(const Monotonic<Variables>& bot, const Monotonic<Variables>& top) {
	if(bot <= top) {
		AntiChain<Variables> dsn = bot.asAntiChain().intersection(top.asAntiChain());
		Monotonic<Variables> newTop = (top.asAntiChain() - dsn).asMonotonic();
		Monotonic<Variables> newBot = bot & newTop;

		if(newBot <= newTop) {
			return Interval(newBot, newTop).intervalSize();
		} else {
			return 0;
		}
	} else {
		return 0;
	}
}
template<unsigned int Variables>
uint64_t intervalSizeFast(const Monotonic<Variables>& intervalBot, const Monotonic<Variables>& intervalTop) {
	using MBF = Monotonic<Variables>;
	using AC = AntiChain<Variables>;
	if(!(intervalBot <= intervalTop)) {
		return 0;
	}

	//AntiChain<Variables> dsn = intersection(intervalBot.asAntiChain(), intervalTop.asAntiChain());
	//Monotonic<Variables> newTop = (intervalTop.asAntiChain() - dsn).asMonotonic();
	AC topAC = intervalTop.asAntiChain() - intervalBot.asAntiChain();
	MBF top = topAC.asMonotonic();
	MBF bot = intervalBot & top;

	if(!(bot <= top)) {
		return 0;
	}
	if(bot == top) {
		return 1;
	}
	if(top == MBF::getBotSucc()) {
		return 2;
	}

	size_t firstOnBit = topAC.getFirst();
	// arbitrarily chosen extention of bot
	MBF top1ac = AC{firstOnBit}.asMonotonic();
	MBF top1acm = top1ac.pred();
	MBF top1acmm = top1acm.pred();

	size_t universe = size_t(1U << Variables) - 1;
	AC umb1{universe & ~firstOnBit};

	uint64_t v1 = intervalSizeFast(bot | top1ac, top);

	uint64_t v2 = 0;

	AC top1acmAsAchain = top1acm.asAntiChain();
	top1acmAsAchain.func.forEachSubSet([&](const BooleanFunction<Variables>& achainSubSet) { // this is to dodge the double function call of AntiChain::forEachSubSet
		AC ss(achainSubSet);
		//MBF subSet = (top1acm.asAntiChain() - ss).asMonotonic();
		if(ss != top1acmAsAchain) { // strict subsets
			MBF subSet = ss.asMonotonic();

			MBF gpm = subSet | top1acmm;

			MBF subTop = acProd(gpm, umb1) & top;

			uint64_t vv = intervalSizeFast(bot | subSet, subTop);

			v2 += vv;
		}
	});

	return 2 * v1 + v2;
}
