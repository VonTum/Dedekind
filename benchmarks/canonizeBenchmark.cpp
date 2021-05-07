#include "benchmark.h"

#include "../dedelib/fileNames.h"
#include "../dedelib/booleanFunction.h"
#include "../dedelib/generators.h"
#include "../dedelib/toString.h"
#include "../dedelib/pcoeff.h"


constexpr static int ITERS = 1000;

template<unsigned int Variables>
class CanonizationBenchmark : public Benchmark {
public:
	CanonizationBenchmark() : Benchmark("canonizationBenchmark") {}

	std::vector<std::pair<Monotonic<Variables>, IntervalSymmetry>> data;

	virtual void init() override {
		std::ifstream benchmarkSetFile(FileName::benchmarkSet(Variables), std::ios::binary);
		if(!benchmarkSetFile.is_open()) throw "File not opened!";

		while(benchmarkSetFile.good()) {
			Monotonic<Variables> mbf = deserializeMBF<Variables>(benchmarkSetFile);
			IntervalSymmetry is = deserializeExtraData(benchmarkSetFile);
			data.push_back(std::make_pair(mbf, is));
			if(data.size() > 10000000) break;
		}
	}

	virtual void run() override {
		BooleanFunction<Variables> totalbf;
		for(size_t i = 0; i < data.size(); i++) {
			const Monotonic<Variables>& workingOn = this->data[i].first;
			Monotonic<Variables> bestBF = workingOn.canonize();
			
			totalbf ^= bestBF.bf;
		}
		std::cout << totalbf;
	}

	virtual void printResults(double deltaTimeMS) {
		double microsPerMBF = deltaTimeMS * 1000000.0 / data.size();
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
