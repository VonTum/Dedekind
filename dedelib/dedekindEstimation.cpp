#include "dedekindEstimation.h"

#include <math.h>
#include <iostream>
#include <fstream>
#include <random>
#include <sstream>
#include <iomanip>
#include "knownData.h"
#include "funcTypes.h"
#include "toString.h"
#include "fileNames.h"
#include "aligned_alloc.h"
#include "flatBufferManagement.h"

// Dedekind estimation asymptotics from Kleitman & Markowsky (1975) doi:10.2307/1998052
static double a(int n) {
	int ch = choose(n, n / 2 - 1);
	double powers = pow(2.0, -n/2) + n*n*pow(2.0, -n-5) - n * pow(2.0, -n-4);
	return ch * powers;
}
static double b(int n) {
	int ch = choose(n, (n - 3) / 2);
	double powers = pow(2.0, -(n+3) / 2) - n*n*pow(2.0, -n-5) - n * pow(2.0, -n-3);
	return ch * powers;
}
static double c(int n) {
	int ch = choose(n, (n - 1) / 2);
	double powers = pow(2.0, -(n+1) / 2) + n*n*pow(2.0, -n-4);
	return ch * powers;
}
double estimateDedekindNumber(int n) {
	if(n % 2 == 0) {
		return pow(2.0, choose(n, n / 2)) * exp(a(n));
	} else {
		return pow(2.0, choose(n, (n-1) / 2) + 1) * exp(b(n) + c(n));
	}
}

void naiveD10Estimation() {
    size_t bufferSizeInBytes;
	Monotonic<9>* randomMBF9Buf = (Monotonic<9>*) mmapWholeFileSequentialRead(FileName::randomMBFs(9), bufferSizeInBytes);

	static_assert(sizeof(Monotonic<9>) == 64);

	size_t numRandomMBF9 = bufferSizeInBytes / sizeof(Monotonic<9>);

	std::cout << "Loaded " << numRandomMBF9 << " MBF9" << std::endl;

	if(numRandomMBF9 == 0) {
		std::cerr << "No MBFs in this file? File is of size " << bufferSizeInBytes << " bytes. " << std::endl;
		exit(1);
	}

	std::cout << "Check if any are equal:" << std::endl;
	uint64_t total = 0;
	uint64_t numEqual = 0;
	uint64_t lastNumCopies = 0;
	uint64_t numEqualCopyCount = 0;
	for(size_t a = 0; a < numRandomMBF9; a++) {	
		uint64_t numCopies = 0;
		for(size_t b = a + 1; b < numRandomMBF9; b++) {
			Monotonic<9> mbfA = randomMBF9Buf[a];
			Monotonic<9> mbfB = randomMBF9Buf[b];

			total++;

			if(mbfA == mbfB) {
				std::cout << "mbf " << a << " == mbf " << b << std::endl;
				numEqual++;
				numCopies++;
			}
		}
		if(numCopies == lastNumCopies) {
			numEqualCopyCount++;
		} else {
			std::cout << numEqualCopyCount << " lines of " << numCopies << " copies!" << std::endl;

			numEqualCopyCount = 1;
			lastNumCopies = numCopies;
		}
	}
	std::cout << numEqual << " / " << total << " were equal!" << std::endl;

	uint64_t totalCount = 0;
	uint64_t validMBF10Count = 0;

	for(size_t a = 0; a < numRandomMBF9 / 2; a++) {
		
		for(size_t b = numRandomMBF9 / 2; b < numRandomMBF9; b++) {

			Monotonic<9> mbfA = randomMBF9Buf[a];
			Monotonic<9> mbfB = randomMBF9Buf[b];
			
			totalCount++;
			
			if(mbfA <= mbfB) {
				validMBF10Count++;
			}
		}
		if(a % 1000 == 0) std::cout << a << std::endl;
	}
	/*
	std::random_device seeder;
	RandomEngine generator{seeder()};
	for(int i = 0; i < 1000000; i++) {
		size_t a = std::uniform_int_distribution<size_t>(0, numRandomMBF9 - 1)(generator);
		size_t b = std::uniform_int_distribution<size_t>(0, numRandomMBF9 - 1)(generator);

		if(a == b) continue;
		Monotonic<9> mbfA = randomMBF9Buf[a];
		Monotonic<9> mbfB = randomMBF9Buf[b];

		totalCount++;
		if(mbfA <= mbfB) {
			validMBF10Count++;
		}
		if(i % 1000 == 0) std::cout << i << std::endl;
	}*/

	double fraction = double(validMBF10Count) / totalCount;
	double sigma_sq = fraction * (1 - fraction) / totalCount;
	std::cout << validMBF10Count << " / " << totalCount << " MBF9 comparisons = " << fraction << "; σ² = " << sigma_sq << ", σ = " << sqrt(sigma_sq) << std::endl;
	std::cout << "D(10) ~= 286386577668298411128469151667598498812366^2 * " << fraction << " = " << (286386577668298411128469151667598498812366.0*286386577668298411128469151667598498812366.0 * fraction) << std::endl;
}

template<unsigned int Variables>
struct GlobalComparisonData {
    Monotonic<Variables>* buf;
    size_t size;

    GlobalComparisonData(Monotonic<Variables>* buf, size_t size) : buf(buf), size(size) {}
};


