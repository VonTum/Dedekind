#include "benchmark.h"

#include "../dedelib/u192.h"
#include "../dedelib/connectGraph.h"
#include "../dedelib/sjomn.h"
#include "../dedelib/pcoeff.h"
#include "../dedelib/generators.h"

#include "../dedelib/topBots.h"

#define BENCH_VARIABLES 7

template<unsigned int Variables>
std::vector<TopBots<Variables>> readTopBots() {
	std::cout << "Reading bots" << std::endl;
	std::ifstream benchFile(FileName::benchmarkSetTopBots(Variables), std::ios::binary);
	if(!benchFile.is_open()) throw "File not opened!";

	return deserializeVector(benchFile, deserializeTopBots<Variables>);
}

template<unsigned int Variables>
std::vector<std::pair<Monotonic<Variables>, Monotonic<Variables>>> makeTopBotPairs(int botFraction = 500, int permutFraction = 5) {
	std::vector<TopBots<Variables>> benchSet = readTopBots<Variables>();
	std::cout << "Converting to pairs" << std::endl;

	std::vector<std::pair<Monotonic<Variables>, Monotonic<Variables>>> result;

	std::default_random_engine generator;

	std::uniform_int_distribution botDistribution(0, botFraction);
	std::uniform_int_distribution permDistribution(0, permutFraction);
	for(const TopBots<Variables>& item : benchSet) {
		Monotonic<Variables> top = item.top;

		for(const Monotonic<Variables>& bot : item.bots) {
			if(botDistribution(generator) != 0) continue;

			bot.forEachPermutation([&](const Monotonic<Variables>& permutedBot) {
				if(permutedBot <= top) {
					if(permDistribution(generator) != 0) return;

					result.emplace_back(top, permutedBot);
				}
			});
		}
	}
	std::cout << result.size() << " pairs created" << std::endl;
	std::shuffle(result.begin(), result.end(), generator);
	return result;
}

std::vector<std::pair<Monotonic<BENCH_VARIABLES>, Monotonic<BENCH_VARIABLES>>> benchSet;

class ConnectBench : public Benchmark {
public:
	using Benchmark::Benchmark;

	virtual void init() override {
		if(benchSet.empty()) {
			if(BENCH_VARIABLES == 7) {
				benchSet = makeTopBotPairs<BENCH_VARIABLES>(500, 5);
			} else {
				benchSet = makeTopBotPairs<BENCH_VARIABLES>(0,0);
			}
		}
	}

	virtual void printResults(double deltaTimeMS) override {
		std::cout << "Took " << deltaTimeMS * 1000000 / benchSet.size() << "ns per countConnected" << std::endl;

		printHistogramAndPCoeffs(BENCH_VARIABLES, false);
	}
};

class ConnectBenchmarkGroupMerge : public ConnectBench {
public:
	ConnectBenchmarkGroupMerge() : ConnectBench("connectGroupMerge") {}

	virtual void run() override {
		uint64_t total = 0;
		for(auto& topBot : benchSet) {
			total += countConnected(AntiChain(andnot(topBot.first.asAntiChain().bf, topBot.second.bf)), topBot.second);
		}
		std::cout << total;
	}
} connectBenchGroupMerge;

class ConnectBenchmarkFloodFill : public ConnectBench {
public:
	ConnectBenchmarkFloodFill() : ConnectBench("connectFloodFill") {}

	virtual void run() override {
		uint64_t total = 0;
		for(auto& topBot : benchSet) {
			BooleanFunction<BENCH_VARIABLES> graph = andnot(topBot.first.bf, topBot.second.bf);
			total += countConnectedFloodFill(graph);
		}
		std::cout << total;
	}
} connectBenchFloodFill;

class ConnectBenchmarkSingletonElimination : public ConnectBench {
public:
	ConnectBenchmarkSingletonElimination() : ConnectBench("connectSingletonElimination") {}

	virtual void run() override {
		uint64_t total = 0;
		for(auto& topBot : benchSet) {
			BooleanFunction<BENCH_VARIABLES> graph = andnot(topBot.first.bf, topBot.second.bf);
			total += countConnectedSingletonElimination(graph);
		}
		std::cout << total;
	}
} connectBenchSingletonElimination;

