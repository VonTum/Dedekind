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
inline std::ostream& operator<<(std::ostream& os, const std::vector<T>& vec) {
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
inline std::ostream& operator<<(std::ostream& os, const set<T>& vec) {
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
inline std::ostream& operator<<(std::ostream& os, const aligned_set<T, Align>& vec) {
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
inline std::ostream& operator<<(std::ostream& os, const SmallVector<T, MaxSize>& vec) {
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

inline std::ostream& operator<<(std::ostream& os, FunctionInput f) {
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

inline std::ostream& operator<<(std::ostream& os, const VariableCoOccurence& vo) {
	os << vo.coOccursWith;
	return os;
}
inline std::ostream& operator<<(std::ostream& os, const InitialVariableObservations& obs) {
	os << "{occ:" << obs.occurenceCount << ",subGraph:" << obs.subGraphSize << '}';
	return os;
}
inline std::ostream& operator<<(std::ostream& os, const PreprocessedFunctionInputSet& s) {
	os << s.functionInputSet;
	os << '.';
	for(auto occ : s.variableOccurences) {
		os << occ;
	}
	return os;
}

inline std::ostream& operator<<(std::ostream& os, const EquivalenceClass& eq) {
	os << eq.asFunctionInputSet();
	return os;
}

template<typename V>
inline std::ostream& operator<<(std::ostream& os, const EquivalenceClassMap<V>& eqMap) {
	bool isFirst = true;
	for(const ValuedEquivalenceClass<V>& item : eqMap) {
		os << (isFirst ? '{' : ',') << item.equivClass << ": " << item.value;
		isFirst = false;
	}
	os << '}';
	return os;
}

inline std::ostream& operator<<(std::ostream& os, const LayerStack& layers) {
	os << "LayerStack" << layers.layers;
	return os;
}

inline std::ostream& operator<<(std::ostream& os, EquivalenceClassIndex idx) {
	os << idx.size << '|' << idx.indexInSubLayer;
	return os;
}

inline std::ostream& operator<<(std::ostream& os, NoExtraInfo) {
	return os;
}

inline std::ostream& operator<<(std::ostream& os, ValueCounted vc) {
	os << "c: " << vc.count << " v:" << vc.value;
	return os;
}

template<typename V>
inline std::ostream& operator<<(std::ostream& os, const BakedEquivalenceClass<EquivalenceClassInfo<V>>& eq) {
	os << eq.eqClass << ": inv:" << eq.inverse << " up:" << eq.minimalForcedOffAbove << " dn:" << eq.minimalForcedOnBelow << " nxt:(";
	const NextClass* iter = eq.iterNextClasses().begin();
	const NextClass* iterEnd = eq.iterNextClasses().end();

	if(iter != iterEnd) {
		os << iter->formationCount << "x" << iter->nodeIndex;
		iter++;
		for(; iter != iterEnd; iter++) {
			os << ", " << iter->formationCount << "x" << iter->nodeIndex;
		}
	}
	os << ")";
	os << " " << static_cast<V>(eq);
	return os;
}

template<typename ExtraInfo>
inline std::ostream& operator<<(std::ostream& os, const LayerDecomposition<ExtraInfo>& d) {
	for(size_t size = 0; size <= d.getNumberOfFunctionInputs(); size++) {
		os << "    [" << size << "] ";
		const BakedEquivalenceClassMap<EquivalenceClassInfo<ExtraInfo>>& subSizeMap = d[size];
		for(size_t indexInSubSize = 0; indexInSubSize < subSizeMap.size(); indexInSubSize++) {
			os << subSizeMap[indexInSubSize];
			if(indexInSubSize < subSizeMap.size() - 1) os << ",  ";
		}
		os << "\n";
	}

	return os;
}

template<typename ExtraInfo>
inline std::ostream& operator<<(std::ostream& os, const DedekindDecomposition<ExtraInfo>& d) {
	for(int layer = d.numLayers()-1; layer >= 0; layer--) {
		os << "Layer " << layer << "\n";
		os << d[layer] << "\n";
	}

	return os;
}

inline void printIndent(std::ostream& os, int indent) {
	for(int i = 0; i < indent; i++) {
		os << "  ";
	}
}
