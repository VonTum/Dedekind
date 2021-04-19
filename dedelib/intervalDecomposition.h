#pragma once

#include "dedekindDecomposition.h"

struct IntervalSize {
	valueInt intervalSizeToBottom;

	static void populate(DedekindDecomposition<IntervalSize>& decomp);
	static valueInt getDedekind(const DedekindDecomposition<IntervalSize>& decomp);
};
