#pragma once

#include <vector>
#include <ostream>
#include "functionInput.h"
#include "equivalenceClass.h"
#include "equivalenceClassMap.h"
#include "layerStack.h"

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

std::ostream& operator<<(std::ostream& os, VariableOccurence vo) {
	os << char('a' + vo.index) << ':' << vo.count;
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
	os << static_cast<const PreprocessedFunctionInputSet&>(eq);
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

void printIndent(std::ostream& os, int indent) {
	for(int i = 0; i < indent; i++) {
		os << "  ";
	}
}
