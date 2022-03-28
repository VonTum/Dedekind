#include "commands.h"

#include "../dedelib/bufferedMap.h"
#include "../dedelib/funcTypes.h"
#include "../dedelib/fullIntervalSizeComputation.h"
#include "../dedelib/intervalAndSymmetriesMap.h"
#include "../dedelib/flatMBFStructure.h"

#include "../dedelib/timeTracker.h"
#include "../dedelib/fileNames.h"

#include "../dedelib/MBFDecomposition.h"


template<unsigned int Variables>
void runGenAllMBFs() {
	std::pair<BufferedSet<Monotonic<Variables>>, size_t> resultPair;
	{
		TimeTracker timer;
		resultPair = generateAllMBFsFast<Variables>();
	}
	BufferedSet<Monotonic<Variables>>& result = resultPair.first;
	size_t numberOfLinks = resultPair.second;
	{
		std::cout << "Sorting\n";
		TimeTracker timer;
		std::sort(result.begin(), result.end(), [](Monotonic<Variables>& a, Monotonic<Variables>& b) -> bool {return a.size() < b.size(); });
	}


	uint64_t sizeCounts[(1 << Variables) + 1];
	for(size_t i = 0; i < (1 << Variables) + 1; i++) {
		sizeCounts[i] = 0;
	}
	for(const Monotonic<Variables>& item : result) {
		sizeCounts[item.size()]++;
	}

	std::cout << "R(" << Variables << ") == " << result.size() << "\n";

	{
		std::ofstream file(FileName::allMBFS(Variables), std::ios::binary);

		for(const Monotonic<Variables>& item : result) {
			serializeMBF(item, file);
		}
		file.close();
	}

	{
		std::ofstream file(FileName::allMBFSInfo(Variables));


		file << "R(" << Variables << ") == " << result.size() << "\n";

		std::cout << "numberOfLinks: " << numberOfLinks << "\n";
		file << "numberOfLinks: " << numberOfLinks << "\n";

		//std::cout << "classesPerSize: " << sizeCounts[0];
		file << "classesPerSize: " << sizeCounts[0];
		for(size_t i = 1; i < (1 << Variables) + 1; i++) {
			//std::cout << ", " << sizeCounts[i] << "\n";
			file << ", " << sizeCounts[i];
		}
		file << "\n";
		file.close();
	}
}

template<unsigned int Variables>
void runSortAndComputeLinks() {
	TimeTracker timer;

	std::ifstream inputFile(FileName::allMBFS(Variables), std::ios::binary);
	std::ofstream sortedFile(FileName::allMBFSSorted(Variables), std::ios::binary);
	std::ofstream linkFile(FileName::mbfLinks(Variables), std::ios::binary);

	sortAndComputeLinks<Variables>(inputFile, sortedFile, linkFile);

	inputFile.close();
	sortedFile.close();
	linkFile.close();
}