template<unsigned int Variables>
void estimateNextDedekindNumber() {
    size_t bufferSizeInBytes;
	Monotonic<Variables>* randomMBFBuf = (Monotonic<Variables>*) mmapWholeFileSequentialRead(FileName::randomMBFs(Variables), bufferSizeInBytes);

	size_t numRandomMBF = bufferSizeInBytes / sizeof(Monotonic<Variables>);
    numRandomMBF = 4096*16; // Overwrite it for quicker run

	std::cout << "Loaded " << numRandomMBF << " MBFs" << std::endl;

	if(numRandomMBF == 0) {
		std::cerr << "No MBFs in this file? File is of size " << bufferSizeInBytes << " bytes. " << std::endl;
		exit(1);
	}

    size_t bufSizeA = numRandomMBF / 2;
    size_t bufSizeB = numRandomMBF - bufSizeA;

    Monotonic<Variables>* bufA = randomMBFBuf;
    Monotonic<Variables>* bufB = randomMBFBuf+bufSizeA;

    size_t totalCount = bufSizeA * bufSizeB;
    size_t validCombinationCount = 0;
    for(size_t i = 0; i < bufSizeA; i++) {
        for(size_t j = 0; j < bufSizeB; j++) {
            if(bufA[i] <= bufB[j]) {
                validCombinationCount++;
            }
        }
    }

    double dedekindNumVariables = dedekindNumbersAsDoubles[Variables];
	double fraction = double(validCombinationCount) / totalCount;
	double sigma_sq = fraction * (1 - fraction) / totalCount;
	std::cout << validCombinationCount << " / " << totalCount << " MBF9 comparisons = " << fraction << "; σ² = " << sigma_sq << ", σ = " << sqrt(sigma_sq) << std::endl;
	std::cout << "D(" << Variables + 1 << ") ~= " << dedekindNumVariables << "^2 * " << fraction << " = " << (dedekindNumVariables * dedekindNumVariables * fraction) << std::endl;
}


template void estimateNextDedekindNumber<1>();
template void estimateNextDedekindNumber<2>();
template void estimateNextDedekindNumber<3>();
template void estimateNextDedekindNumber<4>();
template void estimateNextDedekindNumber<5>();
template void estimateNextDedekindNumber<6>();
template void estimateNextDedekindNumber<7>();
template void estimateNextDedekindNumber<8>();
template void estimateNextDedekindNumber<9>();

// To see what bits would be opportune to use for the signature
static std::vector<uint16_t> getSignatureBits(int bitCount, unsigned int Variables, size_t numBitsToSelect) {
	std::vector<uint16_t> result;

	for(uint16_t i = 0; i < uint16_t(1) << Variables; i++) {
		if(popcnt16(i) == bitCount) {
			result.push_back(i);
		}
	}

	std::default_random_engine generator;
	
	std::shuffle(result.begin(), result.end(), generator);

	result.resize(numBitsToSelect);

	return result;
}

static void codeGenBitList(std::ostream& ss, const std::vector<uint16_t>& bits, int bitsPerBit, char combinator, size_t offset) {
	for(size_t i = 0; i < bits.size() / bitsPerBit; i++) {
		ss << "\tresult |= uint16_t(mbf.bf.bitset.get64(" << bits[i * bitsPerBit + 0] << ")";
		for(size_t j = 1; j < bitsPerBit; j++) {
			ss << " " << combinator << " mbf.bf.bitset.get64(" << bits[i * bitsPerBit + j] << ")";
		}
		ss << ") & uint16_t(1)) << " << (i + offset) << ";" << std::endl;
	}
}

static void codeGenThreeDimensionalBalancer(std::ostream& ss, uint16_t startPoint, int idx) {
	ss << "\tresult |= uint16_t((mbf.bf.bitset.get64(" << startPoint + 1/*a*/
	 << ") & mbf.bf.bitset.get64(" << startPoint + 2/*b*/
	 << ") & mbf.bf.bitset.get64(" << startPoint + 4/*c*/
	// << ")) | mbf.bf.bitset.get64(" << startPoint + 6/*bc*/
	 << ") & uint16_t(1)) << " << idx << ";" << std::endl;
}


void codeGenGetSignature() {
	std::ostream& oss = std::cout;

	for(unsigned int Variables = 5; Variables < 10; Variables++) {
		oss << "if constexpr(Variables == " << Variables << ") {" << std::endl;
		//oss << std::setbase(2);
		if(Variables % 2 == 0) {
			std::vector<uint16_t> bits = getSignatureBits(Variables / 2, Variables, 16);

			codeGenBitList(oss, bits, 1, ' ', 0);
		} else {
			// Combine multiple bits to even out the odds of a signature bit
			if(false) {
				const int numBitsPerElemList[10]{0, 0, 0, 0, 0, 1, 0, 3, 0, 5};
				size_t numBitPerBit = numBitsPerElemList[Variables];
				std::vector<uint16_t> lowBits = getSignatureBits(Variables / 2, Variables, 8 * numBitPerBit);
				std::vector<uint16_t> highBits = getSignatureBits(Variables / 2 + 1, Variables, 8 * numBitPerBit);

				codeGenBitList(oss, lowBits, numBitPerBit, '&', 0);
				codeGenBitList(oss, highBits, numBitPerBit, '|', 8);
			} else {
				std::vector<uint16_t> bits = getSignatureBits((Variables - 3) / 2, Variables - 3, 16);
				for(int i = 0; i < 16; i++) {
					codeGenThreeDimensionalBalancer(std::cout, bits[i] << 3, i);
				}
			}
		}
		oss << "} else ";
	}
	oss << "{\n\tresult = mbf.bf.bitset.data;\n}" << std::endl;
}

