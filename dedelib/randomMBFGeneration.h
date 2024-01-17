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
