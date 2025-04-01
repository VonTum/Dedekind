#pragma once

#include <array>
#include "funcTypes.h"

void benchmarkMBF9Generation();

template<unsigned int Variables>
void benchmarkRandomMBFGeneration();

extern template void benchmarkRandomMBFGeneration<1>();
extern template void benchmarkRandomMBFGeneration<2>();
extern template void benchmarkRandomMBFGeneration<3>();
extern template void benchmarkRandomMBFGeneration<4>();
extern template void benchmarkRandomMBFGeneration<5>();
extern template void benchmarkRandomMBFGeneration<6>();
extern template void benchmarkRandomMBFGeneration<7>();

void parallelizeMBF8GenerationAcrossAllCores(size_t numToGenerate);
void parallelizeMBF9GenerationAcrossAllCores(size_t numToGenerate);

template<unsigned int Variables>
void estimateDedekRandomWalks();

extern template void estimateDedekRandomWalks<1>();
extern template void estimateDedekRandomWalks<2>();
extern template void estimateDedekRandomWalks<3>();
extern template void estimateDedekRandomWalks<4>();
extern template void estimateDedekRandomWalks<5>();
extern template void estimateDedekRandomWalks<6>();
extern template void estimateDedekRandomWalks<7>();
extern template void estimateDedekRandomWalks<8>();
extern template void estimateDedekRandomWalks<9>();
extern template void estimateDedekRandomWalks<10>();
extern template void estimateDedekRandomWalks<11>();
extern template void estimateDedekRandomWalks<12>();
extern template void estimateDedekRandomWalks<13>();
extern template void estimateDedekRandomWalks<14>();
extern template void estimateDedekRandomWalks<15>();
