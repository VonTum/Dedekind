#pragma once

#include "layerDecomposition.h"
#include "dedekindDecomposition.h"

struct ValueCounted {
	countInt count;
	valueInt value;
};

void assignValues(DedekindDecomposition<ValueCounted>& decomp);
