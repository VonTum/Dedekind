#include "benchmark.h"

#include "../dedelib/sjomn.h"

class ThreadCreateBenchmark : public Benchmark {
public:
	ThreadCreateBenchmark() : Benchmark("oldConnect") {}

	virtual void init() override {}

	virtual void run() override {
		for(int iter = 0; iter < 1000; iter++) {

		}
	}

	virtual void printResults(double timeTaken) override {}

} oldConnect;
