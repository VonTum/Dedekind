#pragma once

#include <vector>
#include <ostream>
#include "functionInput.h"
#include "equivalenceClass.h"
#include "equivalenceClassMap.h"
#include "layerStack.h"
#include "smallVector.h"

#include "set.h"
#include "aligned_set.h"

template<typename T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& vec) {
	os << '{';
	auto iter = vec.begin();
	auto iterEnd = vec.end();
	if(iter != iterEnd) {
		os << *iter;
		++iter;
		for(; iter != iterEnd; ++iter) {
			os << ',' << *iter;
		}
	}
	os << '}';
	return os;
}
template<typename T>
std::ostream& operator<<(std::ostream& os, const set<T>& vec) {
	os << '{';
	auto iter = vec.begin();
	auto iterEnd = vec.end();
	if(iter != iterEnd) {
		os << *iter;
		++iter;
		for(; iter != iterEnd; ++iter) {
			os << ',' << *iter;
		}
	}
	os << '}';
	return os;
}
template<typename T, size_t Align>
std::ostream& operator<<(std::ostream& os, const aligned_set<T, Align>& vec) {
	os << '{';
	auto iter = vec.begin();
	auto iterEnd = vec.end();
	if(iter != iterEnd) {
		os << *iter;
		++iter;
		for(; iter != iterEnd; ++iter) {
			os << ',' << *iter;
		}
	}
	os << '}';
	return os;
}
template<typename T, size_t MaxSize>
std::ostream& operator<<(std::ostream& os, const SmallVector<T, MaxSize>& vec) {
	os << '{';
	auto iter = vec.begin();
	auto iterEnd = vec.end();
	if(iter != iterEnd) {
		os << *iter;
		++iter;
		for(; iter != iterEnd; ++iter) {
			os << ',' << *iter;
		}
	}
	os << '}';
	return os;
}

std::ostream& operator<<(std::ostream& os, FunctionInput f) {
	if(f.empty()) {
		os << '/';
	} else {
		int curIndex = 0;
		int32_t v = f.inputBits;
		while(v != 0) {
			if(v & 0x1) {
				os << char('a' + curIndex);
			}
			v >>= 1;
			curIndex++;
		}
	}
	return os;
}

std::ostream& operator<<(std::ostream& os, const VariableCoOccurence& vo) {
	os << vo.coOccursWith;
	return os;
}
std::ostream& operator<<(std::ostream& os, const InitialVariableObservations& obs) {
	os << "{occ:" << obs.occurenceCount << ",subGraph:" << obs.subGraphSize << '}';
	return os;
}
std::ostream& operator<<(std::ostream& os, const PreprocessedFunctionInputSet& s) {
	os << s.functionInputSet;
	os << '.';
	for(auto occ : s.variableOccurences) {
		os << occ;
	}
	return os;
}

std::ostream& operator<<(std::ostream& os, const EquivalenceClass& eq) {
	os << eq.asFunctionInputSet();
	return os;
}

template<typename V>
std::ostream& operator<<(std::ostream& os, const EquivalenceClassMap<V>& eqMap) {
	bool isFirst = true;
	for(const ValuedEquivalenceClass<V>& item : eqMap) {
		os << (isFirst ? '{' : ',') << item.equivClass << ": " << item.value;
		isFirst = false;
	}
	os << '}';
	return os;
}

std::ostream& operator<<(std::ostream& os, const LayerStack& layers) {
	os << "LayerStack" << layers.layers;
	return os;
}

std::ostream& operator<<(std::ostream& os, EquivalenceClassIndex idx) {
	os << idx.size << '|' << idx.indexInSubLayer;
	return os;
}

std::ostream& operator<<(std::ostream& os, const EquivalenceClassInfo& info) {
	os << "c:" << info.count << " v:" << info.value << " inv:" << info.inverse << " up:" << info.minimalForcedOffAbove << " dn:" << info.minimalForcedOnBelow;
	return os;
}

template<typename V>
std::ostream& operator<<(std::ostream& os, const BakedValuedEquivalenceClass<V>& n) {
	os << n.eqClass << ": " << n.value;
	return os;
}

std::ostream& operator<<(std::ostream& os, const LayerDecomposition& d) {
	for(size_t size = 0; size <= d.getNumberOfFunctionInputs(); size++) {
		os << "    [" << size << "] ";
		const EquivalenceMap& subSizeMap = d[size];
		for(size_t indexInSubSize = 0; indexInSubSize < subSizeMap.size(); indexInSubSize++) {
			os << subSizeMap[indexInSubSize];
			if(indexInSubSize < subSizeMap.size() - 1) os << ",  ";
		}
		os << "\n";
	}

	return os;
}

std::ostream& operator<<(std::ostream& os, const DedekindDecomposition& d) {
	for(int layer = d.numLayers()-1; layer >= 0; layer--) {
		os << "Layer " << layer << "\n";
		os << d[layer] << "\n";
	}

	return os;
}

void printIndent(std::ostream& os, int indent) {
	for(int i = 0; i < indent; i++) {
		os << "  ";
	}
}
