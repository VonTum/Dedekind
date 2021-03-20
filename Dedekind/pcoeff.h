#pragma once

#include "connectGraph.h"
#include "allMBFsMap.h"

template<unsigned int Variables>
void computeDedekindPCoeff() {
	AllMBFMap<Variables, ExtraData> allMBFs = readAllMBFsMapExtraData<Variables>();

	for(int mainLayerI = 0; mainLayerI < (1 << Variables) + 1; mainLayerI++) {
		const BakedMap<Monotonic<Variables>, ExtraData>& mainLayer = allMBFs.layers[mainLayerI];

		for(const KeyValue<Monotonic<Variables>, ExtraData>& kv : mainLayer) {

		}
	}
}


