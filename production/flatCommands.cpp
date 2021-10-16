#include "commands.h"

#include "../dedelib/flatPCoeff.h"



template<unsigned int Variables>
void isEvenPlus2() {
	FlatMBFStructure<Variables> allMBFs = readFlatMBFStructure<Variables>(false, true, true, false);

	bool isEven = true; // 0 is even
	for(NodeIndex i = 0; i < mbfCounts[Variables]; i++) {
		uint64_t classSize = allMBFs.allClassInfos[i].classSize;

		if(classSize % 2 == 0) continue;

		uint64_t intervalSizeDown = allMBFs.allClassInfos[i].intervalSizeDown;
		if(intervalSizeDown % 2 == 0) continue;

		NodeIndex dualI = allMBFs.allNodes[i].dual;
		uint64_t intervalSizeUp = allMBFs.allClassInfos[dualI].intervalSizeDown;
		if(intervalSizeUp % 2 == 0) continue;

		isEven = !isEven;
	}

	std::cout << "D(" << (Variables + 2) << ") is " << (isEven ? "even" : "odd") << std::endl;
}

CommandSet flatCommands {"Flat DataStructure", {
	{"flatDPlusOne1", []() {flatDPlus1<1>(); }},
	{"flatDPlusOne2", []() {flatDPlus1<2>(); }},
	{"flatDPlusOne3", []() {flatDPlus1<3>(); }},
	{"flatDPlusOne4", []() {flatDPlus1<4>(); }},
	{"flatDPlusOne5", []() {flatDPlus1<5>(); }},
	{"flatDPlusOne6", []() {flatDPlus1<6>(); }},
	{"flatDPlusOne7", []() {flatDPlus1<7>(); }},

	{"flatDPlusTwo1", []() {flatDPlus2<1, 32>(); }},
	{"flatDPlusTwo2", []() {flatDPlus2<2, 32>(); }},
	{"flatDPlusTwo3", []() {flatDPlus2<3, 32>(); }},
	{"flatDPlusTwo4", []() {flatDPlus2<4, 32>(); }},
	{"flatDPlusTwo5", []() {flatDPlus2<5, 32>(); }},
	{"flatDPlusTwo6", []() {flatDPlus2<6, 32>(); }},
	{"flatDPlusTwo7", []() {flatDPlus2<7, 32>(); }},
	
	{"isEvenPlusTwo1", []() {isEvenPlus2<1>(); }},
	{"isEvenPlusTwo2", []() {isEvenPlus2<2>(); }},
	{"isEvenPlusTwo3", []() {isEvenPlus2<3>(); }},
	{"isEvenPlusTwo4", []() {isEvenPlus2<4>(); }},
	{"isEvenPlusTwo5", []() {isEvenPlus2<5>(); }},
	{"isEvenPlusTwo6", []() {isEvenPlus2<6>(); }},
	{"isEvenPlusTwo7", []() {isEvenPlus2<7>(); }},
}, {}};
