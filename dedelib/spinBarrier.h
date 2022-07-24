#pragma once

#include <atomic>
#include <stdint.h>

template<unsigned int ThreadCount = 2>
class SpinBarrier {
	std::atomic<uint32_t> arriveCounts[ThreadCount];
public:
	SpinBarrier() {
		for(unsigned int i = 0; i < ThreadCount; i++) {
			arriveCounts[i].store(0);
		}
	}

	void wait(unsigned int threadI) {
		int myArriveCount = arriveCounts[threadI].fetch_add(1);
		for(unsigned int i = 0; i < ThreadCount; i++) {
			if(i == threadI) continue;
			while(arriveCounts[i].load() == myArriveCount);
		}
	}
};
