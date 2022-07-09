#include "commands.h"

#include "../dedelib/bufferedMap.h"
#include "../dedelib/funcTypes.h"
#include "../dedelib/fullIntervalSizeComputation.h"
#include "../dedelib/intervalAndSymmetriesMap.h"
#include "../dedelib/flatMBFStructure.h"

#include "../dedelib/timeTracker.h"
#include "../dedelib/fileNames.h"

#include "../dedelib/MBFDecomposition.h"

#include <random>
#include "../dedelib/generators.h"

#include <cstring>


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
		assert(curNodeIndex == flatNodeLayerOffsets[Variables][layer]);

		NodeIndex firstNodeInDualLayer = flatNodeLayerOffsets[Variables][(1 << Variables) - layer];
		const BakedMap<Monotonic<Variables>, ExtraData>& curLayer = sourceMap.layers[layer];
		const BakedMap<Monotonic<Variables>, ExtraData>& dualLayer = sourceMap.layers[(1 << Variables) - layer];
		for(size_t i = 0; i < layerSizes[Variables][layer]; i++) {
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
	assert(currentLinkInLayer == getTotalLinkCount(Variables));
	// add tails of the buffers, since we use the differences between the current and next elements to mark lists
	//allNodes[mbfCounts[Variables]].dual = 0xFFFFFFFFFFFFFFFF; // invalid
	allNodes[mbfCounts[Variables]].downLinks = getTotalLinkCount(Variables); // end of the allLinks buffer

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

void convertFlatMBFStructureToSourceMBFStructure(unsigned int Variables) {
	const FlatNode* allNodes = readFlatBuffer<FlatNode>(FileName::flatNodes(Variables), mbfCounts[Variables] + 1);
	const uint32_t* allLinks = readFlatBuffer<uint32_t>(FileName::flatLinks(Variables), getTotalLinkCount(Variables));
	
	std::unique_ptr<uint32_t[]> resultingFile(new uint32_t[getTotalLinkCount(Variables)]);
	uint32_t* curItemInFile = resultingFile.get();

	std::cout << "Link count distribution: " << std::endl;

	size_t numberOfLayers = (1 << Variables) + 1;

	size_t maxLayerWidth = getMaxLayerWidth(Variables);
	std::unique_ptr<uint32_t[]> incomingLinks(new uint32_t[getMaxLayerSize(Variables) * maxLayerWidth]);
	std::unique_ptr<int[]> incomingLinksSizes(new int[getMaxLayerSize(Variables)]);
	for(size_t fromLayerI = numberOfLayers-1; fromLayerI > 0; fromLayerI--) {
		size_t toLayerI = fromLayerI - 1;
		std::cout << "To Layer " << toLayerI << std::endl;
		size_t fromLayerSize = layerSizes[Variables][fromLayerI];
		size_t toLayerSize = layerSizes[Variables][toLayerI];
		std::memset(incomingLinksSizes.get(), 0, toLayerSize * sizeof(int));

		size_t linksInThisLayer = 0;

		size_t layerOffset = flatNodeLayerOffsets[Variables][fromLayerI];

		const FlatNode* firstFromNode = allNodes + layerOffset;
		for(uint32_t i = 0; i < fromLayerSize; i++) {
			uint64_t downLinksStart = firstFromNode[i].downLinks;
			uint64_t downLinksEnd = firstFromNode[i+1].downLinks;

			for(const NodeOffset* curDownlink = allLinks + downLinksStart; curDownlink != allLinks + downLinksEnd; curDownlink++) {
				NodeOffset targetNodeI = *curDownlink;
				assert(targetNodeI < toLayerSize);
				incomingLinks[targetNodeI * maxLayerWidth + incomingLinksSizes[targetNodeI]++] = i;
				linksInThisLayer++;
			}
		}

		std::cout << linksInThisLayer << ", ";

		for(size_t i = 0; i < toLayerSize; i++) {
			const uint32_t* curLinks = incomingLinks.get() + maxLayerWidth * i;
			int curLinksSize = incomingLinksSizes[i];
			for(int i = 0; i < curLinksSize - 1; i++) {
				*curItemInFile++ = curLinks[i];
			}
			*curItemInFile++ = curLinks[curLinksSize - 1] | static_cast<uint32_t>(0x80000000); // Mark end of links
		}
	}

	if(curItemInFile != resultingFile.get() + getTotalLinkCount(Variables)) {
		std::cerr << "Invalid file! Not the correct number of links!" << std::endl;
		std::terminate();
	}

	{
		std::ofstream sourceLinkFile(FileName::mbfStructure(Variables), std::ios::binary);
		sourceLinkFile.write(reinterpret_cast<const char*>(resultingFile.get()), sizeof(uint32_t) * getTotalLinkCount(Variables));
	} // Close file
}

void randomizeMBF_LUT7() {
	constexpr unsigned int Variables = 7;

	FlatMBFStructure<Variables> allMBFData = readFlatMBFStructure<Variables>(true, false, false, false);

	uint64_t* mbfsUINT64 = (uint64_t*) aligned_malloc(mbfCounts[7]*sizeof(Monotonic<7>), 4096);

	std::mt19937 generator;
	for(size_t i = 0; i < mbfCounts[Variables]; i++) {
		Monotonic<Variables> mbf = allMBFData.mbfs[i];
		permuteRandom(mbf, generator);
		uint64_t upper = _mm_extract_epi64(mbf.bf.bitset.data, 1);
		uint64_t lower = _mm_extract_epi64(mbf.bf.bitset.data, 0);
		mbfsUINT64[i*2] = upper;
		mbfsUINT64[i*2+1] = lower;

		if(i % 100000 == 0) std::cout << i << "/" << mbfCounts[Variables] << std::endl;
	}

	writeFlatBuffer(mbfsUINT64, FileName::flatMBFsU64(Variables), FlatMBFStructure<Variables>::MBF_COUNT*2);

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
	convertFlatMBFStructureToSourceMBFStructure(Variables);
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

	{"convertFlatMBFStructureToSourceMBFStructure1", []() {convertFlatMBFStructureToSourceMBFStructure(1); }},
	{"convertFlatMBFStructureToSourceMBFStructure2", []() {convertFlatMBFStructureToSourceMBFStructure(2); }},
	{"convertFlatMBFStructureToSourceMBFStructure3", []() {convertFlatMBFStructureToSourceMBFStructure(3); }},
	{"convertFlatMBFStructureToSourceMBFStructure4", []() {convertFlatMBFStructureToSourceMBFStructure(4); }},
	{"convertFlatMBFStructureToSourceMBFStructure5", []() {convertFlatMBFStructureToSourceMBFStructure(5); }},
	{"convertFlatMBFStructureToSourceMBFStructure6", []() {convertFlatMBFStructureToSourceMBFStructure(6); }},
	{"convertFlatMBFStructureToSourceMBFStructure7", []() {convertFlatMBFStructureToSourceMBFStructure(7); }},

	{"randomizeMBF_LUT7", []() {randomizeMBF_LUT7();}},

	{"preCompute1", []() {preComputeFiles<1>(); }},
	{"preCompute2", []() {preComputeFiles<2>(); }},
	{"preCompute3", []() {preComputeFiles<3>(); }},
	{"preCompute4", []() {preComputeFiles<4>(); }},
	{"preCompute5", []() {preComputeFiles<5>(); }},
	{"preCompute6", []() {preComputeFiles<6>(); }},
	{"preCompute7", []() {preComputeFiles<7>(); }},
}, {}};
