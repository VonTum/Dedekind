#pragma once

#include <vector>
#include <mutex>
#include <memory>
#include <stddef.h>

#include "u192.h"
#include "pcoeffClasses.h"

#include "synchronizedQueue.h"

#include "processingContext.h"

#include "threadPool.h"

#include "numaMem.h"

constexpr size_t VALIDATION_BUFFER_SIZE(unsigned int Variables) {
	return mbfCounts[Variables];
}

BetaSum produceBetaTerm(ClassInfo info, uint64_t pcoeffSum, uint64_t pcoeffCount);
BetaSum produceBetaTerm(ClassInfo info, ProcessedPCoeffSum processedPCoeff);

ResultProcessorOutput resultProcessor(
	unsigned int Variables,
	PCoeffProcessingContext& context,
	size_t numResults
);

// Expects mbfs0 to be stored in memory of socket0, and mbfs1 to have memory of socket1
ResultProcessorOutput NUMAResultProcessorWithValidator(
	unsigned int Variables,
	PCoeffProcessingContext& context,
	size_t numResults,
	size_t numValidators,
	void(*validator)(const OutputBuffer&, const void*, ThreadPool&),
	const void* mbfs[2]
);
