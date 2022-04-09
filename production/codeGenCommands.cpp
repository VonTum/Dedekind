#include "commands.h"


#include "../dedelib/collectionOperations.h"
#include "../dedelib/layerStack.h"
#include "../dedelib/toString.h"

#include <fstream>
#include <algorithm>


int groupCount = 9; // max group count
int bitsCount = 16; // max stack size
int maxSmallPermuteSize = 4; // number of permute functions with builtin impl



/*
case 1:
	bits9 = bits8; bits8 = bits7; bits7 = bits6; bits6 = bits5; bits5 = bits4; bits4 = bits3; bits3 = bits2; bits2 = bits1;
	bits1 = _mm256_and_si256(data, curMask);
	curMask = _mm256_slli_epi32(curMask, 1);
	permute1();
	curMask = _mm256_srli_epi32(curMask, 1);
	break;
case 2:
	bits9 = bits7; bits8 = bits6; bits7 = bits5; bits6 = bits4; bits5 = bits3; bits4 = bits2; bits3 = bits1;
	bits1 = _mm256_and_si256(data, curMask);
	bits2 = _mm256_and_si256(_mm256_srli_epi32(data, 2), curMask);
	curMask = _mm256_slli_epi32(curMask, 2);
	permute2();
	curMask = _mm256_srli_epi32(curMask, 2);
	break;
	...
*/

static void genPushStack(std::ostream& os, int bitsCount, int offset) {
	for(int bitsMover = bitsCount - 1; bitsMover - offset >= 0; bitsMover--) {
		os << "bits" << bitsMover << " = bits" << bitsMover - offset << "; ";
	}
	os << "\n";
}

static void genPopStack(std::ostream& os, int bitsCount, int offset) {
	for(int bitsMover = 0; bitsMover + offset < bitsCount; bitsMover++) {
		os << "bits" << bitsMover << " = bits" << bitsMover + offset << "; ";
	}
	os << "\n";
}

void genCodeForEquivClass() {
	std::ofstream os("equivClassCode.txt");

	os << "case " << 1 << ":\n";
	os << "\t";
	genPushStack(os, bitsCount, 1);
	os << "\tbits0 = _mm256_and_si256(data, curMask);\n";
	os << "\tcurMask = _mm256_slli_epi32(curMask, 1);\n";
	os << "\tif(permute1()) return true;\n";
	os << "\tcurMask = _mm256_srli_epi32(curMask, 1);\n";
	os << "\t";
	genPopStack(os, bitsCount, 1);
	os << "\tbreak;\n";

	for(int groupSize = 2; groupSize <= groupCount; groupSize++) {
		os << "case " << groupSize << ":\n";
		os << "\t";
		genPushStack(os, bitsCount, groupSize+1);
		if(groupSize <= maxSmallPermuteSize) {
			os << "\tbits0 = _mm256_sub_epi32(_mm256_slli_epi32(curMask, " << groupSize << "), curMask);\n";
		} else {
			os << "\tbits0 = _mm256_slli_epi32( _mm256_sub_epi32(_mm256_slli_epi32(curMask, " << maxSmallPermuteSize << "), curMask), " << groupSize - maxSmallPermuteSize << ");\n";
		}
		for(int j = 1; j < groupSize; j++) {
			os << "\tbits" << j << " = _mm256_slli_epi32( _mm256_and_si256(_mm256_srli_epi32(data, " << groupSize - j << "), curMask) , " << groupSize-1 << "); // TODO optimize, perhaps unneeded shift\n";
		}
		os << "\tbits" << groupSize << " = _mm256_slli_epi32( _mm256_and_si256(data, curMask) , " << groupSize - 1 << "); // TODO optimize, perhaps unneeded shift\n";
		os << "\tcurMask = _mm256_slli_epi32(curMask, " << groupSize << ");\n";
		os << "\tif(permute" << groupSize << "()) return true;\n";
		os << "\tcurMask = _mm256_srli_epi32(curMask, " << groupSize << ");\n";
		os << "\t";
		genPopStack(os, bitsCount, groupSize+1);
		os << "\tbreak;\n";
	}
}

