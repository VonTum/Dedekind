#pragma once

#include "layerDecomposition.h"
#include "dedekindDecomposition.h"

struct ValueCounted {
	countInt count;
	valueInt value;

	static void populate(DedekindDecomposition<ValueCounted>& decomp);
	static valueInt getDedekind(const DedekindDecomposition<ValueCounted>& decomp);
};
