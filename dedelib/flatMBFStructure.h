#pragma once

#include <stddef.h>
#include <memory.h>
#include <thread>

#include "funcTypes.h"
#include "knownData.h"

typedef uint32_t NodeIndex;
typedef uint32_t NodeOffset;

struct ClassInfo {
	uint64_t intervalSizeDown : 48; // log2(2414682040998) == 41.1349703699
	uint64_t classSize : 16; // log2(5040) == 12.2992080184
};

struct FlatNode {
	uint64_t dual : 30; // log2(490013148) == 28.8682452191
	uint64_t downLinks : 34; // log2(7329014832) == 32.770972138
	// this second downLinks index marks a list going from this->downLinks to (this+1)->downLinks
};

struct ProcessedPCoeffSum {
	uint64_t pcoeffSum : 48; // log2(2^35 * 5040) = 47.2992080184 bits
	uint64_t pcoeffCount : 16; // log2(5040) = 12.2992080184 bits
};

template<unsigned int Variables>
class FlatMBFStructure {
public:
	constexpr static size_t MBF_COUNT = mbfCounts[Variables];
	constexpr static size_t LINK_COUNT = getTotalLinkCount<Variables>();
	
	// sized MBF_COUNT == mbfCounts[Variables]
	Monotonic<Variables>* mbfs;
	ClassInfo* allClassInfos;
	FlatNode* allNodes;

	// sized LINK_COUNT == getTotalLinkCount<Variables>()
	NodeOffset* allLinks;

	struct CachedOffsets {
		// these help us to find the nodes of each layer
		NodeIndex nodeLayerOffsets[(1 << Variables) + 2];

		constexpr CachedOffsets() : nodeLayerOffsets{} {
			nodeLayerOffsets[0] = 0;
			for(size_t i = 0; i <= (1 << Variables); i++) {
				nodeLayerOffsets[i+1] = nodeLayerOffsets[i] + getLayerSize<Variables>(i);
			}
		}

		constexpr NodeIndex operator[](size_t i) const {return nodeLayerOffsets[i];}
	};
	constexpr static CachedOffsets cachedOffsets = CachedOffsets();

	
	FlatMBFStructure(bool enableMBFs = true, bool enableAllClassInfos = true, bool enableAllNodes = true, bool enableAllLinks = true):
		mbfs(nullptr),
		allClassInfos(nullptr),
		allNodes(nullptr),
		allLinks(nullptr) {
		/*
		these allocations are very large and happen upon initialization
		if we split the total work over several thousand jobs then this 
		means a lot of allocations, meaning a lot of overhead. malloc is
		faster than new. These buffers will be read in by a raw file read
		anyway. 
		*/
		if(enableMBFs) mbfs = (Monotonic<Variables>*) malloc(sizeof(Monotonic<Variables>) * MBF_COUNT);
		if(enableAllClassInfos) allClassInfos = (ClassInfo*) malloc(sizeof(ClassInfo) * (MBF_COUNT));
		if(enableAllLinks) allLinks = (NodeOffset*) malloc(sizeof(NodeOffset) * LINK_COUNT);

		if(enableAllNodes) {
			allNodes = (FlatNode*) malloc(sizeof(FlatNode) * (MBF_COUNT + 1));
			// add tails of the buffers, since we use the differences between the current and next elements to mark lists
			//allNodes[mbfCounts[Variables]].dual = 0xFFFFFFFFFFFFFFFF; // invalid
			allNodes[mbfCounts[Variables]].downLinks = LINK_COUNT; // end of the allLinks buffer
		}
	}
	~FlatMBFStructure() {
		if(mbfs) free(mbfs);
		if(allClassInfos) free(allClassInfos);
		if(allNodes) free(allNodes);
		if(allLinks) free(allLinks);
	}
	FlatMBFStructure(const FlatMBFStructure&) = delete;
	FlatMBFStructure& operator=(const FlatMBFStructure&) = delete;