/*
curValue = _mm256_or_si256(curValue, _mm256_or_si256(bits1, _mm256_srli_epi32(bits2, 1)));
nextGroup();
curValue = _mm256_andnot_si256(bits0, curValue);
curValue = _mm256_or_si256(curValue, _mm256_or_si256(bits2, _mm256_srli_epi32(bits1, 1)));
nextGroup();
curValue = _mm256_andnot_si256(bits0, curValue);
*/

static void genShift(std::ostream& os, int bitsIndex, int shift) {
	if(shift > 0) {
		os << "_mm256_srli_epi32(bits" << bitsIndex+1 << ", " << shift << ")";
	} else {
		os << "bits" << bitsIndex+1;
	}
}

static void genArg(std::ostream& os, const std::vector<int>& permut, int index) {
	if(permut.size() - index == 1) {
		genShift(os, permut[index], index);
	} else {
		os << "_mm256_or_si256(";
		genShift(os, permut[index], index);
		os << ", ";
		genArg(os, permut, index + 1);
		os << ")";
	}
}

void genCodeForSmallPermut(std::ostream& os, int permutSize) {
	std::vector<int> integers = generateIntegers(permutSize);

	forEachPermutation(integers, [&os](const std::vector<int>& permut) {
		os << "\tcurValue = _mm256_or_si256(curValue, ";
		genArg(os, permut, 0);
		os << ");\n";
		os << "\tif(nextGroup()) return true;\n";
		os << "\tcurValue = _mm256_andnot_si256(bits0, curValue);\n";
	});
}

/*
curValue = _mm256_or_si256(curValue, _mm256_srli_epi32(bits5, 4));
permute4();
curValue = _mm256_andnot_si256(_mm256_srli_epi32(bits5, 4), curValue);
SWAP(bits1, bits5);
*/

static void genCodeForLargePermut(std::ostream& os, int permutSize) {
	for(int i = 1; i <= permutSize; i++) {
		os << "\tcurValue = _mm256_or_si256(curValue, _mm256_srli_epi32(bits" << permutSize << ", " << permutSize - 1 << "));\n";
		os << "\tif(permute" << permutSize-1 << "()) return true;\n";
		os << "\tcurValue = _mm256_andnot_si256(_mm256_srli_epi32(bits" << permutSize << ", " << permutSize - 1 << "), curValue);\n";
		if(i == 1) {
			os << "\tSWAP(bits" << i << ", bits" << permutSize << ");\n";
		} else if(i == permutSize) {
			os << "\tSWAP(bits" << i-1 << ", bits" << permutSize << ");\n";
		} else {
			os << "\tROTRIGHT(bits" << i-1 << ", bits" << i << ", bits" << permutSize << ");\n";
		}
	}
}

void genCodeForAllPermut() {
	std::ofstream os("permuteCode.txt");

	os << "inline bool permute1() { // no mask needed to repair permute1, can just reuse bits1\n";
	os << "\tcurValue = _mm256_or_si256(curValue, bits0);\n";
	os << "\tif(nextGroup()) return true;\n";
	os << "\tcurValue = _mm256_andnot_si256(bits0, curValue);\n";
	os << "\treturn false;\n";
	os << "}\n";

	for(int i = 2; i <= maxSmallPermuteSize; i++) {
		os << "inline bool permute" << i << "() {\n";
		genCodeForSmallPermut(os, i);
		os << "\treturn false;\n";
		os << "}\n";
	}

	for(int i = maxSmallPermuteSize + 1; i <= groupCount; i++) {
		os << "inline bool permute" << i << "() {\n";
		genCodeForLargePermut(os, i);
		os << "\treturn false;\n";
		os << "}\n";
	}
}

void genCodeForNatvis() {
	std::ofstream os("natvisCode.txt");

	int N = 50;
	/*	<DisplayString Condition = "n==0">{ {}}< / DisplayString>
		<DisplayString Condition = "n==1">{ { { data[0] } }}< / DisplayString>
		<DisplayString Condition = "n==2">{ { { data[0] }, {data[1]} }}< / DisplayString>
		*/
	os << "<DisplayString Condition=\"sz==0\">{{}}</DisplayString>\n";
	for(int row = 1; row < N; row++) {
		os << "<DisplayString Condition=\"sz==" << row << "\">{{{buf[0]}";

		for(int col = 1; col < row; col++) {
			os << ", {buf[" << col << "]}";
		}
		os << "}}</DisplayString>\n";
	}
}