/*
The signature of an MBF is used to optimize comparison. It tries to accomplish two goals:

Firstly: MBFa <= MBFb iff signature(MBFa) <= signature(MBFb)

Secondly: Each signature should be *roughly* equally probable for a uniformly sampled MBF
*/
template<unsigned int Variables>
uint16_t getSignature(Monotonic<Variables> mbf) {
	uint16_t result = 0;

	// GENERATED CODE
	if constexpr(Variables == 5) {
		result |= uint16_t((mbf.bf.bitset.get64(6)) & uint16_t(1)) << 0;
		result |= uint16_t((mbf.bf.bitset.get64(18)) & uint16_t(1)) << 1;
		result |= uint16_t((mbf.bf.bitset.get64(5)) & uint16_t(1)) << 2;
		result |= uint16_t((mbf.bf.bitset.get64(17)) & uint16_t(1)) << 3;
		result |= uint16_t((mbf.bf.bitset.get64(20)) & uint16_t(1)) << 4;
		result |= uint16_t((mbf.bf.bitset.get64(3)) & uint16_t(1)) << 5;
		result |= uint16_t((mbf.bf.bitset.get64(10)) & uint16_t(1)) << 6;
		result |= uint16_t((mbf.bf.bitset.get64(24)) & uint16_t(1)) << 7;
		result |= uint16_t((mbf.bf.bitset.get64(13)) & uint16_t(1)) << 8;
		result |= uint16_t((mbf.bf.bitset.get64(25)) & uint16_t(1)) << 9;
		result |= uint16_t((mbf.bf.bitset.get64(11)) & uint16_t(1)) << 10;
		result |= uint16_t((mbf.bf.bitset.get64(22)) & uint16_t(1)) << 11;
		result |= uint16_t((mbf.bf.bitset.get64(26)) & uint16_t(1)) << 12;
		result |= uint16_t((mbf.bf.bitset.get64(7)) & uint16_t(1)) << 13;
		result |= uint16_t((mbf.bf.bitset.get64(19)) & uint16_t(1)) << 14;
		result |= uint16_t((mbf.bf.bitset.get64(28)) & uint16_t(1)) << 15;
	} else if constexpr(Variables == 6) {
		result |= uint16_t((mbf.bf.bitset.get64(38)) & uint16_t(1)) << 0;
		result |= uint16_t((mbf.bf.bitset.get64(25)) & uint16_t(1)) << 1;
		result |= uint16_t((mbf.bf.bitset.get64(44)) & uint16_t(1)) << 2;
		result |= uint16_t((mbf.bf.bitset.get64(22)) & uint16_t(1)) << 3;
		result |= uint16_t((mbf.bf.bitset.get64(37)) & uint16_t(1)) << 4;
		result |= uint16_t((mbf.bf.bitset.get64(7)) & uint16_t(1)) << 5;
		result |= uint16_t((mbf.bf.bitset.get64(19)) & uint16_t(1)) << 6;
		result |= uint16_t((mbf.bf.bitset.get64(28)) & uint16_t(1)) << 7;
		result |= uint16_t((mbf.bf.bitset.get64(41)) & uint16_t(1)) << 8;
		result |= uint16_t((mbf.bf.bitset.get64(50)) & uint16_t(1)) << 9;
		result |= uint16_t((mbf.bf.bitset.get64(42)) & uint16_t(1)) << 10;
		result |= uint16_t((mbf.bf.bitset.get64(49)) & uint16_t(1)) << 11;
		result |= uint16_t((mbf.bf.bitset.get64(13)) & uint16_t(1)) << 12;
		result |= uint16_t((mbf.bf.bitset.get64(21)) & uint16_t(1)) << 13;
		result |= uint16_t((mbf.bf.bitset.get64(11)) & uint16_t(1)) << 14;
		result |= uint16_t((mbf.bf.bitset.get64(56)) & uint16_t(1)) << 15;
	} else if constexpr(Variables == 7) {
		result |= uint16_t((mbf.bf.bitset.get64(82) & mbf.bf.bitset.get64(88) & mbf.bf.bitset.get64(37)) & uint16_t(1)) << 0;
		result |= uint16_t((mbf.bf.bitset.get64(35) & mbf.bf.bitset.get64(52) & mbf.bf.bitset.get64(28)) & uint16_t(1)) << 1;
		result |= uint16_t((mbf.bf.bitset.get64(26) & mbf.bf.bitset.get64(22) & mbf.bf.bitset.get64(69)) & uint16_t(1)) << 2;
		result |= uint16_t((mbf.bf.bitset.get64(42) & mbf.bf.bitset.get64(70) & mbf.bf.bitset.get64(74)) & uint16_t(1)) << 3;
		result |= uint16_t((mbf.bf.bitset.get64(73) & mbf.bf.bitset.get64(14) & mbf.bf.bitset.get64(67)) & uint16_t(1)) << 4;
		result |= uint16_t((mbf.bf.bitset.get64(25) & mbf.bf.bitset.get64(98) & mbf.bf.bitset.get64(13)) & uint16_t(1)) << 5;
		result |= uint16_t((mbf.bf.bitset.get64(97) & mbf.bf.bitset.get64(21) & mbf.bf.bitset.get64(49)) & uint16_t(1)) << 6;
		result |= uint16_t((mbf.bf.bitset.get64(76) & mbf.bf.bitset.get64(104) & mbf.bf.bitset.get64(50)) & uint16_t(1)) << 7;
		result |= uint16_t((mbf.bf.bitset.get64(102) | mbf.bf.bitset.get64(106) | mbf.bf.bitset.get64(54)) & uint16_t(1)) << 8;
		result |= uint16_t((mbf.bf.bitset.get64(53) | mbf.bf.bitset.get64(78) | mbf.bf.bitset.get64(51)) & uint16_t(1)) << 9;
		result |= uint16_t((mbf.bf.bitset.get64(46) | mbf.bf.bitset.get64(43) | mbf.bf.bitset.get64(86)) & uint16_t(1)) << 10;
		result |= uint16_t((mbf.bf.bitset.get64(60) | mbf.bf.bitset.get64(89) | mbf.bf.bitset.get64(92)) & uint16_t(1)) << 11;
		result |= uint16_t((mbf.bf.bitset.get64(90) | mbf.bf.bitset.get64(29) | mbf.bf.bitset.get64(85)) & uint16_t(1)) << 12;
		result |= uint16_t((mbf.bf.bitset.get64(45) | mbf.bf.bitset.get64(113) | mbf.bf.bitset.get64(27)) & uint16_t(1)) << 13;
		result |= uint16_t((mbf.bf.bitset.get64(108) | mbf.bf.bitset.get64(39) | mbf.bf.bitset.get64(75)) & uint16_t(1)) << 14;
		result |= uint16_t((mbf.bf.bitset.get64(99) | mbf.bf.bitset.get64(116) | mbf.bf.bitset.get64(77)) & uint16_t(1)) << 15;
	} else if constexpr(Variables == 8) {
		result |= uint16_t((mbf.bf.bitset.get64(120)) & uint16_t(1)) << 0;
		result |= uint16_t((mbf.bf.bitset.get64(212)) & uint16_t(1)) << 1;
		result |= uint16_t((mbf.bf.bitset.get64(142)) & uint16_t(1)) << 2;
		result |= uint16_t((mbf.bf.bitset.get64(226)) & uint16_t(1)) << 3;
		result |= uint16_t((mbf.bf.bitset.get64(178)) & uint16_t(1)) << 4;
		result |= uint16_t((mbf.bf.bitset.get64(116)) & uint16_t(1)) << 5;
		result |= uint16_t((mbf.bf.bitset.get64(30)) & uint16_t(1)) << 6;
		result |= uint16_t((mbf.bf.bitset.get64(141)) & uint16_t(1)) << 7;
		result |= uint16_t((mbf.bf.bitset.get64(85)) & uint16_t(1)) << 8;
		result |= uint16_t((mbf.bf.bitset.get64(135)) & uint16_t(1)) << 9;
		result |= uint16_t((mbf.bf.bitset.get64(60)) & uint16_t(1)) << 10;
		result |= uint16_t((mbf.bf.bitset.get64(89)) & uint16_t(1)) << 11;
		result |= uint16_t((mbf.bf.bitset.get64(228)) & uint16_t(1)) << 12;
		result |= uint16_t((mbf.bf.bitset.get64(113)) & uint16_t(1)) << 13;
		result |= uint16_t((mbf.bf.bitset.get64(139)) & uint16_t(1)) << 14;
		result |= uint16_t((mbf.bf.bitset.get64(83)) & uint16_t(1)) << 15;
	} else if constexpr(Variables == 9) {
		// 1 bit
		/*result |= uint16_t((mbf.bf.bitset.get64(293)) & uint16_t(1)) << 0;
		result |= uint16_t((mbf.bf.bitset.get64(212)) & uint16_t(1)) << 1;
		result |= uint16_t((mbf.bf.bitset.get64(142)) & uint16_t(1)) << 2;
		result |= uint16_t((mbf.bf.bitset.get64(226)) & uint16_t(1)) << 3;
		result |= uint16_t((mbf.bf.bitset.get64(332)) & uint16_t(1)) << 4;
		result |= uint16_t((mbf.bf.bitset.get64(116)) & uint16_t(1)) << 5;
		result |= uint16_t((mbf.bf.bitset.get64(456)) & uint16_t(1)) << 6;
		result |= uint16_t((mbf.bf.bitset.get64(424)) & uint16_t(1)) << 7;
		result |= uint16_t((mbf.bf.bitset.get64(355)) & uint16_t(1)) << 8;
		result |= uint16_t((mbf.bf.bitset.get64(301)) & uint16_t(1)) << 9;
		result |= uint16_t((mbf.bf.bitset.get64(205)) & uint16_t(1)) << 10;
		result |= uint16_t((mbf.bf.bitset.get64(309)) & uint16_t(1)) << 11;
		result |= uint16_t((mbf.bf.bitset.get64(403)) & uint16_t(1)) << 12;
		result |= uint16_t((mbf.bf.bitset.get64(185)) & uint16_t(1)) << 13;
		result |= uint16_t((mbf.bf.bitset.get64(484)) & uint16_t(1)) << 14;
		result |= uint16_t((mbf.bf.bitset.get64(466)) & uint16_t(1)) << 15;*/

		// 2 bits
		/*result |= uint16_t((mbf.bf.bitset.get64(293) & mbf.bf.bitset.get64(212)) & uint16_t(1)) << 0;
		result |= uint16_t((mbf.bf.bitset.get64(142) & mbf.bf.bitset.get64(226)) & uint16_t(1)) << 1;
		result |= uint16_t((mbf.bf.bitset.get64(332) & mbf.bf.bitset.get64(116)) & uint16_t(1)) << 2;
		result |= uint16_t((mbf.bf.bitset.get64(456) & mbf.bf.bitset.get64(424)) & uint16_t(1)) << 3;
		result |= uint16_t((mbf.bf.bitset.get64(85) & mbf.bf.bitset.get64(135)) & uint16_t(1)) << 4;
		result |= uint16_t((mbf.bf.bitset.get64(60) & mbf.bf.bitset.get64(89)) & uint16_t(1)) << 5;
		result |= uint16_t((mbf.bf.bitset.get64(228) & mbf.bf.bitset.get64(270)) & uint16_t(1)) << 6;
		result |= uint16_t((mbf.bf.bitset.get64(139) & mbf.bf.bitset.get64(83)) & uint16_t(1)) << 7;
		result |= uint16_t((mbf.bf.bitset.get64(355) | mbf.bf.bitset.get64(301)) & uint16_t(1)) << 8;
		result |= uint16_t((mbf.bf.bitset.get64(205) | mbf.bf.bitset.get64(309)) & uint16_t(1)) << 9;
		result |= uint16_t((mbf.bf.bitset.get64(403) | mbf.bf.bitset.get64(185)) & uint16_t(1)) << 10;
		result |= uint16_t((mbf.bf.bitset.get64(484) | mbf.bf.bitset.get64(466)) & uint16_t(1)) << 11;
		result |= uint16_t((mbf.bf.bitset.get64(124) | mbf.bf.bitset.get64(188)) & uint16_t(1)) << 12;
		result |= uint16_t((mbf.bf.bitset.get64(110) | mbf.bf.bitset.get64(151)) & uint16_t(1)) << 13;
		result |= uint16_t((mbf.bf.bitset.get64(310) | mbf.bf.bitset.get64(333)) & uint16_t(1)) << 14;
		result |= uint16_t((mbf.bf.bitset.get64(199) | mbf.bf.bitset.get64(122)) & uint16_t(1)) << 15;*/

		// 3 bits
		/*result |= uint16_t((mbf.bf.bitset.get64(293) & mbf.bf.bitset.get64(212) & mbf.bf.bitset.get64(142)) & uint16_t(1)) << 0;
		result |= uint16_t((mbf.bf.bitset.get64(226) & mbf.bf.bitset.get64(332) & mbf.bf.bitset.get64(116)) & uint16_t(1)) << 1;
		result |= uint16_t((mbf.bf.bitset.get64(456) & mbf.bf.bitset.get64(424) & mbf.bf.bitset.get64(85)) & uint16_t(1)) << 2;
		result |= uint16_t((mbf.bf.bitset.get64(135) & mbf.bf.bitset.get64(60) & mbf.bf.bitset.get64(89)) & uint16_t(1)) << 3;
		result |= uint16_t((mbf.bf.bitset.get64(228) & mbf.bf.bitset.get64(270) & mbf.bf.bitset.get64(139)) & uint16_t(1)) << 4;
		result |= uint16_t((mbf.bf.bitset.get64(83) & mbf.bf.bitset.get64(108) & mbf.bf.bitset.get64(394)) & uint16_t(1)) << 5;
		result |= uint16_t((mbf.bf.bitset.get64(396) & mbf.bf.bitset.get64(53) & mbf.bf.bitset.get64(344)) & uint16_t(1)) << 6;
		result |= uint16_t((mbf.bf.bitset.get64(360) & mbf.bf.bitset.get64(202) & mbf.bf.bitset.get64(263)) & uint16_t(1)) << 7;
		result |= uint16_t((mbf.bf.bitset.get64(355) | mbf.bf.bitset.get64(301) | mbf.bf.bitset.get64(205)) & uint16_t(1)) << 8;
		result |= uint16_t((mbf.bf.bitset.get64(309) | mbf.bf.bitset.get64(403) | mbf.bf.bitset.get64(185)) & uint16_t(1)) << 9;
		result |= uint16_t((mbf.bf.bitset.get64(484) | mbf.bf.bitset.get64(466) | mbf.bf.bitset.get64(124)) & uint16_t(1)) << 10;
		result |= uint16_t((mbf.bf.bitset.get64(188) | mbf.bf.bitset.get64(110) | mbf.bf.bitset.get64(151)) & uint16_t(1)) << 11;
		result |= uint16_t((mbf.bf.bitset.get64(310) | mbf.bf.bitset.get64(333) | mbf.bf.bitset.get64(199)) & uint16_t(1)) << 12;
		result |= uint16_t((mbf.bf.bitset.get64(122) | mbf.bf.bitset.get64(179) | mbf.bf.bitset.get64(436)) & uint16_t(1)) << 13;
		result |= uint16_t((mbf.bf.bitset.get64(440) | mbf.bf.bitset.get64(94) | mbf.bf.bitset.get64(410)) & uint16_t(1)) << 14;
		result |= uint16_t((mbf.bf.bitset.get64(422) | mbf.bf.bitset.get64(285) | mbf.bf.bitset.get64(316)) & uint16_t(1)) << 15;*/
		
		// 4 bits
		/*result |= uint16_t((mbf.bf.bitset.get64(293) & mbf.bf.bitset.get64(212) & mbf.bf.bitset.get64(142) & mbf.bf.bitset.get64(226)) & uint16_t(1)) << 0;
		result |= uint16_t((mbf.bf.bitset.get64(332) & mbf.bf.bitset.get64(116) & mbf.bf.bitset.get64(456) & mbf.bf.bitset.get64(424)) & uint16_t(1)) << 1;
		result |= uint16_t((mbf.bf.bitset.get64(85) & mbf.bf.bitset.get64(135) & mbf.bf.bitset.get64(60) & mbf.bf.bitset.get64(89)) & uint16_t(1)) << 2;
		result |= uint16_t((mbf.bf.bitset.get64(228) & mbf.bf.bitset.get64(270) & mbf.bf.bitset.get64(139) & mbf.bf.bitset.get64(83)) & uint16_t(1)) << 3;
		result |= uint16_t((mbf.bf.bitset.get64(108) & mbf.bf.bitset.get64(394) & mbf.bf.bitset.get64(396) & mbf.bf.bitset.get64(53)) & uint16_t(1)) << 4;
		result |= uint16_t((mbf.bf.bitset.get64(344) & mbf.bf.bitset.get64(360) & mbf.bf.bitset.get64(202) & mbf.bf.bitset.get64(263)) & uint16_t(1)) << 5;
		result |= uint16_t((mbf.bf.bitset.get64(432) & mbf.bf.bitset.get64(92) & mbf.bf.bitset.get64(417) & mbf.bf.bitset.get64(57)) & uint16_t(1)) << 6;
		result |= uint16_t((mbf.bf.bitset.get64(282) & mbf.bf.bitset.get64(389) & mbf.bf.bitset.get64(106) & mbf.bf.bitset.get64(39)) & uint16_t(1)) << 7;
		result |= uint16_t((mbf.bf.bitset.get64(355) | mbf.bf.bitset.get64(301) | mbf.bf.bitset.get64(205) | mbf.bf.bitset.get64(309)) & uint16_t(1)) << 8;
		result |= uint16_t((mbf.bf.bitset.get64(403) | mbf.bf.bitset.get64(185) | mbf.bf.bitset.get64(484) | mbf.bf.bitset.get64(466)) & uint16_t(1)) << 9;
		result |= uint16_t((mbf.bf.bitset.get64(124) | mbf.bf.bitset.get64(188) | mbf.bf.bitset.get64(110) | mbf.bf.bitset.get64(151)) & uint16_t(1)) << 10;
		result |= uint16_t((mbf.bf.bitset.get64(310) | mbf.bf.bitset.get64(333) | mbf.bf.bitset.get64(199) | mbf.bf.bitset.get64(122)) & uint16_t(1)) << 11;
		result |= uint16_t((mbf.bf.bitset.get64(179) | mbf.bf.bitset.get64(436) | mbf.bf.bitset.get64(440) | mbf.bf.bitset.get64(94)) & uint16_t(1)) << 12;
		result |= uint16_t((mbf.bf.bitset.get64(410) | mbf.bf.bitset.get64(422) | mbf.bf.bitset.get64(285) | mbf.bf.bitset.get64(316)) & uint16_t(1)) << 13;
		result |= uint16_t((mbf.bf.bitset.get64(468) | mbf.bf.bitset.get64(157) | mbf.bf.bitset.get64(458) | mbf.bf.bitset.get64(107)) & uint16_t(1)) << 14;
		result |= uint16_t((mbf.bf.bitset.get64(345) | mbf.bf.bitset.get64(428) | mbf.bf.bitset.get64(174) | mbf.bf.bitset.get64(62)) & uint16_t(1)) << 15;*/

		// 5 bits
		/*result |= uint16_t((mbf.bf.bitset.get64(116) & mbf.bf.bitset.get64(456) & mbf.bf.bitset.get64(424) & mbf.bf.bitset.get64(85) & mbf.bf.bitset.get64(135)) & uint16_t(1)) << 1;
		result |= uint16_t((mbf.bf.bitset.get64(60) & mbf.bf.bitset.get64(89) & mbf.bf.bitset.get64(228) & mbf.bf.bitset.get64(270) & mbf.bf.bitset.get64(139)) & uint16_t(1)) << 2;
		result |= uint16_t((mbf.bf.bitset.get64(83) & mbf.bf.bitset.get64(108) & mbf.bf.bitset.get64(394) & mbf.bf.bitset.get64(396) & mbf.bf.bitset.get64(53)) & uint16_t(1)) << 3;
		result |= uint16_t((mbf.bf.bitset.get64(344) & mbf.bf.bitset.get64(360) & mbf.bf.bitset.get64(202) & mbf.bf.bitset.get64(263) & mbf.bf.bitset.get64(432)) & uint16_t(1)) << 4;
		result |= uint16_t((mbf.bf.bitset.get64(92) & mbf.bf.bitset.get64(417) & mbf.bf.bitset.get64(57) & mbf.bf.bitset.get64(282) & mbf.bf.bitset.get64(389)) & uint16_t(1)) << 5;
		result |= uint16_t((mbf.bf.bitset.get64(106) & mbf.bf.bitset.get64(39) & mbf.bf.bitset.get64(209) & mbf.bf.bitset.get64(15) & mbf.bf.bitset.get64(101)) & uint16_t(1)) << 6;
		result |= uint16_t((mbf.bf.bitset.get64(184) & mbf.bf.bitset.get64(300) & mbf.bf.bitset.get64(51) & mbf.bf.bitset.get64(71) & mbf.bf.bitset.get64(393)) & uint16_t(1)) << 7;
		result |= uint16_t((mbf.bf.bitset.get64(355) | mbf.bf.bitset.get64(301) | mbf.bf.bitset.get64(205) | mbf.bf.bitset.get64(309) | mbf.bf.bitset.get64(403)) & uint16_t(1)) << 8;
		result |= uint16_t((mbf.bf.bitset.get64(185) | mbf.bf.bitset.get64(484) | mbf.bf.bitset.get64(466) | mbf.bf.bitset.get64(124) | mbf.bf.bitset.get64(188)) & uint16_t(1)) << 9;
		result |= uint16_t((mbf.bf.bitset.get64(110) | mbf.bf.bitset.get64(151) | mbf.bf.bitset.get64(310) | mbf.bf.bitset.get64(333) | mbf.bf.bitset.get64(199)) & uint16_t(1)) << 10;
		result |= uint16_t((mbf.bf.bitset.get64(122) | mbf.bf.bitset.get64(179) | mbf.bf.bitset.get64(436) | mbf.bf.bitset.get64(440) | mbf.bf.bitset.get64(94)) & uint16_t(1)) << 11;
		result |= uint16_t((mbf.bf.bitset.get64(410) | mbf.bf.bitset.get64(422) | mbf.bf.bitset.get64(285) | mbf.bf.bitset.get64(316) | mbf.bf.bitset.get64(468)) & uint16_t(1)) << 12;
		result |= uint16_t((mbf.bf.bitset.get64(157) | mbf.bf.bitset.get64(458) | mbf.bf.bitset.get64(107) | mbf.bf.bitset.get64(345) | mbf.bf.bitset.get64(428)) & uint16_t(1)) << 13;
		result |= uint16_t((mbf.bf.bitset.get64(174) | mbf.bf.bitset.get64(62) | mbf.bf.bitset.get64(295) | mbf.bf.bitset.get64(31) | mbf.bf.bitset.get64(167)) & uint16_t(1)) << 14;
		result |= uint16_t((mbf.bf.bitset.get64(244) | mbf.bf.bitset.get64(362) | mbf.bf.bitset.get64(93) | mbf.bf.bitset.get64(115) | mbf.bf.bitset.get64(434)) & uint16_t(1)) << 15;*/

		// 3D Balancing Function. Highly correlated bits are taken in sub-cubes for 50/50 split, such that the correlation between groups is minimizedresult |= uint16_t(((mbf.bf.bitset.get64(305) & mbf.bf.bitset.get64(306) & mbf.bf.bitset.get64(308)) | mbf.bf.bitset.get64(310)) & uint16_t(1)) << 0;
		// It's the smallest sub-function I could find that I could split 50/50 well. 
		result |= uint16_t(mbf.bf.bitset.get64(305) & mbf.bf.bitset.get64(306) & mbf.bf.bitset.get64(308) & uint16_t(1)) << 0;
		result |= uint16_t(mbf.bf.bitset.get64(201) & mbf.bf.bitset.get64(202) & mbf.bf.bitset.get64(204) & uint16_t(1)) << 1;
		result |= uint16_t(mbf.bf.bitset.get64(353) & mbf.bf.bitset.get64(354) & mbf.bf.bitset.get64(356) & uint16_t(1)) << 2;
		result |= uint16_t(mbf.bf.bitset.get64(177) & mbf.bf.bitset.get64(178) & mbf.bf.bitset.get64(180) & uint16_t(1)) << 3;
		result |= uint16_t(mbf.bf.bitset.get64(297) & mbf.bf.bitset.get64(298) & mbf.bf.bitset.get64(300) & uint16_t(1)) << 4;
		result |= uint16_t(mbf.bf.bitset.get64(57) & mbf.bf.bitset.get64(58) & mbf.bf.bitset.get64(60) & uint16_t(1)) << 5;
		result |= uint16_t(mbf.bf.bitset.get64(153) & mbf.bf.bitset.get64(154) & mbf.bf.bitset.get64(156) & uint16_t(1)) << 6;
		result |= uint16_t(mbf.bf.bitset.get64(225) & mbf.bf.bitset.get64(226) & mbf.bf.bitset.get64(228) & uint16_t(1)) << 7;
		result |= uint16_t(mbf.bf.bitset.get64(329) & mbf.bf.bitset.get64(330) & mbf.bf.bitset.get64(332) & uint16_t(1)) << 8;
		result |= uint16_t(mbf.bf.bitset.get64(401) & mbf.bf.bitset.get64(402) & mbf.bf.bitset.get64(404) & uint16_t(1)) << 9;
		result |= uint16_t(mbf.bf.bitset.get64(337) & mbf.bf.bitset.get64(338) & mbf.bf.bitset.get64(340) & uint16_t(1)) << 10;
		result |= uint16_t(mbf.bf.bitset.get64(393) & mbf.bf.bitset.get64(394) & mbf.bf.bitset.get64(396) & uint16_t(1)) << 11;
		result |= uint16_t(mbf.bf.bitset.get64(105) & mbf.bf.bitset.get64(106) & mbf.bf.bitset.get64(108) & uint16_t(1)) << 12;
		result |= uint16_t(mbf.bf.bitset.get64(169) & mbf.bf.bitset.get64(170) & mbf.bf.bitset.get64(172) & uint16_t(1)) << 13;
		result |= uint16_t(mbf.bf.bitset.get64(89) & mbf.bf.bitset.get64(90) & mbf.bf.bitset.get64(92) & uint16_t(1)) << 14;
		result |= uint16_t(mbf.bf.bitset.get64(449) & mbf.bf.bitset.get64(450) & mbf.bf.bitset.get64(452) & uint16_t(1)) << 15;

		// Second 3D Balancing Function attempt
		/*result |= uint16_t(mbf.bf.bitset.get64(321) & mbf.bf.bitset.get64(322) & mbf.bf.bitset.get64(324) & uint16_t(1)) << 0;
		result |= uint16_t(mbf.bf.bitset.get64(25) & mbf.bf.bitset.get64(26) & mbf.bf.bitset.get64(28) & uint16_t(1)) << 1;
		result |= uint16_t(mbf.bf.bitset.get64(273) & mbf.bf.bitset.get64(274) & mbf.bf.bitset.get64(276) & uint16_t(1)) << 2;
		result |= uint16_t(mbf.bf.bitset.get64(265) & mbf.bf.bitset.get64(266) & mbf.bf.bitset.get64(268) & uint16_t(1)) << 3;
		result |= uint16_t(mbf.bf.bitset.get64(305) & mbf.bf.bitset.get64(306) & mbf.bf.bitset.get64(308) & uint16_t(1)) << 4;
		result |= uint16_t(mbf.bf.bitset.get64(201) & mbf.bf.bitset.get64(202) & mbf.bf.bitset.get64(204) & uint16_t(1)) << 5;
		result |= uint16_t(mbf.bf.bitset.get64(353) & mbf.bf.bitset.get64(354) & mbf.bf.bitset.get64(356) & uint16_t(1)) << 6;
		result |= uint16_t(mbf.bf.bitset.get64(177) & mbf.bf.bitset.get64(178) & mbf.bf.bitset.get64(180) & uint16_t(1)) << 7;
		result |= uint16_t(mbf.bf.bitset.get64(297) & mbf.bf.bitset.get64(298) & mbf.bf.bitset.get64(300) & uint16_t(1)) << 8;
		result |= uint16_t(mbf.bf.bitset.get64(57) & mbf.bf.bitset.get64(58) & mbf.bf.bitset.get64(60) & uint16_t(1)) << 9;
		result |= uint16_t(mbf.bf.bitset.get64(153) & mbf.bf.bitset.get64(154) & mbf.bf.bitset.get64(156) & uint16_t(1)) << 10;
		result |= uint16_t(mbf.bf.bitset.get64(225) & mbf.bf.bitset.get64(226) & mbf.bf.bitset.get64(228) & uint16_t(1)) << 11;
		result |= uint16_t(mbf.bf.bitset.get64(465) & mbf.bf.bitset.get64(466) & mbf.bf.bitset.get64(468) & uint16_t(1)) << 12;
		result |= uint16_t(mbf.bf.bitset.get64(121) & mbf.bf.bitset.get64(122) & mbf.bf.bitset.get64(124) & uint16_t(1)) << 13;
		result |= uint16_t(mbf.bf.bitset.get64(433) & mbf.bf.bitset.get64(434) & mbf.bf.bitset.get64(436) & uint16_t(1)) << 14;
		result |= uint16_t(mbf.bf.bitset.get64(425) & mbf.bf.bitset.get64(426) & mbf.bf.bitset.get64(428) & uint16_t(1)) << 15;*/

	} else {
		result = mbf.bf.bitset.data;
	}

	return result;
}

