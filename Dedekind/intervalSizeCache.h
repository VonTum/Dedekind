#pragma once

#include "interval.h"
#include "bufferedMap.h"

#include "parallelIter.h"

#include "MBFDecomposition.h"

#include <mutex>
#include <atomic>

template<unsigned int Variables, bool UseCanonization>
BooleanFunction<Variables> getIntervalBFRepresentation(const Monotonic<Variables>& bot, const Monotonic<Variables>& top) {
	assert(bot <= top);
	BooleanFunction<Variables> base = andnot(top.func, bot.func);
	if constexpr(UseCanonization) {
		return base.canonize();
	} else {
		return base;
	}
}

template<unsigned int Variables, bool UseCanonization>
uint64_t intervalSizeMemoized(const Monotonic<Variables>& intervalBot, const Monotonic<Variables>& intervalTop, BufferedMap<BooleanFunction<Variables>, uint64_t>& knownIntervalSizes, std::mutex& knownIntervalSizeMutex) {
	using MBF = Monotonic<Variables>;
	using AC = AntiChain<Variables>;
	if(!(intervalBot <= intervalTop)) {
		return 0;
	}

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

	// memoization result
	BooleanFunction<Variables> hashRep = getIntervalBFRepresentation<Variables, UseCanonization>(bot, top);
	KeyValue<BooleanFunction<Variables>, uint64_t>* found = knownIntervalSizes.find(hashRep);
	if(found) {
		return found->value;
	}

	size_t firstOnBit = topAC.getFirst();
	// arbitrarily chosen extention of bot
	MBF top1ac = AC{firstOnBit}.asMonotonic();
	MBF top1acm = top1ac.pred();
	MBF top1acmm = top1acm.pred();

	size_t universe = size_t(1U << Variables) - 1;
	AC umb1{universe & ~firstOnBit};

	uint64_t v1 = intervalSizeMemoized<Variables, UseCanonization>(bot | top1ac, top, knownIntervalSizes, knownIntervalSizeMutex);

	uint64_t v2 = 0;

	AC top1acmAsAchain = top1acm.asAntiChain();
	top1acmAsAchain.func.forEachSubSet([&](const BooleanFunction<Variables>& achainSubSet) { // this is to dodge the double function call of AntiChain::forEachSubSet
		AC ss(achainSubSet);
		//MBF subSet = (top1acm.asAntiChain() - ss).asMonotonic();
		if(ss != top1acmAsAchain) { // strict subsets
			MBF subSet = ss.asMonotonic();

			MBF gpm = subSet | top1acmm;

			MBF subTop = acProd(gpm, umb1) & top;

			uint64_t vv = intervalSizeMemoized<Variables, UseCanonization>(bot | subSet, subTop, knownIntervalSizes, knownIntervalSizeMutex);

			v2 += vv;
		}
	});

	uint64_t result = 2 * v1 + v2;
	knownIntervalSizeMutex.lock();
	knownIntervalSizes.add(hashRep, result);
	knownIntervalSizeMutex.unlock();
	return result;
}

constexpr size_t compactIntervalBufferSizes[]{2, 2, 8, 35, 347, 35877, 999999999 /*To be determined*/};
constexpr size_t fastIntervalBufferSizes[]{2, 2, 11, 99, 3936, 3257608};


template<unsigned int Variables, bool UseCanonization = (Variables >= 6)>
class IntervalSizeCache {
	BakedMap<BooleanFunction<Variables>, uint64_t> intervalSizes;

public:
	IntervalSizeCache() = default;
	IntervalSizeCache(const BufferedMap<BooleanFunction<Variables>, uint64_t>& intervals) : 
		intervalSizes(intervals, new KeyValue<BooleanFunction<Variables>, uint64_t>[intervals.size()]) {}
	~IntervalSizeCache() {
		delete[] intervalSizes.begin();
	}

	uint64_t getIntervalSize(const Monotonic<Variables>& intervalBot, const Monotonic<Variables>& intervalTop) const {
		using MBF = Monotonic<Variables>;
		using AC = AntiChain<Variables>;
		if(!(intervalBot <= intervalTop)) {
			return 0;
		}

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

		return intervalSizes.get(getIntervalBFRepresentation<Variables, UseCanonization>(bot, top));
	}

	uint64_t getIntervalSizeFromBot(const Monotonic<Variables>& top) const {
		using MBF = Monotonic<Variables>;
		using AC = AntiChain<Variables>;

		if(top.isEmpty()) { // top == bot
			return 1;
		}
		if(top == MBF::getBotSucc()) {
			return 2;
		}

		if constexpr(UseCanonization) {
			return intervalSizes.get(top.func.canonize());
		} else {
			return intervalSizes.get(top.func);
		}
	}

	static IntervalSizeCache generate() {
		if constexpr(UseCanonization) {
			std::pair<BufferedSet<Monotonic<Variables>>, uint64_t> allMBFsPair = generateAllMBFsFast<Variables>();
			const BufferedSet<Monotonic<Variables>>& allMBFs = allMBFsPair.first;

			std::mutex intervalSizeMutex;
			BufferedMap<BooleanFunction<Variables>, uint64_t> intervalSizes(compactIntervalBufferSizes[Variables]);
			std::cout << "made interval buffer\n";

			std::cout << "Iterating MBFs: " << allMBFs.size() << " mbfs found!\n";

			std::atomic<int> i = 0;

			//iterCollectionInParallel(allMBFs, [&](const Monotonic<Variables>& top) {
			for(const Monotonic<Variables>& top : allMBFs) {
				int curI = i.fetch_add(1);
				if(curI % 1000 == 0) {
					std::cout << curI << "/" << allMBFs.size() << " ---- " << intervalSizes.size() << "/" << compactIntervalBufferSizes[Variables] << " intervals found" << "\n";
				}
				forEachMonotonicFunctionUpTo<Variables>(top, [&](const Monotonic<Variables>& bot) {
					intervalSizeMemoized<Variables, UseCanonization>(bot, top, intervalSizes, intervalSizeMutex); // memoizes this interval, and all related
				});
			}
			//});

			std::cout << Variables << "> Number of intervals: " << intervalSizes.size() << "\n";

			return IntervalSizeCache(intervalSizes);
		} else {
			std::mutex intervalSizeMutex;
			BufferedMap<BooleanFunction<Variables>, uint64_t> intervalSizes(fastIntervalBufferSizes[Variables]);
			std::cout << "made interval buffer\n";

			std::atomic<int> i = 0;

			forEachMonotonicFunction<Variables>([&](const Monotonic<Variables>& top) {
				int curI = i.fetch_add(1);
				if(curI % 1000 == 0) {
					std::cout << curI << "/" << dedekindNumbers[Variables] << " ---- " << intervalSizes.size() << "/" << fastIntervalBufferSizes[Variables] << " intervals found" << "\n";
				}
				forEachMonotonicFunctionUpTo<Variables>(top, [&](const Monotonic<Variables>& bot) {
					intervalSizeMemoized<Variables, UseCanonization>(bot, top, intervalSizes, intervalSizeMutex); // memoizes this interval, and all related
				});
			});

			std::cout << Variables << "> Number of intervals: " << intervalSizes.size() << "\n";

			return IntervalSizeCache(intervalSizes);
		}
	}
};

