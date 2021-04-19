#pragma once

#include <vector>
#include <type_traits>
#include <cassert>
#include <algorithm>

template<typename Collection, typename Func>
void forEachSubgroupRecurse(const Collection& collection, const Func& func, Collection& output, size_t startFrom, size_t indexInOutput) {
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

// expects a function of type: void func(const Collection& subSet)
template<typename Collection, typename Func>
void forEachSubgroup(const Collection& collection, size_t groupSize, const Func& func) {
	Collection subGroup(groupSize);

	forEachSubgroupRecurse(collection, func, subGroup, 0, 0);
}

// expects a function of type: void func(const Collection& subSet)
// does not run the function on the collection itself, nor on the empty group
template<typename Collection, typename Func>
void forEachNonEmptySubgroup(const Collection& collection, const Func& func) {
	if(collection.size() >= 1) {
		for(size_t i = 1; i < collection.size(); i++) {
			forEachSubgroup(collection, i, func);
		}
		func(collection);
	}
}
// expects a function of type: void func(const Collection& subSet)
// does not run the function on the collection itself, nor on the empty group
template<typename Collection, typename Func>
void forEachSubgroup(const Collection& collection, const Func& func) {
	Collection emptyCol;
	func(emptyCol);
	forEachNonEmptySubgroup(collection, func);
}

template<typename Collection, typename Func>
inline void forEachSplitRecursive(const Collection& collection, size_t indexInCollection, const Func& func, Collection& firstBuf, Collection& secondBuf) {
	if(indexInCollection == collection.size()) {
		func(const_cast<const Collection&>(firstBuf), const_cast<const Collection&>(secondBuf));
	} else {
		const auto& item = collection[indexInCollection];
		firstBuf.push_back(item);
		forEachSplitRecursive(collection, indexInCollection + 1, func, firstBuf, secondBuf);
		firstBuf.pop_back();
		secondBuf.push_back(item);
		forEachSplitRecursive(collection, indexInCollection + 1, func, firstBuf, secondBuf);
		secondBuf.pop_back();
	}
}

// expects a function of type: void func(const Collection& a, const Collection& b)
template<typename Collection, typename Func>
inline void forEachSplit(const Collection& collection, const Func& func) {
	Collection firstBuf;
	Collection secondBuf;

	forEachSplitRecursive(collection, 0, func, firstBuf, secondBuf);
}


// returns true iff all items in a also appear in b, eg a is a subset of b (a <= b)
template<typename SubSetIter, typename SetIter>
bool isSubSet(SubSetIter aIter, SubSetIter aEnd, SetIter bStart, SetIter bEnd) {
	while(aIter != aEnd) {
		for(auto bIter = bStart; bIter != bEnd; ++bIter) {
			if(*aIter == *bIter) {
				// found
				goto nextItem;
			}
		}
		return false; // no corresponding element in b found for item
		nextItem:
		++aIter;
	}
	return true; // all elements matched
}

template<typename Collection>
Collection removeAll(const Collection& vec, const Collection& toRemove) {
	assert(isSubSet(toRemove, vec));
	Collection result;
	result.reserve(vec.size() - toRemove.size());

	for(const auto& item : vec) {
		for(const auto& removeCompare : toRemove) {
			if(item == removeCompare) {
				goto dontAdd;
			}
		}
		result.push_back(item);
		dontAdd:;
	}
	return result;
}

template<typename Collection>
Collection pop_front(const Collection& vec) {
	Collection result;
	result.reserve(vec.size() - 1);
	for(size_t i = 1; i < vec.size(); i++) {
		result.push_back(vec[i]);
	}
	return result;
}

template<typename Collection, typename F>
void forEachPermutationRecurse(Collection& vecToPermute, size_t keepBefore, const F& funcToRun) {
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
template<typename Collection, typename F> // pass by value, we're messing with this vector in forEachPermutationRecurse
void forEachPermutation(Collection vecToPermute, const F& funcToRun) {
	forEachPermutationRecurse(vecToPermute, 0, funcToRun);
}


template<typename Collection, typename F>
bool existsPermutationForWhichRecurse(Collection& vecToPermute, size_t keepBefore, const F& funcToRun) {
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
template<typename Collection, typename F> // pass by value, we're messing with this vector in forEachPermutationRecurse
bool existsPermutationForWhich(Collection& vecToPermute, const F& funcToRun) {
	return existsPermutationForWhichRecurse(vecToPermute, 0, funcToRun);
}

// destroys b
template<typename AIter, typename BIter>
bool destructiveIsSubSetWithDuplicates(AIter aIter, AIter aEnd, BIter bStart, BIter bEnd) {
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

inline std::vector<int> generateIntegers(int max) {
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
	return result;
}

template<typename ColIterBegin, typename ColIterEnd>
inline bool allEqual(ColIterBegin begin, ColIterEnd end) {
	if(begin != end) {
		const auto& firstElem = *begin;
		++begin;
		for(; begin != end; ++begin) {
			if(firstElem != *begin) {
				return false;
			}
		}
	}
	return true;
}

template<typename ColIterBegin, typename ColIterEnd>
inline size_t countDistinct(ColIterBegin begin, ColIterEnd end) {
	std::vector<decltype(*begin)> foundItems;
	for(; begin != end; ++begin) {
		decltype(*begin) item = *begin;
		for(decltype(*begin) compareTo : foundItems) {
			if(item == compareTo) {
				goto nextItem;
			}
		}
		foundItems.push_back(item);
		nextItem:;
	}
	return foundItems.size();
}

template<typename T>
struct CountedGroup {
	T group;
	int count;
};

template<typename T>
bool operator==(const CountedGroup<T>& first, const CountedGroup<T>& second) {
	return first.count == second.count && first.group == second.group;
}
template<typename T>
bool operator!=(const CountedGroup<T>& first, const CountedGroup<T>& second) {
	return first.count != second.count || first.group != second.group;
}

template<template<typename> typename OutputCollection, typename ColIterBegin, typename ColIterEnd>
inline auto tallyDistinctOrdered(ColIterBegin begin, ColIterEnd end) -> OutputCollection<CountedGroup<typename std::remove_reference<decltype(*begin)>::type>> {
	OutputCollection<CountedGroup<typename std::remove_reference<decltype(*begin)>::type>> foundItems;
	for(; begin != end; ++begin) {
		const auto& item = *begin;
		for(auto& compareTo : foundItems) {
			if(item == compareTo.group) {
				compareTo.count++;
				goto nextItem;
			}
		}
		foundItems.push_back({item, 1});
		nextItem:;
	}
	return foundItems;
}

template<typename Collection, typename IntCollection, typename Func>
void getSortPermutation(const Collection& collection, Func compare, IntCollection& indices) {
	std::sort(indices.begin(), indices.end(), [&collection, &compare](int first, int second) {
		return compare(collection[first], collection[second]);
	});
}

template<typename Collection, typename PermutationCollection>
void permuteIntoExistingBuf(const Collection& col, const PermutationCollection& permutation, Collection& result) {
	assert(result.size() == permutation.size());

	for(size_t i = 0; i < permutation.size(); i++) {
		result[i] = col[permutation[i]];
	}
}

template<typename Collection, typename PermutationCollection>
Collection permute(const Collection& col, const PermutationCollection& permutation) {
	Collection result(permutation.size());

	permuteIntoExistingBuf(col, permutation, result);

	return result;
}

/* 
	produces a list of ids, where each id corresponds to participation in a certain group, starting from 0 and ascending
	returns the grouping and the groups
	example: assignUniqueGroups({1.5, 1.5, 2.0, 1.5, 2.0, 1.7}) = {0, 0, 1, 0, 1, 2}, {1.5, 2.0, 1.7}
*/
template<template<typename> typename Collection, typename T>
static std::pair<Collection<int>, Collection<T>> assignUniqueGroups(const Collection<T>& values) {
	Collection<int> result(values.size());
	Collection<T> knownValues;
	for(size_t i = 0; i < values.size(); i++) {
		for(int indexInKnownGroups = 0; indexInKnownGroups < knownValues.size(); indexInKnownGroups++) {
			if(knownValues[indexInKnownGroups] == values[i]) {
				result[i] = indexInKnownGroups;
				goto nextItem;
			}
		}
		result[i] = knownValues.size();
		knownValues.push_back(values[i]);
		nextItem:;
	}
	return std::make_pair(std::move(result), std::move(knownValues));
}

template<typename T>
// input vector must be SORTED
static std::vector<int> countOccurencesSorted(const std::vector<T>& identifiers) {
	std::vector<int> occurenceGroups{1};
	T lastObs = identifiers[0];
	for(int i = 1; i < identifiers.size(); i++) {
		T curObs = identifiers[i];
		if(curObs == lastObs) {
			occurenceGroups.back()++;
		} else {
			occurenceGroups.push_back(1);
			lastObs = curObs;
		}
	}
	return occurenceGroups;
}