void genGraphVisCode(int numLayers) {
	LayerStack layerStack = generateLayers(numLayers);
	std::ofstream os("graphVisCode.txt");
	for(int l = layerStack.layers.size() - 1; l > 0; l--) {
		const FullLayer& layerAbove = layerStack.layers[l];
		const FullLayer& layerBelow = layerStack.layers[l - 1];

		for(FunctionInput fi : layerAbove) {
			for(FunctionInput fiB : layerBelow) {
				if(isSubSet(fiB, fi)) {
					os << fi << "->" << fiB << "\n";
				}
			}
		}
	}
}

#include <string.h>
void genPermute1234Luts() {
	const char* permutations[] {
		"abcd", "abdc", "acbd", "acdb", "adbc", "adcb",
        "bacd", "badc", "bcad", "bcda", "bdac", "bdca",
        "cbad", "cbda", "cabd", "cadb", "cdba", "cdab", 
        "dbca", "dbac", "dcba", "dcab", "dabc", "dacb"
	};

	const char* oneVars[] {
		"a", "b", "c", "d"
	};

	const char* twoVars[] {
		"ab", "ac", "ad", "bc", "bd", "cd"
	};

	// Single positions
	for(const char* oneVarPosition : oneVars) {
		std::cout << "case({selectedSet, selectedPermutationInSet})" << std::endl;
		for(int setI = 0; setI < 4; setI++) {
			for(int permInSetI = 0; permInSetI < 6; permInSetI++) {
				const char* selectedPermutation = permutations[setI * 6 + permInSetI];

				int varAtPosition = selectedPermutation[oneVarPosition[0] - 'a'] - 'a';

				std::cout << "5'o" << (char('0' + setI)) << (char('0' + permInSetI)) << ": varInPos_" << oneVarPosition << "<=" << char('0' + varAtPosition) << "; ";
			}
			std::cout << "\n";
		}
		std::cout << "default: varInPos_" << oneVarPosition << "<=2'bXX;\nendcase" << std::endl;
	}

	// Double positions
	for(const char* twoVarPosition : twoVars) {
		std::cout << "case({selectedSet, selectedPermutationInSet})" << std::endl;
		for(int setI = 0; setI < 4; setI++) {
			for(int permInSetI = 0; permInSetI < 6; permInSetI++) {
				const char* selectedPermutation = permutations[setI * 6 + permInSetI];

				char varsAtPosition[2] {
					selectedPermutation[twoVarPosition[0] - 'a'],
					selectedPermutation[twoVarPosition[1] - 'a']
				};
				if(varsAtPosition[0] > varsAtPosition[1]) std::swap(varsAtPosition[0], varsAtPosition[1]);
				
				int foundAtIndex;
				for(foundAtIndex = 0; ; foundAtIndex++) {
					if(strncmp(varsAtPosition, twoVars[foundAtIndex], 2) == 0) break; // Found!
				}

				if(foundAtIndex >= 3) foundAtIndex++; // 0,1,2, 4,5,6. Skip 3 for more efficient multiplexers

				std::cout << "5'o" << (char('0' + setI)) << (char('0' + permInSetI)) << ": varInPos_" << twoVarPosition << "<=" << char('0' + foundAtIndex) << "; ";
			}
			std::cout << "\n";
		}
		std::cout << "default: varInPos_" << twoVarPosition << "<=3'bXXX;\nendcase" << std::endl;
	}
}


CommandSet codeGenCommands {"Code Generation", {
	{"graphVis1", []() {genGraphVisCode(1); }},
	{"graphVis2", []() {genGraphVisCode(2); }},
	{"graphVis3", []() {genGraphVisCode(3); }},
	{"graphVis4", []() {genGraphVisCode(4); }},
	{"graphVis5", []() {genGraphVisCode(5); }},
	{"graphVis6", []() {genGraphVisCode(6); }},
	{"graphVis7", []() {genGraphVisCode(7); }},
	{"graphVis8", []() {genGraphVisCode(8); }},
	{"graphVis9", []() {genGraphVisCode(9); }},

	{"genPermute1234LUTs", genPermute1234Luts}
}, {}};


