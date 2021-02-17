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

#include "functionInputBitSet.h"
#include "bufferedMap.h"
#include "knownData.h"

#include <vector>

template<unsigned int Variables>
constexpr FunctionInputBitSet<Variables> getTop() {
	return FunctionInputBitSet<Variables>::full();
}
template<unsigned int Variables>
constexpr FunctionInputBitSet<Variables> getBot() {
	return FunctionInputBitSet<Variables>::empty();
}

template<unsigned int Variables>
struct Interval {
	using MBF = FunctionInputBitSet<Variables>;

	MBF bot;
	MBF top;

	Interval(MBF bot, MBF top) : bot(bot), top(top) {
		assert(bot.isSubSetOf(top));
	}

	bool contains(const MBF& item) const {
		return this->bot.isSubSetOf(item) && item.isSubSetOf(this->top);
	}

	// expects a function of type void(const MBF&)
	template<typename Func>
	void forEach(const Func& func) const {
		BufferedSet<MBF> curHashed(8000000); // just large enough for 6
		std::vector<MBF> toExpand;

		toExpand.push_back(bot);

		do {
			for(const MBF& curExpanding : toExpand) {
				curExpanding.forEachUpExpansion(top, [&](const MBF& expanded) {
					curHashed.add(expanded);
				});
			}
			toExpand.clear();
			toExpand.reserve(curHashed.size());
			for(const MBF& item : curHashed) {
				func(item);
				toExpand.push_back(item);
			}
			curHashed.clear();
		} while(!toExpand.empty());
	}

	uint64_t intevalSizeVeryNaive() const {
		uint64_t total = 0;

		forEachMonotonicFunction<Variables>([this, &total](const MBF& fibs) {
			if(this->contains(fibs)) {
				total++;
			}
		});

		return total;
	}
	
	uint64_t intevalSizeNaive() const {
		BufferedSet<MBF> curHashed(dedekindNumbers[Variables]); // just large enough for 6
		std::vector<MBF> toExpand;

		toExpand.push_back(bot);
		
		size_t totalIntervalSize = 0;

		do {
			totalIntervalSize += toExpand.size();
			for(const MBF& curExpanding : toExpand) {
				curExpanding.forEachUpExpansion(top, [&](const MBF& expanded) {
					curHashed.add(expanded);
				});
			}
			toExpand.clear();
			toExpand.reserve(curHashed.size());
			for(MBF& item : curHashed) {
				toExpand.push_back(item);
			}
			curHashed.clear();
		} while(!toExpand.empty());

		return totalIntervalSize;
	}

	uint64_t intervalSize() const {
		if(bot == top) {
			return 1;
		}

		BitSet<1 << Variables> nextBits = top.asAntiChain().bitset;
		// arbitrarily chosen extention of bot
		MBF b1ac = MBF::minimalForcedDown(nextBits.getFirstOnBit());

		uint64_t intervalSize1 = getIntervalSizeForNonNormal(b1ac | bot, top);
		uint64_t intervalSize2 = getIntervalSizeForNonNormal(bot, b1ac.prev() | bot | (top - b1ac));
		return intervalSize1 + intervalSize2;
	}

	uint64_t intervalSizeFast() const {
		//std::cout << "intervalSizeFast: \n";
		//printAC(bot);
		//printAC(top);

		if(bot == top) {
			//std::cout << "intervalsizeFast bot == top : return 1\n";
			return 1;
		}

		BitSet<1 << Variables> nextBits = top.asAntiChain().bitset;
		if(nextBits.isEmpty() || nextBits.count() == 1 && nextBits.get(0)) {
			//std::cout << "intervalsizeFast not b1 : return 2\n";
			return 2;
		}
		size_t firstOnBit = nextBits.getFirstOnBit();
		// arbitrarily chosen extention of bot
		MBF top1ac = MBF::minimalForcedDown(firstOnBit);
		//printAC(top1ac);
		MBF top1acm = top1ac.prev();
		//printAC(top1acm);
		MBF top1acmm = top1acm.prev();
		//printAC(top1acmm);

		size_t universe = size_t(1U << Variables) - 1;
		MBF umb1 = MBF::minimalForcedDown(universe & ~(firstOnBit));
		//printAC(umb1);

		//printAC(bot | top1ac);

		//std::cout << "getting first interval...\n";
		uint64_t v1 = getIntervalSizeForNonNormalFast(bot | top1ac, top);
		//std::cout << "first interval result...\n";
		//printVar(v1);

		uint64_t v2 = 0;

		top1acm.asAntiChain().forEachSubSet([&](const MBF& ss) {
			MBF subSet = top1acm & ~ss.monotonizeDown();
			if(subSet != top1acm) { // strict subsets
				//printAC(subSet);
				MBF gpm = subSet | top1acmm;

				/*printAC(gpm);
				printAC(acProd(gpm, umb1));
				printAC(bot);
				printAC(top);*/

				//std::cout << "getting second interval...\n";
				uint64_t vv = getIntervalSizeForNonNormalFast(bot | subSet.monotonizeDown(), acProd(gpm, umb1) & top);
				//std::cout << "second interval result...\n";

				//printVar(vv);

				v2 += vv;
			}
		});
		//std::cout << "intervalsizeFast v : return " << 2 * v1 + v2 << "\n";
		return 2 * v1 + v2;
	}
};

template<unsigned int Variables>
uint64_t getIntervalSizeForNonNormal(const FunctionInputBitSet<Variables>& bot, const FunctionInputBitSet<Variables>& top) {
	if(bot.isSubSetOf(top)) {
		FunctionInputBitSet<Variables> dsn = (bot.asAntiChain() & top.asAntiChain());
		FunctionInputBitSet<Variables> newTop = (top.asAntiChain() - dsn).monotonizeDown();
		FunctionInputBitSet<Variables> newBot = bot & newTop;

		if(newBot.isSubSetOf(newTop)) {
			return Interval(newBot, newTop).intervalSize();
		} else {
			return 0;
		}
	} else {
		return 0;
	}
}
template<unsigned int Variables>
uint64_t getIntervalSizeForNonNormalFast(const FunctionInputBitSet<Variables>& bot, const FunctionInputBitSet<Variables>& top) {
	assert(bot.isMonotonic());
	assert(top.isMonotonic());

	/*std::cout << "getIntervalSizeForNonNormalFast: \n";
	std::cout << "len ";
	printAC(top);
	std::cout << "len ";
	printAC(bot);*/
	if(bot.isSubSetOf(top)) {
		FunctionInputBitSet<Variables> dsn = (bot.asAntiChain() & top.asAntiChain());
		//printAC(dsn);
		FunctionInputBitSet<Variables> newTop = (top.asAntiChain() - dsn).monotonizeDown();
		//printAC(newTop);
		FunctionInputBitSet<Variables> newBot = bot & newTop;
		//printAC(newBot);

		if(newBot.isSubSetOf(newTop)) {
			return Interval(newBot, newTop).intervalSizeFast();
		} else {
			//std::cout << "newBot !<= newTop: return 0\n";
			return 0;
		}
	} else {
		//std::cout << "bot !<= top: return 0\n";
		return 0;
	}
}
