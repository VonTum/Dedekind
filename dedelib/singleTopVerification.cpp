#include "singleTopVerification.h"
#include "knownData.h"
#include "numaMem.h"
#include "flatBufferManagement.h"
#include "fileNames.h"
#include "threadUtils.h"
#include "threadPool.h"
#include "resultCollection.h"
#include <pthread.h>

SingleTopPThreadData::SingleTopPThreadData(unsigned int Variables) : Variables(Variables) {
	size_t mbfSize = (1 << (Variables > 3 ? Variables-3 : 0)); // sizeof(Monotonic<Variables>)
	size_t mbfBufSize = mbfSize * mbfCounts[Variables];

	std::cout << "Loading MBFs...\n" << std::flush;
	void* numaMBFs = numa_alloc_interleaved(mbfBufSize);
	readFlatVoidBufferNoMMAP(FileName::flatMBFs(Variables), mbfBufSize, numaMBFs);
	std::cout << "MBFs loaded\nLoading ClassInfos...\n" << std::flush;
	void* numaClassInfos = numa_alloc_interleaved(sizeof(ClassInfo) * mbfCounts[Variables]);
	readFlatVoidBufferNoMMAP(FileName::flatClassInfo(Variables), sizeof(ClassInfo) * mbfCounts[Variables], numaClassInfos);
	std::cout << "ClassInfos Loaded\n" << std::flush;

	mbfLUT = numaMBFs;
	classInfos = static_cast<const ClassInfo*>(numaClassInfos);
}

void SingleTopPThreadData::run(NodeIndex topIdx, void*(*func)(void*)) {
	this->topIdx = topIdx;

	void* flatNodes = mmapFlatVoidBuffer(FileName::flatNodes(Variables), mbfCounts[Variables] * sizeof(FlatNode));
	this->topDual = ((const FlatNode*) flatNodes)[topIdx].dual;
	munmapFlatVoidBuffer(flatNodes, mbfCounts[Variables] * sizeof(FlatNode));

	std::cout << "Top dual " + std::to_string(topDual) + "\n" << std::endl;

	curIndex.store(0);
	this->result.resultSum.betaSum = 0;
	this->result.resultSum.countedIntervalSizeDown = 0;
	this->result.dualSum.betaSum = 0;
	this->result.dualSum.countedIntervalSizeDown = 0;
	this->result.validationSum.betaSum = 0;
	this->result.validationSum.countedIntervalSizeDown = 0;

	PThreadBundle allThreads = allCoresSpread(this, func);
	allThreads.join();
}

SingleTopPThreadData::~SingleTopPThreadData() {
	size_t mbfSize = (1 << (Variables > 3 ? Variables-3 : 0)); // sizeof(Monotonic<Variables>)
	size_t mbfBufSize = mbfSize * mbfCounts[Variables];

	numa_free(const_cast<void*>(mbfLUT), mbfBufSize);
	numa_free(const_cast<ClassInfo*>(classInfos), sizeof(ClassInfo) * mbfCounts[Variables]);
}