template<unsigned int Variables>
FlatMBFStructure<Variables> convertMBFMapToFlatMBFStructure(const AllMBFMap<Variables, ExtraData>& sourceMap) {
	/*
	these allocations are very large and happen upon initialization
	if we split the total work over several thousand jobs then this 
	means a lot of allocations, meaning a lot of overhead. malloc is
	faster than new. These buffers will be read in by a raw file read
	anyway. 
	*/
	Monotonic<Variables>* mbfs = (Monotonic<Variables>*) malloc(sizeof(Monotonic<Variables>) * FlatMBFStructure<Variables>::MBF_COUNT);
	ClassInfo* allClassInfos = (ClassInfo*) malloc(sizeof(ClassInfo) * FlatMBFStructure<Variables>::MBF_COUNT);
	FlatNode* allNodes = (FlatNode*) malloc(sizeof(FlatNode) * (FlatMBFStructure<Variables>::MBF_COUNT + 1));
	NodeOffset* allLinks = (NodeOffset*) malloc(sizeof(NodeOffset) * FlatMBFStructure<Variables>::LINK_COUNT);
	
	size_t currentLinkInLayer = 0;
	NodeIndex curNodeIndex = 0;
	for(size_t layer = 0; layer <= (1 << Variables); layer++) {
		std::cout << "Layer " << layer << std::endl;
		assert(curNodeIndex == FlatMBFStructure<Variables>::cachedOffsets.nodeLayerOffsets[layer]);

		NodeIndex firstNodeInDualLayer = FlatMBFStructure<Variables>::cachedOffsets.nodeLayerOffsets[(1 << Variables) - layer];
		const BakedMap<Monotonic<Variables>, ExtraData>& curLayer = sourceMap.layers[layer];
		const BakedMap<Monotonic<Variables>, ExtraData>& dualLayer = sourceMap.layers[(1 << Variables) - layer];
		for(size_t i = 0; i < getLayerSize<Variables>(layer); i++) {
			const KeyValue<Monotonic<Variables>, ExtraData>& elem = curLayer[i];
			mbfs[curNodeIndex] = elem.key;
			allClassInfos[curNodeIndex].intervalSizeDown = elem.value.intervalSizeToBottom;
			allClassInfos[curNodeIndex].classSize = elem.value.symmetries;

			Monotonic<Variables> keyDual = elem.key.dual();
			allNodes[curNodeIndex].dual = firstNodeInDualLayer + dualLayer.indexOf(keyDual.canonize());
			if(layer > 0) { // no downconnections for layer 0
				DownConnection* curDownConnection = elem.value.downConnections;
				DownConnection* downConnectionsEnd = curLayer[i+1].value.downConnections;
				allNodes[curNodeIndex].downLinks = currentLinkInLayer;
				while(curDownConnection != downConnectionsEnd) {
					int downConnectionNodeIndex = static_cast<int>(curDownConnection->id);
					allLinks[currentLinkInLayer] = downConnectionNodeIndex;
					curDownConnection++;
					currentLinkInLayer++;
				}
			}
			curNodeIndex++;
		}
	}
	assert(currentLinkInLayer == getTotalLinkCount<Variables>());
	// add tails of the buffers, since we use the differences between the current and next elements to mark lists
	//allNodes[mbfCounts[Variables]].dual = 0xFFFFFFFFFFFFFFFF; // invalid
	allNodes[mbfCounts[Variables]].downLinks = getTotalLinkCount<Variables>(); // end of the allLinks buffer

	FlatMBFStructure<Variables> result;
	result.mbfs = mbfs;
	result.allClassInfos = allClassInfos;
	result.allNodes = allNodes;
	result.allLinks = allLinks;
	return result;
}

template<unsigned int Variables>
void convertMBFMapToFlatMBFStructure() {
	AllMBFMap<Variables, ExtraData> sourceMap = readAllMBFsMapExtraDownLinks<Variables>();

	FlatMBFStructure<Variables> destinationStructure = convertMBFMapToFlatMBFStructure<Variables>(sourceMap);

	writeFlatMBFStructure(destinationStructure);
}

#include <random>
#include "../dedelib/generators.h"

void randomizeMBF_LUT7() {
	constexpr unsigned int Variables = 7;

	FlatMBFStructure<Variables> allMBFData = readFlatMBFStructure<Variables>(false, false, true, false);

	uint64_t* mbfsUINT64 = (uint64_t*) aligned_malloc(mbfCounts[7]*sizeof(Monotonic<7>), 4096);

	std::default_random_engine generator;
	for(size_t i = 0; i < mbfCounts[Variables]; i++) {
		Monotonic<Variables> mbf = allMBFData.mbfs[i];
		permuteRandom(mbf, generator);
		uint64_t upper = _mm_extract_epi64(mbf.bf.bitset.data, 1);
		uint64_t lower = _mm_extract_epi64(mbf.bf.bitset.data, 0);
		mbfsUINT64[i*2] = upper;
		mbfsUINT64[i*2+1] = lower;

		if(i % 100000 == 0) std::cout << i << "/" << mbfCounts[Variables] << std::endl;
	}

	writeFlatBuffer(mbfsUINT64, FileName::flatMBFsU64(Variables), FlatMBFStructure<Variables>::MBF_COUNT);

	aligned_free(mbfsUINT64);
}


template<unsigned int Variables>
void preComputeFiles() {
	runGenAllMBFs<Variables>();
	runSortAndComputeLinks<Variables>();
	computeIntervalsParallel<Variables>();
	//computeDPlus1<Variables>();
	addSymmetriesToIntervalFile<Variables>();
	convertMBFMapToFlatMBFStructure<Variables>();
	//flatDPlus1<Variables>();
}


