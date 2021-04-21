#include "benchmark.h"

#include "../dedelib/u192.h"
#include "../dedelib/connectGraph.h"
#include "../dedelib/sjomn.h"
#include "../dedelib/pcoeff.h"
#include "../dedelib/generators.h"

template<unsigned int Variables>
struct PCoeffBenchmarkSet {
	std::vector<std::pair<Monotonic<Variables>, Monotonic<Variables>>> benchmarkSet; // BOT TOP

	PCoeffBenchmarkSet(size_t targetSize) {
		while(benchmarkSet.size() < targetSize) {
			Monotonic<Variables> a = generateMonotonic<Variables>();
			Monotonic<Variables> b = generateMonotonic<Variables>();
			if(a.isEmpty() || b.isEmpty()) continue;
			if(a <= b) {
				benchmarkSet.push_back(std::make_pair(a, b));
			} else if(b <= a) {
				benchmarkSet.push_back(std::make_pair(b, a));
			}
		}
	}
};

PCoeffBenchmarkSet<7> benchSet(100000);


class OldConnectBenchmark : public Benchmark {
public:
	OldConnectBenchmark() : Benchmark("oldConnect") {}

	virtual void init() override {}

	virtual void run() override {
		constexpr unsigned int Variables = 7;
		uint64_t total = 0;
		for(const std::pair<Monotonic<Variables>, Monotonic<Variables>>& sample : benchSet.benchmarkSet) {
			const Monotonic<Variables>& bot = sample.first;
			const Monotonic<Variables>& top = sample.second;

			total += computePermutationPCoeffSum(splitAC(top.asAntiChain()), top, bot);
		}
		std::cout << total;
	}
} oldConnect;

class NewConnectBenchmark : public Benchmark {
public:
	NewConnectBenchmark() : Benchmark("newConnect") {}

	virtual void init() override {}

	virtual void run() override {
		constexpr unsigned int Variables = 7;
		uint64_t total = 0;
		for(const std::pair<Monotonic<Variables>, Monotonic<Variables>>& sample : benchSet.benchmarkSet) {
			const Monotonic<Variables>& bot = sample.first;
			const Monotonic<Variables>& top = sample.second;

			AntiChain<Variables> topAC = top.asAntiChain();

			SmallVector<Monotonic<Variables>, getMaxLayerWidth(Variables)> splitTop = splitAC(topAC);
			assert(splitTop.size() >= 1);
			total += computePermutationPCoeffSumFast(splitTop, top, bot);
		}
		std::cout << total;

		printHistogramAndPCoeffs(7);
	}
} newConnect;
