#pragma once

#include <vector>
#include <mutex>
#include <memory>
#include <stddef.h>
#include <functional>

#include "u192.h"
#include "pcoeffClasses.h"

#include "synchronizedQueue.h"

#include "processingContext.h"

#include "threadPool.h"

#include "numaMem.h"

// The values after the halfway point are 0, we do not store these in the results file and buffers
constexpr size_t VALIDATION_BUFFER_SIZE(unsigned int Variables) {
#ifdef PCOEFF_DEDUPLICATE
	return flatNodeLayerOffsets[Variables][(1 << Variables) / 2 + 1];
#else
	return mbfCounts[Variables];
#endif
}

BetaSum produceBetaTerm(ClassInfo info, uint64_t pcoeffSum, uint64_t pcoeffCount);
BetaSum produceBetaTerm(ClassInfo info, ProcessedPCoeffSum processedPCoeff);

ResultProcessorOutput NUMAResultProcessor(
	unsigned int Variables,
	PCoeffProcessingContext& context,
	const std::function<std::vector<JobTopInfo>()>& topLoader
);
