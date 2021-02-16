#pragma once

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
	FunctionInputBitSet<Variables> bot;
	FunctionInputBitSet<Variables> top;

	Interval(FunctionInputBitSet<Variables> bot, FunctionInputBitSet<Variables> top) : bot(bot), top(top) {
		assert(bot.isSubSetOf(top));
	}

	bool contains(const FunctionInputBitSet<Variables>& item) const {
		return this->bot.isSubSetOf(item) && item.isSubSetOf(this->top);
	}

	// expects a function of type void(const FunctionInputBitSet<Variables>&)
	template<typename Func>
	void forEach(const Func& func) const {
		BufferedSet<FunctionInputBitSet<Variables>> curHashed(8000000); // just large enough for 6
		std::vector<FunctionInputBitSet<Variables>> toExpand;

		toExpand.push_back(bot);

		do {
			for(const FunctionInputBitSet<Variables>& curExpanding : toExpand) {
				curExpanding.forEachUpExpansion(top, [&](const FunctionInputBitSet<Variables>& expanded) {
					curHashed.add(expanded);
				});
			}
			toExpand.clear();
			toExpand.reserve(curHashed.size());
			for(const FunctionInputBitSet<Variables>& item : curHashed) {
				func(item);
				toExpand.push_back(item);
			}
			curHashed.clear();
		} while(!toExpand.empty());
	}

	uint64_t intevalSizeVeryNaive() const {
		uint64_t total = 0;
		for(uint64_t data = 0; data < (1 << (1 << Variables)); data++) {
			FunctionInputBitSet<Variables> fibs;
			fibs.bitset.data = data;

			if(fibs.isMonotonic()) {
				if(this->contains(fibs)) {
					total++;
				}
			}
		}
		return total;
	}
	
	uint64_t intevalSizeNaive() const {
		BufferedSet<FunctionInputBitSet<Variables>> curHashed(dedekindNumbers[Variables]); // just large enough for 6
		std::vector<FunctionInputBitSet<Variables>> toExpand;

		toExpand.push_back(bot);
		
		size_t totalIntervalSize = 0;

		do {
			totalIntervalSize += toExpand.size();
			for(const FunctionInputBitSet<Variables>& curExpanding : toExpand) {
				curExpanding.forEachUpExpansion(top, [&](const FunctionInputBitSet<Variables>& expanded) {
					curHashed.add(expanded);
				});
			}
			toExpand.clear();
			toExpand.reserve(curHashed.size());
			for(FunctionInputBitSet<Variables>& item : curHashed) {
				toExpand.push_back(item);
			}
			curHashed.clear();
		} while(!toExpand.empty());

		return totalIntervalSize;
	}

	uint64_t intervalSizeFast() const {
		if(!(bot.isSubSetOf(top))) {
			return 0;
		}
		if(bot == top) {
			return 1;
		}

		//std::cout << "currently processing " << bot.bitset.data << " - " << top.bitset.data << "\n";

		BitSet<1 << Variables> predOfTop = top.asAntiChain().bitset;
		// arbitrarily chosen extention of bot
		FunctionInputBitSet<Variables> b1ac = FunctionInputBitSet<Variables>::minimalForcedDown(predOfTop.getFirstOnBit());

		//std::cout << logStream.str().c_str();
		//logStream.clear();

		uint64_t intervalSize1 = getIntervalSizeForNonNormal(b1ac | bot, top);
		uint64_t intervalSize2 = getIntervalSizeForNonNormal(bot, b1ac.prev() | bot | (top - b1ac));
		return intervalSize1 + intervalSize2;
	}
};

template<unsigned int Variables>
uint64_t getIntervalSizeForNonNormal(const FunctionInputBitSet<Variables>& bot, const FunctionInputBitSet<Variables>& top) {
	if(bot.isSubSetOf(top)) {
		FunctionInputBitSet<Variables> dsn = (bot.asAntiChain() & top.asAntiChain());
		FunctionInputBitSet<Variables> newTop = (top.asAntiChain() - dsn).monotonizeDown();
		FunctionInputBitSet<Variables> newBot = bot & newTop;

		if(newBot.isSubSetOf(newTop)) {
			return Interval(newBot, newTop).intervalSizeFast();
		} else {
			return 0;
		}
	} else {
		return 0;
	}
}
