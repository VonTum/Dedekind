#pragma once

#include "parallelIter.h"

#include "layerDecomposition.h"
#include "layerStack.h"

struct NoExtraData;

template<typename ExtraData = NoExtraData>
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
				iterCollectionInParallel(smallerClasses.subSizeMap(setSize), [&fullSmallerLayer, &fullLargerLayer, &largerClasses](BakedEquivalenceClass<EquivalenceClassInfo<ExtraData>>& smallerClass) {
					FunctionInputSet smallerOnElems = smallerClass.eqClass.asFunctionInputSet();
					FunctionInputSet smallerOffElems = invert(smallerOnElems, fullSmallerLayer);
					FunctionInputSet largerOffRemoved = removeForcedOn(fullLargerLayer, smallerOffElems);
					PreprocessedFunctionInputSet preprocessed = preprocess(std::move(largerOffRemoved));
					eqClassIndexInt index = largerClasses.indexOf(preprocessed);
					smallerClass.minimalForcedOffAbove = index;
				});
			}
			for(size_t setSize = 0; setSize <= largerClasses.getNumberOfFunctionInputs(); setSize++) {
				iterCollectionInParallel(largerClasses.subSizeMap(setSize), [&fullSmallerLayer, &fullLargerLayer, &smallerClasses](BakedEquivalenceClass<EquivalenceClassInfo<ExtraData>>& largerClass) {
					FunctionInputSet largerOnElems = largerClass.eqClass.asFunctionInputSet();
					FunctionInputSet largerOffElems = invert(largerOnElems, fullLargerLayer);
					FunctionInputSet smallerOnRemoved = removeForcedOff(fullSmallerLayer, largerOffElems);
					PreprocessedFunctionInputSet preprocessed = preprocess(std::move(smallerOnRemoved));
					eqClassIndexInt index = smallerClasses.indexOf(preprocessed);
					largerClass.minimalForcedOnBelow = index;
				});
			}
		}

		std::cout << "Populating...\n";
		ExtraData::populate(*this);
	}

	inline size_t numLayers() const { return layers.size(); }

	LayerDecomposition<ExtraData>& operator[](size_t i) { return layers[i]; }
	const LayerDecomposition<ExtraData>& operator[](size_t i) const { return layers[i]; }

	FullLayer& layer(size_t i) { return layerStack.layers[i]; }
	const FullLayer& layer(size_t i) const { return layerStack.layers[i]; }

	const BakedEquivalenceClass<EquivalenceClassInfo<ExtraData>>& emptyBottom() const { return layers.front().empty(); }
	const BakedEquivalenceClass<EquivalenceClassInfo<ExtraData>>& fullBottom() const { return layers.front().full(); }
	const BakedEquivalenceClass<EquivalenceClassInfo<ExtraData>>& emptyTop() const { return layers.back().empty(); }
	const BakedEquivalenceClass<EquivalenceClassInfo<ExtraData>>& fullTop() const { return layers.back().full(); }

	valueInt getDedekind() const {
		return ExtraData::getDedekind(*this);
	}
};

struct NoExtraData {
	// for ExtraInfo implementations
	static void populate(DedekindDecomposition<NoExtraData>&) {}
	static valueInt getDedekind(const DedekindDecomposition<NoExtraData>&) { throw "Not implemented!"; }
};