	FlatMBFStructure(FlatMBFStructure&& other) noexcept :
		mbfs(other.mbfs), 
		allClassInfos(other.allClassInfos), 
		allNodes(other.allNodes), 
		allLinks(other.allLinks) {

		other.mbfs = nullptr;
		other.allClassInfos = nullptr;
		other.allNodes = nullptr;
		other.allLinks = nullptr;
	}
	FlatMBFStructure& operator=(FlatMBFStructure&& other) noexcept {
		this->mbfs = other.mbfs;
		this->allClassInfos = other.allClassInfos;
		this->allNodes = other.allNodes;
		this->allLinks = other.allLinks;

		other.mbfs = nullptr;
		other.allClassInfos = nullptr;
		other.allNodes = nullptr;
		other.allLinks = nullptr;
	}
};


template<unsigned int Variables>
void writeFlatMBFStructure(const FlatMBFStructure<Variables>& structure) {
	std::ofstream mbfsFile(FileName::flatMBFs(Variables), std::ios::binary);
	mbfsFile.write(reinterpret_cast<const char*>(structure.mbfs), sizeof(Monotonic<Variables>) * FlatMBFStructure<Variables>::MBF_COUNT);
	mbfsFile.close();
	std::ofstream allClassInfos(FileName::flatClassInfo(Variables), std::ios::binary);
	allClassInfos.write(reinterpret_cast<const char*>(structure.allClassInfos), sizeof(ClassInfo) * FlatMBFStructure<Variables>::MBF_COUNT);
	allClassInfos.close();
	std::ofstream allNodesFile(FileName::flatNodes(Variables), std::ios::binary);
	allNodesFile.write(reinterpret_cast<const char*>(structure.allNodes), sizeof(FlatNode) * FlatMBFStructure<Variables>::MBF_COUNT);
	allNodesFile.close();
	std::ofstream allLinksFile(FileName::flatLinks(Variables), std::ios::binary);
	allLinksFile.write(reinterpret_cast<const char*>(structure.allLinks), sizeof(NodeOffset) * FlatMBFStructure<Variables>::LINK_COUNT);
	allLinksFile.close();
}

template<unsigned int Variables>
FlatMBFStructure<Variables> readFlatMBFStructure(bool enableMBFs = true, bool enableAllClassInfos = true, bool enableAllNodes = true, bool enableAllLinks = true) {
	FlatMBFStructure<Variables> structure(enableMBFs, enableAllClassInfos, enableAllNodes, enableAllLinks);

	// read files in parallel
	std::thread mbfsThread;
	std::thread allClassInfosThread;
	std::thread allNodesThread;

	if(enableMBFs) mbfsThread = std::thread([&structure](){
		std::ifstream mbfsFile(FileName::flatMBFs(Variables), std::ios::binary);
		mbfsFile.read(reinterpret_cast<char*>(structure.mbfs), sizeof(Monotonic<Variables>) * FlatMBFStructure<Variables>::MBF_COUNT);
		mbfsFile.close();
	});
	if(enableAllClassInfos) allClassInfosThread = std::thread([&structure](){
		std::ifstream allClassInfos(FileName::flatClassInfo(Variables), std::ios::binary);
		allClassInfos.read(reinterpret_cast<char*>(structure.allClassInfos), sizeof(ClassInfo) * FlatMBFStructure<Variables>::MBF_COUNT);
		allClassInfos.close();
	});
	if(enableAllNodes) allNodesThread = std::thread([&structure](){
		std::ifstream allNodesFile(FileName::flatNodes(Variables), std::ios::binary);
		allNodesFile.read(reinterpret_cast<char*>(structure.allNodes), sizeof(FlatNode) * FlatMBFStructure<Variables>::MBF_COUNT);
		allNodesFile.close();
	});
	if(enableAllLinks) {
		std::ifstream allLinksFile(FileName::flatLinks(Variables), std::ios::binary);
		allLinksFile.read(reinterpret_cast<char*>(structure.allLinks), sizeof(NodeOffset) * FlatMBFStructure<Variables>::LINK_COUNT);
		allLinksFile.close();
	}
	if(enableMBFs) mbfsThread.join();
	if(enableAllClassInfos) allClassInfosThread.join();
	if(enableAllNodes) allNodesThread.join();

	return structure;
}




