#include "commands.h"

#include "../dedelib/bottomBufferCreator.h"
#include "../dedelib/flatBufferManagement.h"
#include "../dedelib/fileNames.h"

#include "../dedelib/flatPCoeffProcessing.h"

#include <iostream>
#include <random>
#include <cstring>

inline void benchmarkBottomBufferProduction(const std::vector<std::string>& args) {
	unsigned int Variables = std::stoi(args[0]);
	int sampleCount = std::stoi(args[1]);
	int threadCount = std::stoi(args[2]);

	const FlatNode* flatNodes = readFlatBuffer<FlatNode>(FileName::flatNodes(Variables), mbfCounts[Variables] + 1);
	auto tops = generateRangeSample(Variables, sampleCount);
	auto jobTops = convertTopInfos(flatNodes, tops);

	PCoeffProcessingContext context(Variables);
	//context.outputQueue.close(); // Don't use output queue

	std::cout << "Files loaded. Starting benchmark." << std::endl;

	std::thread loopBack([&](){
		auto startTime = std::chrono::high_resolution_clock::now();
		size_t bufferI = 0;
		size_t queueIdxRotator = 0;
		while(true) {
			auto optBuf = context.inputQueue.pop_wait_rotate(queueIdxRotator);
			if(optBuf.has_value()) {
				double secondsSinceStart = ((std::chrono::high_resolution_clock::now() - startTime).count() * 1.0e-9);
				std::cout << "Buffer " << bufferI++ << " received at " << secondsSinceStart << "s" << std::endl;
				context.free(optBuf.value().bufStart);
			} else {
				break;
			}
		}
	});
	runBottomBufferCreator(Variables, jobTops, context, threadCount);

	loopBack.join();
}

template<unsigned int Variables>
inline void testBottomBufferProduction(const std::vector<std::string>& args) {
	int sampleCount = std::stoi(args[0]);
	int threadCount = std::stoi(args[1]);
	int loopBackThreadCount = std::stoi(args[2]);

	const FlatNode* flatNodes = readFlatBuffer<FlatNode>(FileName::flatNodes(Variables), mbfCounts[Variables] + 1);
	const Monotonic<Variables>* mbfs = readFlatBuffer<Monotonic<Variables>>(FileName::flatMBFs(Variables), mbfCounts[Variables]);
	std::vector<NodeIndex> tops;
	std::default_random_engine generator;
	std::uniform_int_distribution<NodeIndex> dist(0, mbfCounts[Variables] - 1);
	std::cout << "Tops: ";
	for(int i = 0; i < sampleCount; i++) {
		NodeIndex newTop = dist(generator);
		std::cout << newTop << ',';
		tops.push_back(newTop);
	}
	std::cout << std::endl;
	auto jobTops = convertTopInfos(flatNodes, tops);

	PCoeffProcessingContext context(Variables);
	//context.outputQueue.close(); // Don't use output queue

	std::cout << "Files loaded. Starting test." << std::endl;

	std::vector<std::thread> loopBackThreads;
	loopBackThreads.reserve(loopBackThreadCount);
	for(int i = 0; i < loopBackThreadCount; i++) {
		std::thread loopBack([&](){
			bool* foundBottoms = new bool[mbfCounts[Variables]];
			size_t queueIdxRotator = 0;
			while(true) {
				std::memset(foundBottoms, 0, mbfCounts[Variables]*sizeof(bool));
				auto optBuf = context.inputQueue.pop_wait_rotate(queueIdxRotator);
				if(optBuf.has_value()) {
					auto buf = optBuf.value();

					NodeIndex top = buf.getTop();
					NodeIndex topDual = flatNodes[top].dual;
					std::cout << "Top=" << top << " received! Dual=" << topDual << " bufStart= " << (void*) buf.bufStart << " bufEnd=" << (buf.bufEnd - buf.bufStart) << " blockEnd=" << (buf.blockEnd - buf.bufStart) << std::endl;
					

					if(loopBackThreadCount == 1) {
						size_t sampleSize = std::min<size_t>(buf.bufferSize(), 50);
						NodeIndex selectedSampleStart = std::uniform_int_distribution<NodeIndex>(0, buf.bufferSize() - sampleSize)(generator);
						std::cout << "Random buffer sample at offset " << selectedSampleStart << ": " << std::endl;
						/*for(size_t i = 0; i < sampleSize; i+=2) {
							std::cout << buf.bufStart[selectedSampleStart + i] << ' ';
						}
						std::cout << std::endl;
						for(size_t i = 1; i < sampleSize; i+=2) {
							std::cout << buf.bufStart[selectedSampleStart + i] << ' ';
						}
						std::cout << std::endl;*/
						for(size_t i = 0; i < sampleSize; i++) {
							std::cout << buf.bufStart[selectedSampleStart + i] << ' ';
						}
						std::cout << std::endl;
					}

					for(NodeIndex& i : buf) {
						if(foundBottoms[i]) {
							std::cout << "  \033[31mERROR: Duplicate bottom " << i << " at position " << (&i - buf.bufStart) << std::endl;
						}
						foundBottoms[i] = true;
					}

					NodeIndex checkUpTo = top;
	#ifdef PCOEFF_DEDUPLICATE
					if(topDual < checkUpTo) checkUpTo = topDual;
	#endif

					Monotonic<Variables> topMBF = mbfs[top];
					for(NodeIndex i = 0; i < checkUpTo; i++) {
						Monotonic<Variables> botMBF = mbfs[i];

						bool correctBottom = hasPermutationBelow(topMBF, botMBF);

						if(foundBottoms[i] != correctBottom) {
							std::cout << "  \033[31mERROR: Incorrect Bottom " << i << (correctBottom ? " should have been included!" : " should NOT have been included!") << std::endl;
						}
					}
					for(NodeIndex i = checkUpTo; i < mbfCounts[Variables]; i++) {
						if(i == top) continue;
	#ifdef PCOEFF_DEDUPLICATE
						if(i == topDual) continue;
	#endif
						if(foundBottoms[i]) {
							std::cout << "  \033[31mERROR: Bottom " << i << " is present but shouldn't be! Above Top/Dual" << std::endl;
						}
					}

					context.free(buf.bufStart);
				} else {
					break;
				}
			}
		});
		loopBackThreads.push_back(std::move(loopBack));
	}
	runBottomBufferCreator(Variables, jobTops, context, threadCount);

	for(std::thread& t : loopBackThreads) {
		t.join();
	}
}


CommandSet bottomBufferCommands {"Bottom Buffer Commands", {}, {
	{"benchmarkBottomBufferProduction", benchmarkBottomBufferProduction},
	{"testBottomBufferProduction1", testBottomBufferProduction<1>},
	{"testBottomBufferProduction2", testBottomBufferProduction<2>},
	{"testBottomBufferProduction3", testBottomBufferProduction<3>},
	{"testBottomBufferProduction4", testBottomBufferProduction<4>},
	{"testBottomBufferProduction5", testBottomBufferProduction<5>},
	{"testBottomBufferProduction6", testBottomBufferProduction<6>},
	{"testBottomBufferProduction7", testBottomBufferProduction<7>},
}};

