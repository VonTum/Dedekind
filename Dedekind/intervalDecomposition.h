#pragma once

#include "dedekindDecomposition.h"

struct IntervalSize {
	valueInt intervalSizeToBottom;
};

void assignIntervalSizes(DedekindDecomposition<IntervalSize>& decomp);
