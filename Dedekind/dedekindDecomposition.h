#pragma once

#include "layerDecomposition.h"
#include "layerStack.h"

struct NoExtraInfo {};

template<typename ExtraData = NoExtraInfo>
class DedekindDecomposition {
	std::vector<LayerDecomposition<ExtraData>> layers;
	LayerStack layerStack;

public:
	DedekindDecomposition() = default;
	DedekindDecomposition(size_t order) : layers(order + 1), layerStack(generateLayers(order)) {
		std::cout << "Generating decomposition for layer " << order << "\n";
		new(&layers[0]) LayerDecomposition<ExtraData>(layerStack.layers[0]);

		for(size_t i = 1; i < layerStack.layers.size(); i++) {
			std::cout << "Generating decomposition for layer " << i << "\n";
			new(&layers[i]) LayerDecomposition<ExtraData>(layerStack.layers[i]);
			std::cout << "Linking layers " << i << " - " << (i - 1) << "\n";

			LayerDecomposition<ExtraData>& smallerClasses = layers[i - 1];
			LayerDecomposition<ExtraData>& largerClasses = layers[i];
			const FullLayer& fullSmallerLayer = layerStack.layers[i - 1];
			const FullLayer& fullLargerLayer = layerStack.layers[i];
			for(size_t setSize = 0; setSize <= smallerClasses.getNumberOfFunctionInputs(); setSize++) {
				for(BakedEquivalenceClass<EquivalenceClassInfo<ExtraData>>& smallerClass : smallerClasses[setSize]) {
					FunctionInputSet smallerOnElems = smallerClass.eqClass.asFunctionInputSet();
					FunctionInputSet smallerOffElems = invert(smallerOnElems, fullSmallerLayer);
					FunctionInputSet largerOffRemoved = removeForcedOn(fullLargerLayer, smallerOffElems);
					PreprocessedFunctionInputSet preprocessed = preprocess(std::move(largerOffRemoved));
					EquivalenceClassIndex index = largerClasses.indexOf(preprocessed);
					smallerClass.minimalForcedOffAbove = index;
				}
			}
			for(size_t setSize = 0; setSize <= largerClasses.getNumberOfFunctionInputs(); setSize++) {
				for(BakedEquivalenceClass<EquivalenceClassInfo<ExtraData>>& largerClass : largerClasses[setSize]) {
					FunctionInputSet largerOnElems = largerClass.eqClass.asFunctionInputSet();
					FunctionInputSet largerOffElems = invert(largerOnElems, fullLargerLayer);
					FunctionInputSet smallerOnRemoved = removeForcedOff(fullSmallerLayer, largerOffElems);
					PreprocessedFunctionInputSet preprocessed = preprocess(std::move(smallerOnRemoved));
					EquivalenceClassIndex index = smallerClasses.indexOf(preprocessed);
					largerClass.minimalForcedOnBelow = index;
				}
			}
		}
	}

	inline size_t numLayers() const { return layers.size(); }

	LayerDecomposition<ExtraData>& operator[](size_t i) { return layers[i]; }
	const LayerDecomposition<ExtraData>& operator[](size_t i) const { return layers[i]; }

	FullLayer& layer(size_t i) { return layerStack.layers[i]; }
	const FullLayer& layer(size_t i) const { return layerStack.layers[i]; }

	const BakedEquivalenceClass<EquivalenceClassInfo<ExtraData>>& bottom() const { return layers.front().full(); }
	const BakedEquivalenceClass<EquivalenceClassInfo<ExtraData>>& top() const { return layers.back().empty(); }
};
