#include "dedekindEstimation.h"

#include <math.h>
#include "knownData.h"
#include <iostream>
#include <fstream>
#include "funcTypes.h"
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
