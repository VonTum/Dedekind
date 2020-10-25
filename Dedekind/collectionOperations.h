#pragma once

#include <vector>
#include <type_traits>

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
template<typename Collection, typename Func>
void forEachSubgroup(const Collection& collection, size_t groupSize, const Func& func) {
	Collection subGroup(groupSize);

	forEachSubgroupRecurse(collection, func, subGroup, 0, 0);
}

template<typename Collection>
Collection removeAll(const Collection& vec, const Collection& toRemove) {
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
bool existsPermutationForWhich(Collection vecToPermute, const F& funcToRun) {
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

template<typename ColIterBegin, typename ColIterEnd>
inline auto tallyDistinctOrdered(ColIterBegin begin, ColIterEnd end) -> std::vector<CountedGroup<typename std::remove_reference<decltype(*begin)>::type>> {
	std::vector<CountedGroup<typename std::remove_reference<decltype(*begin)>::type>> foundItems;
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

template<typename Collection, typename Func>
std::vector<int> getSortPermutation(const Collection& collection, Func compare) {
	std::vector<int> indices = generateIntegers(collection.size());
	std::sort(indices.begin(), indices.end(), [&collection, &compare](int first, int second) {
		return compare(collection[first], collection[second]);
	});
	return indices;
}

template<typename Collection>
Collection permute(const Collection& col, std::vector<int>& permutation) {
	Collection result(permutation.size());

	for(size_t i = 0; i < permutation.size(); i++) {
		result[i] = col[permutation[i]];
	}
	return result;
}
