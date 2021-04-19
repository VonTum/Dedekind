#include "benchmark.h"

#include "../dedelib/u192.h"
#include "../dedelib/connectGraph.h"
#include "../dedelib/sjomn.h"
#include "../dedelib/pcoeff.h"
#include "../dedelib/generators.h"

class OldConnectBenchmark : public Benchmark {
public:
	OldConnectBenchmark() : Benchmark("oldConnect") {}

	virtual void init() override {}

	virtual void run() override {
		constexpr unsigned int Variables = 7;
		uint64_t total = 0;
		for(int iter = 0; iter < 50000; iter++) {
			Monotonic<Variables> top = generateMonotonic<Variables>();
			Monotonic<Variables> bot = generateMonotonic<Variables>();
			if(top.size() < bot.size()) std::swap(bot, top);

			bot &= top;

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
		for(int iter = 0; iter < 50000; iter++) {
			Monotonic<Variables> top = generateMonotonic<Variables>();
			Monotonic<Variables> bot = generateMonotonic<Variables>();
			if(top.size() < bot.size()) std::swap(bot, top);

			bot &= top;

			total += computePermutationPCoeffSumFast(splitAC(top.asAntiChain()), top, bot);
		}
		std::cout << total;
	}
} newConnect;
