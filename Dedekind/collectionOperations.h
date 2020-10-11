#pragma once

#include <vector>

struct IteratorEnd {};

template<typename Iter>
struct IteratorFactory {
	Iter iter;

	Iter begin() const {
		return iter;
	}
	IteratorEnd end() const {
		return IteratorEnd{};
	}
};

template<template<typename> typename Collection, typename T, typename Func>
void forEachSubgroupRecurse(const Collection<T>& collection, const Func& func, Collection<T>& output, size_t startFrom, size_t indexInOutput) {
	size_t leftoverItems = output.size() - indexInOutput;
	if(leftoverItems > 0) {
		for(size_t i = startFrom; i < collection.size() - (leftoverItems - 1); i++) {
			output[indexInOutput] = collection[i];
			forEachSubgroupRecurse(collection, func, output, i + 1, indexInOutput + 1);
		}
	} else {
		func(output);
	}
}
template<template<typename> typename Collection, typename T, typename Func>
void forEachSubgroup(const Collection<T>& collection, size_t groupSize, const Func& func) {
	Collection<T> subGroup(groupSize);

	forEachSubgroupRecurse(collection, func, subGroup, 0, 0);
}

template<template<typename> typename Collection, typename T>
Collection<T> removeAll(const Collection<T>& vec, const Collection<T>& toRemove) {
	Collection<T> result;
	result.reserve(vec.size() - toRemove.size());

	for(const T& item : vec) {
		for(const T& removeCompare : toRemove) {
			if(item == removeCompare) {
				goto dontAdd;
			}
		}
		result.push_back(item);
		dontAdd:;
	}
	return result;
}

template<template<typename> typename Collection, typename T>
Collection<T> pop_front(const Collection<T>& vec) {
	Collection<T> result;
	result.reserve(vec.size() - 1);
	for(int i = 1; i < vec.size(); i++) {
		result.push_back(vec[i]);
	}
	return result;
}

template<template<typename> typename Collection, typename T, typename F>
void forEachPermutationRecurse(Collection<T>& vecToPermute, size_t keepBefore, const F& funcToRun) {
	if(keepBefore < vecToPermute.size() - 1) {
		forEachPermutationRecurse(vecToPermute, keepBefore + 1, funcToRun);
		for(size_t i = keepBefore + 1; i < vecToPermute.size(); i++) {
			std::swap(vecToPermute[keepBefore], vecToPermute[i]);
			forEachPermutationRecurse(vecToPermute, keepBefore + 1, funcToRun);
			std::swap(vecToPermute[keepBefore], vecToPermute[i]);
		}
	} else {
		funcToRun(vecToPermute);
	}
}
// runs the given function on all permutations of 
template<template<typename> typename Collection, typename T, typename F> // pass by value, we're messing with this vector in forEachPermutationRecurse
void forEachPermutation(Collection<T> vecToPermute, const F& funcToRun) {
	forEachPermutationRecurse(vecToPermute, 0, funcToRun);
}


template<template<typename> typename Collection, typename T, typename F>
bool existsPermutationForWhichRecurse(Collection<T>& vecToPermute, size_t keepBefore, const F& funcToRun) {
	if(keepBefore < vecToPermute.size() - 1) {
		if(existsPermutationForWhichRecurse(vecToPermute, keepBefore + 1, funcToRun)) return true;
		for(size_t i = keepBefore + 1; i < vecToPermute.size(); i++) {
			std::swap(vecToPermute[keepBefore], vecToPermute[i]);
			if(existsPermutationForWhichRecurse(vecToPermute, keepBefore + 1, funcToRun)) return true;
			std::swap(vecToPermute[keepBefore], vecToPermute[i]);
		}
		return false;
	} else {
		return funcToRun(vecToPermute);
	}
}
// runs the given function on all permutations of 
template<template<typename> typename Collection, typename T, typename F> // pass by value, we're messing with this vector in forEachPermutationRecurse
bool existsPermutationForWhich(Collection<T> vecToPermute, const F& funcToRun) {
	return existsPermutationForWhichRecurse(vecToPermute, 0, funcToRun);
}

// destroys b
template<typename AIter, typename BIter>
bool unordered_contains_all(AIter aIter, AIter aEnd, BIter bStart, BIter bEnd) {
	while(aIter != aEnd) {
		for(auto bIter = bStart; bIter != bEnd; ++bIter) {
			if(*aIter == *bIter) {
				--bEnd;
				*bIter = *bEnd;
				goto nextItem;
			}
		}
		return false; // no corresponding element in b found for item
		nextItem:
		++aIter;
	}
	return true; // all elements matched
}

std::vector<int> generateIntegers(int max) {
	std::vector<int> result(max);

	for(int i = 0; i < max; i++) {
		result[i] = i;
	}

	return result;
}

template<typename T1, typename T2>
std::vector<std::pair<T1, T2>> zip(const std::vector<T1>& a, const std::vector<T2>& b) {
	assert(a.size() == b.size());
	std::vector<std::pair<T1, T2>> result(a.size());
	for(size_t i = 0; i < a.size(); i++) {
		result[i] = std::make_pair(a[i], b[i]);
	}
}
