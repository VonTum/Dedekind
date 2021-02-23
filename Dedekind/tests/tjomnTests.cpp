#include "testsMain.h"
#include "testUtils.h"
#include "generators.h"
#include "../knownData.h"
#include "../toString.h"

#include "../interval.h"

#include "../tjomn.h"
#include "../collectionOperations.h"

// destroys b
template<typename AIter, typename BIter>
bool unordered_contains_all(AIter aIter, AIter aEnd, BIter bStart, BIter bEnd) {
	while(aIter != aEnd) {
		for(auto bIter = bStart; bIter != bEnd; ++bIter) {
			if(*aIter == *bIter) {
				--bEnd;
				*bIter = *bEnd;
				goto nextItem;
			}
		}
		return false; // no corresponding element in b found for item
		nextItem:
		++aIter;
	}
	return true; // all elements matched
}

template<typename T>
bool unordered_equals(const std::vector<T>& a, std::vector<T> b) {
	if(a.size() != b.size()) return false;

	return unordered_contains_all(a.begin(), a.end(), b.begin(), b.end());
}

template<unsigned int Variables>
struct ConnectTest {
	static void run() {
		using MBF = Monotonic<Variables>;
		using AC = AntiChain<Variables>;
		for(int iter = 0; iter < (Variables == 6 ? 5 : SMALL_ITER * 10); iter++) {
			MBF ss = generateMonotonic<Variables>();
			MBF d = generateMonotonic<Variables>();

			std::vector<MBF> res = connect(ss.asAntiChain(), d);

			// no path between items
			for(size_t i = 0; i < res.size(); i++) {
				for(size_t j = i + 1; j < res.size(); j++) {
					ASSERT((res[i] & res[j]) <= d);
				}
			}

			// paths within items
			for(const MBF& curTesting : res) {
				AC ac = curTesting.asAntiChain();
				std::vector<std::pair<MBF, bool>> hasReached;
				ac.forEachOne([&](size_t idx) {
					hasReached.emplace_back(AC{idx}.asMonotonic(), false);
				});
				hasReached[0].second = true;
				MBF cur = AC{ac.getFirst()}.asMonotonic();

				bool hasChanged = true;
				while(hasChanged) {
					hasChanged = false;

					for(size_t i = 1; i < hasReached.size(); i++) {
						if(hasReached[i].second) continue;
						if(!((cur & hasReached[i].first) <= d)) {
							hasChanged = true;
							cur |= hasReached[i].first;
							hasReached[i].second = true;
						}
					}
				}
				for(const std::pair<MBF, bool>& item : hasReached) {
					ASSERT(item.second);
				}
			}
		}
	}
};

template<unsigned int Variables>
struct ConnectFastTest {
	static void run() {
		using MBF = Monotonic<Variables>;
		for(int iter = 0; iter < (Variables == 6 ? 5 : SMALL_ITER * 10); iter++) {
			MBF ss = generateMonotonic<Variables>();
			MBF d = generateMonotonic<Variables>();

			//printVar(ss);
			//printVar(d);

			std::vector<MBF> a = connect(ss.asAntiChain(), d);
			std::vector<MBF> b = connectFast(ss.asAntiChain(), d);

			//printVar(a);
			//printVar(b);

			ASSERT(unordered_equals(a, b));
		}
	}
};


TEST_CASE(testConnect) {
	runFunctionRange<1, 7, ConnectTest>();
}

TEST_CASE(testConnectFast) {
	runFunctionRange<1, 7, ConnectFastTest>();
}

TEST_CASE(testKnownTJOMN) {
	ASSERT(threejoinmeetnumberveryfast(AntiChain<3>{0b011}, AntiChain<3>{0b111}, AntiChain<3>{0b101}, AntiChain<3>{0b001}.asMonotonic()) == 3);
}

// Manual inspection
TEST_CASE(testKnownFourPartNonEquivalent) {
	using AC = AntiChain<4>;
	std::cout << "\n";
	fourPartNonEquivalent(AC{0b0111, 0b1100}, [&](const AC& p1, const AC& p2, const AC& p3, const AC& p4, int classSize) {
		std::cout << "{" << p1 << ", " << p2 << ", " << p3 << ", " << p4 << "}, " << classSize << "\n";
	});
}

// Manual inspection
TEST_CASE(testGenerateTaus) {
	using AC = AntiChain<3>;
	using MBF = Monotonic<3>;
	std::cout << "\n";
	generateTaus(AC{0b111}, [&](const MBF& tau0, const MBF& tau1, const MBF& tau2, const MBF& minDelta, const MBF& maxDelta, int classSize) {
		std::cout << "{" << tau0 << ", " << tau1 << ", " << tau2 << ", " << minDelta << ", " << maxDelta << "}, " << classSize << "\n";
	});
}

// Manual inspection
TEST_CASE(testGenerateNonEquivalentMonotonics) {
	using AC = AntiChain<3>;
	using MBF = Monotonic<3>;
	std::cout << "\n";
	BufferedMap<MBF, int> allChains = generateNonEquivalentMonotonics<3>();

	for(const KeyValue<MBF, int>& item : allChains) {
		std::cout << item.key << ": " << item.value << "\n";
	}
}

// Manual inspection
TEST_CASE(testThreeJoinOneMeetDecompositionsFast) {
	constexpr unsigned int Degree = 3;
	using AC = AntiChain<Degree>;
	using MBF = Monotonic<Degree>;
	std::cout << "\n";
	
	std::map<TJOMN<Degree>, TJOMNInfo> result = threeJoinOneMeetDecompositionsFast<Degree>();

	std::cout << "systems: " << result.size() << "\n";

	uint64_t maxEqClassSize = 0;
	uint64_t maxSolutionCount = 0;

	for(const std::pair<TJOMN<Degree>, TJOMNInfo>& item : result) {
		if(item.second.eqClassSize > maxEqClassSize) {
			maxEqClassSize = item.second.eqClassSize;
		}
		if(item.second.solutionCount > maxSolutionCount) {
			maxSolutionCount = item.second.solutionCount;
		}
	}
	printVar(maxEqClassSize);
	printVar(maxSolutionCount);
}

// Manual inspection
TEST_CASE(testRevolution) {
	constexpr unsigned int Degree = 4;
	using AC = AntiChain<Degree>;
	using MBF = Monotonic<Degree>;
	std::cout << "\n";

	uint256_t d = revolution<Degree>();

	std::cout << "D(" << (Degree + 3) << ") = " << d << "\n";
}