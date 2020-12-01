#pragma once

#include "layerDecomposition.h"
#include "layerStack.h"

class DedekindDecomposition {
	std::vector<LayerDecomposition> layers;
	LayerStack layerStack;

public:
	DedekindDecomposition() = default;
	DedekindDecomposition(size_t order);

	inline size_t numLayers() const { return layers.size(); }

	LayerDecomposition& operator[](size_t i) { return layers[i]; }
	const LayerDecomposition& operator[](size_t i) const { return layers[i]; }

	FullLayer& layer(size_t i) { return layerStack.layers[i]; }
	const FullLayer& layer(size_t i) const { return layerStack.layers[i]; }

	const EquivalenceNode& bottom() const { return layers.front().full(); }
	const EquivalenceNode& top() const { return layers.back().empty(); }
};