class ConnectBenchmarkLeafEliminationDown : public ConnectBench {
public:
	ConnectBenchmarkLeafEliminationDown() : ConnectBench("connectLeafEliminationDown") {}

	virtual void run() override {
		uint64_t total = 0;
		for(auto& topBot : benchSet) {
			BooleanFunction<BENCH_VARIABLES> graph = andnot(topBot.first.bf, topBot.second.bf);
			eliminateLeavesDown(graph);
			total += countConnectedSingletonElimination(graph);
		}
		std::cout << total;
	}
} connectBenchLeafEliminationDown;

class ConnectBenchmarkLeafEliminationUp : public ConnectBench {
public:
	ConnectBenchmarkLeafEliminationUp() : ConnectBench("connectLeafEliminationUp") {}

	virtual void run() override {
		uint64_t total = 0;
		for(auto& topBot : benchSet) {
			BooleanFunction<BENCH_VARIABLES> graph = andnot(topBot.first.bf, topBot.second.bf);
			eliminateLeavesUp(graph);
			total += countConnectedSingletonElimination(graph);
		}
		std::cout << total;
	}
} connectBenchLeafEliminationUp;

class ConnectBenchmarkLeafEliminationDownUp : public ConnectBench {
public:
	ConnectBenchmarkLeafEliminationDownUp() : ConnectBench("connectLeafEliminationDownUp") {}

	virtual void run() override {
		uint64_t total = 0;
		for(auto& topBot : benchSet) {
			BooleanFunction<BENCH_VARIABLES> graph = andnot(topBot.first.bf, topBot.second.bf);
			eliminateLeavesDown(graph);
			eliminateLeavesUp(graph);
			total += countConnectedSingletonElimination(graph);
		}
		std::cout << total;
	}
} connectBenchLeafEliminationDownUp;

class ConnectBenchmarkLeafEliminationUpDown : public ConnectBench {
public:
	ConnectBenchmarkLeafEliminationUpDown() : ConnectBench("connectLeafEliminationUpDown") {}

	virtual void run() override {
		uint64_t total = 0;
		for(auto& topBot : benchSet) {
			BooleanFunction<BENCH_VARIABLES> graph = andnot(topBot.first.bf, topBot.second.bf);
			eliminateLeavesUp(graph);
			eliminateLeavesDown(graph);
			total += countConnectedSingletonElimination(graph);
		}
		std::cout << total;
	}
} connectBenchLeafEliminationUpDown;

class ConnectBenchmarkLeafEliminationUpDownUp : public ConnectBench {
public:
	ConnectBenchmarkLeafEliminationUpDownUp() : ConnectBench("connectLeafEliminationUpDownUp") {}

	virtual void run() override {
		uint64_t total = 0;
		for(auto& topBot : benchSet) {
			BooleanFunction<BENCH_VARIABLES> graph = andnot(topBot.first.bf, topBot.second.bf);
			eliminateLeavesUp(graph);
			eliminateLeavesDown(graph);
			eliminateLeavesUp(graph);
			total += countConnectedSingletonElimination(graph);
		}
		std::cout << total;
	}
} connectBenchLeafEliminationUpDownUp;

class ConnectBenchmarkLeafEliminationDownUpDown : public ConnectBench {
public:
	ConnectBenchmarkLeafEliminationDownUpDown() : ConnectBench("connectLeafEliminationDownUpDown") {}

	virtual void run() override {
		uint64_t total = 0;
		for(auto& topBot : benchSet) {
			BooleanFunction<BENCH_VARIABLES> graph = andnot(topBot.first.bf, topBot.second.bf);
			eliminateLeavesDown(graph);
			eliminateLeavesUp(graph);
			eliminateLeavesDown(graph);
			total += countConnectedSingletonElimination(graph);
		}
		std::cout << total;
	}
} connectBenchLeafEliminationDownUpDown;

/*class OldConnectBenchmark : public Benchmark {
public:
	OldConnectBenchmark() : Benchmark("oldConnect") {}

	virtual void init() override {}

	virtual void run() override {
		constexpr unsigned int Variables = BENCH_VARIABLES;
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
		constexpr unsigned int Variables = BENCH_VARIABLES;
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

		printHistogramAndPCoeffs(BENCH_VARIABLES);
	}
} newConnect;*/