CommandSet dataGenCommands {"Data Generation", {
	{"genAllMBF1", []() {runGenAllMBFs<1>(); }},
	{"genAllMBF2", []() {runGenAllMBFs<2>(); }},
	{"genAllMBF3", []() {runGenAllMBFs<3>(); }},
	{"genAllMBF4", []() {runGenAllMBFs<4>(); }},
	{"genAllMBF5", []() {runGenAllMBFs<5>(); }},
	{"genAllMBF6", []() {runGenAllMBFs<6>(); }},
	{"genAllMBF7", []() {runGenAllMBFs<7>(); }},
	{"genAllMBF8", []() {runGenAllMBFs<8>(); }},
	{"genAllMBF9", []() {runGenAllMBFs<9>(); }},

	{"sortAndComputeLinks1", []() {runSortAndComputeLinks<1>(); }},
	{"sortAndComputeLinks2", []() {runSortAndComputeLinks<2>(); }},
	{"sortAndComputeLinks3", []() {runSortAndComputeLinks<3>(); }},
	{"sortAndComputeLinks4", []() {runSortAndComputeLinks<4>(); }},
	{"sortAndComputeLinks5", []() {runSortAndComputeLinks<5>(); }},
	{"sortAndComputeLinks6", []() {runSortAndComputeLinks<6>(); }},
	{"sortAndComputeLinks7", []() {runSortAndComputeLinks<7>(); }},
	//{"sortAndComputeLinks8", []() {runSortAndComputeLinks<8>(); }},
	//{"sortAndComputeLinks9", []() {runSortAndComputeLinks<9>(); }},

	{"computeIntervalsParallel1", []() {computeIntervalsParallel<1>(); }},
	{"computeIntervalsParallel2", []() {computeIntervalsParallel<2>(); }},
	{"computeIntervalsParallel3", []() {computeIntervalsParallel<3>(); }},
	{"computeIntervalsParallel4", []() {computeIntervalsParallel<4>(); }},
	{"computeIntervalsParallel5", []() {computeIntervalsParallel<5>(); }},
	{"computeIntervalsParallel6", []() {computeIntervalsParallel<6>(); }},
	{"computeIntervalsParallel7", []() {computeIntervalsParallel<7>(); }},
	//{"computeIntervalsParallel8", []() {computeIntervalsParallel<8>(); }},
	//{"computeIntervalsParallel9", []() {computeIntervalsParallel<9>(); }},

	{"addSymmetriesToIntervalFile1", []() {addSymmetriesToIntervalFile<1>(); }},
	{"addSymmetriesToIntervalFile2", []() {addSymmetriesToIntervalFile<2>(); }},
	{"addSymmetriesToIntervalFile3", []() {addSymmetriesToIntervalFile<3>(); }},
	{"addSymmetriesToIntervalFile4", []() {addSymmetriesToIntervalFile<4>(); }},
	{"addSymmetriesToIntervalFile5", []() {addSymmetriesToIntervalFile<5>(); }},
	{"addSymmetriesToIntervalFile6", []() {addSymmetriesToIntervalFile<6>(); }},
	{"addSymmetriesToIntervalFile7", []() {addSymmetriesToIntervalFile<7>(); }},
	//{"addSymmetriesToIntervalFile8", []() {addSymmetriesToIntervalFile<8>(); }},
	//{"addSymmetriesToIntervalFile9", []() {addSymmetriesToIntervalFile<9>(); }},
	
	{"convertMBFMapToFlatMBFStructure1", []() {convertMBFMapToFlatMBFStructure<1>(); }},
	{"convertMBFMapToFlatMBFStructure2", []() {convertMBFMapToFlatMBFStructure<2>(); }},
	{"convertMBFMapToFlatMBFStructure3", []() {convertMBFMapToFlatMBFStructure<3>(); }},
	{"convertMBFMapToFlatMBFStructure4", []() {convertMBFMapToFlatMBFStructure<4>(); }},
	{"convertMBFMapToFlatMBFStructure5", []() {convertMBFMapToFlatMBFStructure<5>(); }},
	{"convertMBFMapToFlatMBFStructure6", []() {convertMBFMapToFlatMBFStructure<6>(); }},
	{"convertMBFMapToFlatMBFStructure7", []() {convertMBFMapToFlatMBFStructure<7>(); }},

	{"randomizeMBF_LUT7", []() {randomizeMBF_LUT7();}},

	{"preCompute1", []() {preComputeFiles<1>(); }},
	{"preCompute2", []() {preComputeFiles<2>(); }},
	{"preCompute3", []() {preComputeFiles<3>(); }},
	{"preCompute4", []() {preComputeFiles<4>(); }},
	{"preCompute5", []() {preComputeFiles<5>(); }},
	{"preCompute6", []() {preComputeFiles<6>(); }},
	{"preCompute7", []() {preComputeFiles<7>(); }},
}, {}};
