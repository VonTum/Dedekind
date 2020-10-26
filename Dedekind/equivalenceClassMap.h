#pragma once

#include "equivalenceClass.h"

#include <vector>

template<typename V>
class EquivalenceClassMap {
	std::vector<std::pair<EquivalenceClass, V>> equivClasses;
public:
	V& get(const PreprocessedFunctionInputSet& preprocessed) {
		for(std::pair<EquivalenceClass, V>& keyValue : equivClasses) {
			if(keyValue.first.contains(preprocessed)) {
				return keyValue.second;
			}
		}
		throw "input was not found!";
	}
	const V& get(const PreprocessedFunctionInputSet& preprocessed) const {
		for(const std::pair<EquivalenceClass, V>& keyValue : equivClasses) {
			if(keyValue.first.contains(preprocessed)) {
				return keyValue.second;
			}
		}
		throw "input was not found!";
	}
	V& getOrDefault(const PreprocessedFunctionInputSet& preprocessed, const V& defaultForCreate) {
		for(std::pair<EquivalenceClass, V>& keyValue : equivClasses) {
			if(keyValue.first.contains(preprocessed)) {
				return keyValue.second;
			}
		}
		equivClasses.emplace_back(EquivalenceClass(preprocessed), defaultForCreate);
		return equivClasses.back().second;
	}

	template<typename FoundFunc, typename NewItemFunc>
	void findOrCreate(const PreprocessedFunctionInputSet& preprocessed, FoundFunc onFound, NewItemFunc createNewItem) {
		for(std::pair<EquivalenceClass, V>& keyValue : equivClasses) {
			if(keyValue.first.contains(preprocessed)) {
				onFound(keyValue.second);
				return;
			}
		}
		equivClasses.emplace_back(EquivalenceClass(preprocessed), createNewItem(preprocessed));
	}

	void add(const PreprocessedFunctionInputSet& preprocessed, const V& value) {
		equivClasses.emplace_back(EquivalenceClass(preprocessed), value);
	}

	void add(const EquivalenceClass& eqClass, const V& value) {
		equivClasses.emplace_back(eqClass, value);
	}

	size_t size() const { return equivClasses.size(); }
	void clear() { equivClasses.clear(); }

	auto begin() { return equivClasses.begin(); }
	auto begin() const { return equivClasses.begin(); }
	auto end() { return equivClasses.end(); }
	auto end() const { return equivClasses.end(); }

	std::pair<EquivalenceClass, V>& front() { return equivClasses.front(); }
	const std::pair<EquivalenceClass, V>& front() const { return equivClasses.front(); }


	template<typename VOut, typename Func>
	EquivalenceClassMap<VOut> mapValues(Func mapFunc) && {
		EquivalenceClassMap<VOut> result;
		result.equivClasses.reserve(this->equivClasses.size());
		for(std::pair<EquivalenceClass, V>& item : *this) {
			result.equivClasses.push_back(std::move(item.first), mapFunc(std::move(item.second)));
		}
		return result;
	}
};
