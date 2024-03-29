#pragma once

#include <vector>
#include <set>
#include <ostream>
#include "functionInput.h"
#include "functionInputSet.h"
#include "booleanFunction.h"
#include "equivalenceClass.h"
#include "equivalenceClassMap.h"
#include "layerStack.h"
#include "smallVector.h"
#include "funcTypes.h"
#include "u192.h"

#include "dedekindDecomposition.h"
#include "valuedDecomposition.h"
#include "intervalDecomposition.h"

#include "set.h"
#include "aligned_set.h"

template<typename Iter, typename IterEnd>
inline void printIter(std::ostream& os, Iter iter, const IterEnd& iterEnd) {
	os << '{';
	if(iter != iterEnd) {
		os << *iter;
		++iter;
		for(; iter != iterEnd; ++iter) {
			os << ',' << *iter;
		}
	}
	os << '}';
}

template<typename T>
inline std::ostream& operator<<(std::ostream& os, const std::vector<T>& vec) {
	printIter(os, vec.begin(), vec.end());
	return os;
}
template<typename T, size_t Size>
inline std::ostream& operator<<(std::ostream& os, const std::array<T, Size>& arr) {
	printIter(os, arr.begin(), arr.end());
	return os;
}
template<typename T>
inline std::ostream& operator<<(std::ostream& os, const std::set<T>& vec) {
	printIter(os, vec.begin(), vec.end());
	return os;
}
template<typename T, size_t Align>
inline std::ostream& operator<<(std::ostream& os, const aligned_set<T, Align>& vec) {
	printIter(os, vec.begin(), vec.end());
	return os;
}
template<typename T, size_t MaxSize>
inline std::ostream& operator<<(std::ostream& os, const SmallVector<T, MaxSize>& vec) {
	printIter(os, vec.begin(), vec.end());
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

template<typename IntType>
inline std::ostream& printBits(std::ostream& os, IntType bits, size_t upTo = sizeof(IntType) * 8) {
	for(size_t i = 0; i < upTo; i++) {
		char bit = ((bits >> (upTo - i - 1)) & 1) == 1 ? '1' : '0';
		os << bit;
	}
	return os;
}

template<size_t Size>
inline std::ostream& operator<<(std::ostream& os, const BitSet<Size>& bitset) {
	for(size_t i = 0; i < Size; i++) {
		char bit = bitset.get(Size - i - 1) ? '1' : '0';
		os << bit;
	}
	return os;
}

template<unsigned int Variables>
inline std::ostream& operator<<(std::ostream& os, const BooleanFunction<Variables>& fis) {
	os << fis.bitset;
	return os;
}

template<unsigned int Variables>
void printSet(const BooleanFunction<Variables>& bf) {
	std::cout << "{";
	bf.forEachOne([](size_t index) {
		std::cout << FunctionInput{static_cast<unsigned int>(index)} << ", ";
	});
	std::cout << "}\n";
}


template<size_t Size>
inline std::ostream& printHex(std::ostream& os, const BitSet<Size>& bitset) {
	char hexDigits[16]{'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
	for(size_t i = 0; i < Size; i += 4) {
		int total = 0;
		for(unsigned int b = 0; b < 4; b++) {
			if(bitset.get(Size - i - 4 + b)) {
				total |= 1 << b;
			}
		}
		os << hexDigits[total];
	}
	return os;
}

inline std::ostream& operator<<(std::ostream& os, u128 v) {
	os << toString(v);
	return os;
}
inline std::ostream& operator<<(std::ostream& os, u192 v) {
	os << toString(v);
	return os;
}


template<unsigned int Variables>
void printACComponent(std::ostream& os, size_t index) {
	if(index == 0) {
		os << '/';
	} else {
		for(unsigned int bit = 0; bit < Variables; bit++) {
			if(index & (size_t(1) << bit)) {
				os << char('a' + bit);
			}
		}
	}
}

template<unsigned int Variables>
std::ostream& operator<<(std::ostream& os, const AntiChain<Variables>& ac) {
	bool isFirst = true;
	ac.forEachOne([&](size_t index) {
		os << (isFirst ? "{" : ", ");
		isFirst = false;
		printACComponent<Variables>(os, index);
	});
	if(isFirst) os << '{';
	os << '}';
	return os;
}

template<unsigned int Variables>
std::ostream& operator<<(std::ostream& os, const Monotonic<Variables>& ac) {
	os << ac.asAntiChain();
	return os;
}
template<unsigned int Variables>
std::ostream& operator<<(std::ostream& os, const Layer<Variables>& l) {
	os << l.asAntiChain();
	return os;
}
template<unsigned int Variables>
std::ostream& operator<<(std::ostream& os, const SingletonAntiChain<Variables>& s) {
	os << "{";
	printACComponent<Variables>(os, s.index);
	os << "}";
	return os;
}





template<unsigned int Variables>
inline std::ostream& printHex(std::ostream& os, const BooleanFunction<Variables>& fis) {
	printHex(os, fis.bitset);
	return os;
}

template<>
inline std::ostream& operator<<(std::ostream& os, const std::vector<bool>& fis) {
	for(FunctionInput::underlyingType i = 0; i < fis.size(); i++) {
		char bit = fis[fis.size() - i - 1] ? '1' : '0';
		os << bit;
	}
	return os;
}
inline std::ostream& operator<<(std::ostream& os, const PreprocessedFunctionInputSet& s) {
	os << s.functionInputSet;
	return os;
}

inline std::ostream& operator<<(std::ostream& os, const EquivalenceClass& eq) {
	os << eq.asFunctionInputSet();
	return os;
}

template<typename Key, typename Value>
inline std::ostream& operator<<(std::ostream& os, const BufferedMap<Key, Value>& eqMap) {
	bool isFirst = true;
	for(const typename BufferedMap<Key, Value>::KeyValue& item : eqMap) {
		os << (isFirst ? '{' : ',') << item.key << ": " << item.value;
		isFirst = false;
	}
	os << '}';
	return os;
}

inline std::ostream& operator<<(std::ostream& os, const LayerStack& layers) {
	os << "LayerStack" << layers.layers;
	return os;
}

inline std::ostream& operator<<(std::ostream& os, NoExtraData) {
	return os;
}

inline std::ostream& operator<<(std::ostream& os, ValueCounted vc) {
	os << "c: " << vc.count << " v:" << vc.value;
	return os;
}

inline std::ostream& operator<<(std::ostream& os, IntervalSize is) {
	os << "is: " << is.intervalSizeToBottom;
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
		const BakedEquivalenceClassMap<EquivalenceClassInfo<ExtraInfo>>& subSizeMap = d.subSizeMap(size);
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
