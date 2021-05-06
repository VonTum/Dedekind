#include "benchmark.h"

#include "../dedelib/booleanFunction.h"
#include "../dedelib/generators.h"
#include "../dedelib/toString.h"


constexpr static int ITERS = 1000;

template<unsigned int Variables>
class CanonizationBenchmark : public Benchmark {
public:
	CanonizationBenchmark() : Benchmark("canonizationBenchmark") {}

	virtual void init() override {}

	virtual void run() override {
		BooleanFunction<Variables> totalbf = generateMBF<Variables>();
		for(int i = 0; i < ITERS; i++) {
			BooleanFunction<Variables> bf = generateMBF<Variables>();

			bf.forEachPermutation([&](const BooleanFunction<Variables>& workingOn) {
				BooleanFunction<Variables> bestBF = workingOn.canonize();
			
				totalbf ^= bestBF;
			});
		}
		std::cout << totalbf;
	}

	virtual void printResults(double deltaTimeMS) {
		double microsPerMBF = deltaTimeMS * 1000000.0 / (ITERS * factorial(Variables));
		std::cout << "Took " << microsPerMBF << "ns per canonization" << std::endl;
	}
};
CanonizationBenchmark<7> newCanonize;

template<unsigned int Variables>
class NaiveCanonizationBenchmark : public Benchmark {
public:
	NaiveCanonizationBenchmark() : Benchmark("naiveCanonizationBenchmark") {}

	virtual void init() override {}

	virtual void run() override {
		BooleanFunction<Variables> totalbf = generateMBF<Variables>();
		for(int i = 0; i < ITERS; i++) {
			BooleanFunction<Variables> bf = generateMBF<Variables>();

			bf.forEachPermutation([&](const BooleanFunction<Variables>& workingOn) {
				BooleanFunction<Variables> bestBF = workingOn;
				workingOn.forEachPermutation([&](const BooleanFunction<Variables>& permuted) {
					if(permuted.bitset <= bestBF.bitset) {
						bestBF = permuted;
					}
				});
				totalbf ^= bestBF;
			});
		}
		std::cout << totalbf;
	}

	virtual void printResults(double deltaTimeMS) {
		double microsPerMBF = deltaTimeMS * 1000000.0 / (ITERS * factorial(Variables));
		std::cout << "Took " << microsPerMBF << "ns per canonization" << std::endl;
	}
};

NaiveCanonizationBenchmark<7> naiveCanonize;
