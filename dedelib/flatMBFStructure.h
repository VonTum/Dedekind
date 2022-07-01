#pragma once

#include <stddef.h>
#include <thread>

#include "funcTypes.h"
#include "knownData.h"

#include "flatBufferManagement.h"

#include "fileNames.h"

#include "jobInfo.h"

typedef uint32_t NodeOffset;
typedef uint64_t LinkIndex;

struct ClassInfo {
	uint64_t intervalSizeDown : 48; // log2(2414682040998) == 41.1349703699
	uint64_t classSize : 16; // log2(5040) == 12.2992080184
};

struct FlatNode {
	// dual could be NodeOffset instead of NodeIndex, Requiring only log2(16440466) == 23.9707478566
	/*NodeIndex*/ uint64_t dual : 30; // log2(490013148) == 28.8682452191
	/*LinkIndex*/ uint64_t downLinks : 34; // log2(7329014832) == 32.770972138
	// this second downLinks index marks a list going from this->downLinks to (this+1)->downLinks
};

template<unsigned int Variables>
class FlatMBFStructure {
public:
	constexpr static size_t MBF_COUNT = mbfCounts[Variables];
	constexpr static size_t LINK_COUNT = getTotalLinkCount(Variables);
	
	
	const Monotonic<Variables>* mbfs; // sized MBF_COUNT == mbfCounts[Variables]
	const ClassInfo* allClassInfos; // sized MBF_COUNT == mbfCounts[Variables]
	const FlatNode* allNodes; // sized MBF_COUNT+1 == mbfCounts[Variables]+1
	const NodeOffset* allLinks; // sized LINK_COUNT == getTotalLinkCount<Variables>()

	bool useFlatBufferManagement = false;

	// these are written by either the generation code, or from a file read / memory map
	FlatMBFStructure() : mbfs(nullptr), allClassInfos(nullptr), allNodes(nullptr), allLinks(nullptr) {}

	~FlatMBFStructure() {
		if(useFlatBufferManagement) {
			if(mbfs) freeFlatBuffer(mbfs, MBF_COUNT);
			if(allClassInfos) freeFlatBuffer(allClassInfos, MBF_COUNT);
			if(allNodes) freeFlatBuffer(allNodes, MBF_COUNT + 1);
			if(allLinks) freeFlatBuffer(allLinks, LINK_COUNT);
		} else {
			if(mbfs) free_const(mbfs);
			if(allClassInfos) free_const(allClassInfos);
			if(allNodes) free_const(allNodes);
			if(allLinks) free_const(allLinks);
		}
	}
	FlatMBFStructure(const FlatMBFStructure&) = delete;
	FlatMBFStructure& operator=(const FlatMBFStructure&) = delete;

	FlatMBFStructure(FlatMBFStructure&& other) noexcept :
		mbfs(other.mbfs), 
		allClassInfos(other.allClassInfos), 
		allNodes(other.allNodes), 
		allLinks(other.allLinks),
		useFlatBufferManagement(other.useFlatBufferManagement) {

		other.mbfs = nullptr;
		other.allClassInfos = nullptr;
		other.allNodes = nullptr;
		other.allLinks = nullptr;
		other.useFlatBufferManagement = false;
	}
	FlatMBFStructure& operator=(FlatMBFStructure&& other) noexcept {
		std::swap(this->mbfs, other.mbfs);
		std::swap(this->allClassInfos, other.allClassInfos);
		std::swap(this->allNodes, other.allNodes);
		std::swap(this->allLinks, other.allLinks);
		std::swap(this->useFlatBufferManagement, other.useFlatBufferManagement);
	}
};

template<unsigned int Variables>
void writeFlatMBFStructure(const FlatMBFStructure<Variables>& structure) {
	writeFlatBuffer(structure.mbfs, FileName::flatMBFs(Variables), FlatMBFStructure<Variables>::MBF_COUNT);
	writeFlatBuffer(structure.allClassInfos, FileName::flatClassInfo(Variables), FlatMBFStructure<Variables>::MBF_COUNT);
	writeFlatBuffer(structure.allNodes, FileName::flatNodes(Variables), FlatMBFStructure<Variables>::MBF_COUNT + 1);
	writeFlatBuffer(structure.allLinks, FileName::flatLinks(Variables), FlatMBFStructure<Variables>::LINK_COUNT);
}

template<unsigned int Variables>
FlatMBFStructure<Variables> readFlatMBFStructure(bool enableMBFs = true, bool enableAllClassInfos = true, bool enableAllNodes = true, bool enableAllLinks = true) {
	FlatMBFStructure<Variables> structure;

	// read files in parallel
	std::thread mbfsThread;
	std::thread allClassInfosThread;
	std::thread allNodesThread;

	if(enableMBFs) mbfsThread = std::thread([&structure](){
		structure.mbfs = readFlatBuffer<Monotonic<Variables>>(FileName::flatMBFs(Variables), FlatMBFStructure<Variables>::MBF_COUNT);});
	if(enableAllClassInfos) allClassInfosThread = std::thread([&structure](){
		structure.allClassInfos = readFlatBuffer<ClassInfo>(FileName::flatClassInfo(Variables), FlatMBFStructure<Variables>::MBF_COUNT);});
	if(enableAllNodes) allNodesThread = std::thread([&structure](){
		structure.allNodes = readFlatBuffer<FlatNode>(FileName::flatNodes(Variables), FlatMBFStructure<Variables>::MBF_COUNT + 1);});
	if(enableAllLinks) 
		structure.allLinks = readFlatBuffer<NodeOffset>(FileName::flatLinks(Variables), FlatMBFStructure<Variables>::LINK_COUNT);

	structure.useFlatBufferManagement = true;

	if(enableMBFs) mbfsThread.join();
	if(enableAllClassInfos) allClassInfosThread.join();
	if(enableAllNodes) allNodesThread.join();

	return structure;
}
