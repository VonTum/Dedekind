#pragma once

#include <vector>
#include <ostream>

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

template<typename T>
struct SubGroupIterator {
	const std::vector<T>* collection;
	std::vector<int> indices;

	SubGroupIterator(const std::vector<T>& collection, int subGroupSize) : collection(&collection), indices(subGroupSize) {
		assert(subGroupSize <= collection.size());

		for(int i = 0; i < subGroupSize; i++) {
			indices[i] = subGroupSize - i - 1;
		}
	}

	std::vector<T> operator*() const {
		std::vector<T> result(indices.size());

		std::transform(indices.begin(), indices.end(), result.begin(), [this](int index) {
			return (*this->collection)[index];
		});

		return result;
	}

	void increment(int index) {
		indices[index]++;
		if(indices[index] == collection->size() - index) {
			if(index + 1 == indices.size()) {
				this->collection = nullptr; // signify end of iteration
			} else {
				increment(index + 1);
				indices[index] = indices[index + 1] + 1;
			}
		}
	}

	void operator++() {
		for(int index = 0; index < indices.size(); index++) {
			indices[index]++;
			if(indices[index] < collection->size() - index) {
				for(; index > 0; index--) {
					indices[index - 1] = indices[index] + 1;
				}
				return;
			}
		}
		this->indices.clear();
	}

	bool operator!=(IteratorEnd) const { return this->indices.size() != 0; }
};

template<typename T>
IteratorFactory<SubGroupIterator<T>> iterSubgroups(const std::vector<T>& collection, int groupSize) {
	return IteratorFactory<SubGroupIterator<T>>{SubGroupIterator<T>(collection, groupSize)};
}

template<typename T, typename Func>
void forEachSubgroupRecurse(const std::vector<T>& collection, const Func& func, std::vector<T>& output, size_t startFrom, size_t indexInOutput) {
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
template<typename T, typename Func>
void forEachSubgroup(const std::vector<T>& collection, size_t groupSize, const Func& func) {
	std::vector<T> subGroup(groupSize);

	forEachSubgroupRecurse(collection, func, subGroup, 0, 0);
}

template<typename T>
std::vector<T> removeAll(const std::vector<T>& vec, const std::vector<T>& toRemove) {
	std::vector<T> result;
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

template<typename T>
std::vector<T> pop_front(const std::vector<T>& vec) {
	std::vector<T> result;
	result.reserve(vec.size() - 1);
	for(int i = 1; i < vec.size(); i++) {
		result.push_back(vec[i]);
	}
	return result;
}

template<typename T, typename F>
void forEachPermutationRecurse(std::vector<T>& vecToPermute, size_t keepBefore, const F& funcToRun) {
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
template<typename T, typename F> // pass by value, we're messing with this vector in forEachPermutationRecurse
void forEachPermutation(std::vector<T> vecToPermute, const F& funcToRun) {
	forEachPermutationRecurse(vecToPermute, 0, funcToRun);
}


template<typename T, typename F>
bool existsPermutationForWhichRecurse(std::vector<T>& vecToPermute, size_t keepBefore, const F& funcToRun) {
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
template<typename T, typename F> // pass by value, we're messing with this vector in forEachPermutationRecurse
bool existsPermutationForWhich(std::vector<T> vecToPermute, const F& funcToRun) {
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

template<typename T1, typename T2>
std::vector<std::pair<T1, T2>> zip(const std::vector<T1>& a, const std::vector<T2>& b) {
	assert(a.size() == b.size());
	std::vector<std::pair<T1, T2>> result(a.size());
	for(size_t i = 0; i < a.size(); i++) {
		result[i] = std::make_pair(a[i], b[i]);
	}
}
