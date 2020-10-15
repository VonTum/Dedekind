
#include "codeGen.h"

#include <fstream>

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

static void genPushStack(std::ostream& os, int bitsCount, int groupSize) {
	for(int bitsMover = bitsCount - 1; bitsMover - groupSize > 0; bitsMover--) {
		os << "bits" << bitsMover << " = bits" << bitsMover - groupSize << "; ";
	}
	os << "\n";
}

static void genPopStack(std::ostream& os, int bitsCount, int groupSize) {
	for(int bitsMover = 1; bitsMover + groupSize < bitsCount; bitsMover++) {
		os << "bits" << bitsMover << " = bits" << bitsMover + groupSize << "; ";
	}
	os << "\n";
}

void genCodeForEquivClass() {
	std::ofstream os("equivClassCode.txt");

	int groupCount = 9;
	int bitsCount = 13;
	for(int groupSize = 1; groupSize <= groupCount; groupSize++) {
		os << "case " << groupSize << ":\n";
		os << "\t";
		genPushStack(os, bitsCount, groupSize);
		os << "\tbits0 = _mm256_sub_epi32(_mm256_slli_epi32(curMask, " << groupSize << "), curMask);\n";
		os << "\tbits1 = _mm256_and_si256(data, curMask);\n";
		for(int j = 2; j <= groupSize; j++) {
			os << "\tbits" << j << " = _mm256_and_si256(_mm256_srli_epi32(data, " << j << "), curMask);\n";
		}
		os << "\tcurMask = _mm256_slli_epi32(curMask, " << groupSize << ");\n";
		os << "\tpermute" << groupSize << "();\n";
		os << "\tcurMask = _mm256_srli_epi32(curMask, " << groupSize << ");\n";
		os << "\t";
		genPopStack(os, bitsCount, groupSize);
		os << "\tbreak;\n";
	}
}