template<unsigned int Variables>
void makeSignatureStatistics() {
	size_t bufferSizeInBytes;
	Monotonic<Variables>* randomMBFBuf = (Monotonic<Variables>*) mmapWholeFileSequentialRead(FileName::randomMBFs(Variables), bufferSizeInBytes);

	size_t numRandomMBF = bufferSizeInBytes / sizeof(Monotonic<Variables>);
    
	std::cout << "Loaded " << numRandomMBF << " MBFs" << std::endl;

	if(numRandomMBF == 0) {
		std::cerr << "No MBFs in this file? File is of size " << bufferSizeInBytes << " bytes. " << std::endl;
		exit(1);
	}

	numRandomMBF = 4096*4096*16; // Overwrite it for quicker run

	size_t statistics[1 << 16];
	for(size_t i = 0; i < 1 << 16; i++) {statistics[i] = 0;}
	size_t perBitStats[16]{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

	for(size_t i = 0; i < numRandomMBF; i++) {
		uint16_t signature = getSignature(randomMBFBuf[i]);

		/*printHex(std::cout, randomMBFBuf[i].bf);
		std::cout << ": " << signature << std::endl;*/
		statistics[signature]++;

		for(int i = 0; i < 16; i++) {
			if((signature & 0b1) == 1) {
				perBitStats[i]++;
			}
			signature >>= 1;
		}
	}

	for(size_t i = 0; i < 1 << 16; i++) {
		if(i % 16 == 0) {
			/*char binary[17];
			binary[16] = '\0';
			for */
			std::cout << "\n[" << i << "]:    \t";
		}

		std::cout << statistics[i] << "\t";
	}
	std::cout << std::endl;

	for(int i = 0; i < 16; i++) {
		std::cout << "bit " << i << ": " << (perBitStats[i] * 100.0 / numRandomMBF) << "%\n";
	}
	std::cout << std::endl;

	size_t naiveComparisons = numRandomMBF * numRandomMBF;
	size_t nonfilteredComparisons = 0; // Sanity check, should equal naiveComparisons
	size_t filteredComparisons = 0;

	for(uint32_t upper = 0; upper < 1 << 16; upper++) {
		for(uint32_t lower = 0; lower < 1 << 16; lower++) {
			size_t comparisonsHere = statistics[lower] * statistics[upper];
			if((~upper & lower) == 0) { // Lower is a subset of upper
				filteredComparisons += comparisonsHere;
			}
			nonfilteredComparisons += comparisonsHere;
		}
	}

	std::cout << "naiveComparisons: " << naiveComparisons << std::endl;
	std::cout << "nonfilteredComparisons: " << nonfilteredComparisons << std::endl;
	std::cout << "filteredComparisons: " << filteredComparisons << std::endl;

	double percent = filteredComparisons * 100.0 / naiveComparisons;
	std::cout << "Filtering %: " << percent << "%" << std::endl;
}

template void makeSignatureStatistics<1>();
template void makeSignatureStatistics<2>();
template void makeSignatureStatistics<3>();
template void makeSignatureStatistics<4>();
template void makeSignatureStatistics<5>();
template void makeSignatureStatistics<6>();
template void makeSignatureStatistics<7>();
template void makeSignatureStatistics<8>();
template void makeSignatureStatistics<9>();


template<unsigned int Variables>
struct MBFBlockComparer {

};


