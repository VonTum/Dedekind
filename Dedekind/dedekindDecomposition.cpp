#include "dedekindDecomposition.h"


DedekindDecomposition::DedekindDecomposition(size_t order) : layers(order + 1), layerStack(generateLayers(order)) {
	for(size_t i = 0; i < order + 1; i++) {
		new(&layers[i]) LayerDecomposition(layerStack.layers[i]);
	}
}
