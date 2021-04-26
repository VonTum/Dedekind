#pragma once

#include "knownData.h"
#include <vector>

template<unsigned int Variables, typename T>
struct SwapperLayers {
	static constexpr size_t MAX_SIZE = getMaxLayerSize<Variables>();

	using IndexType = uint32_t;

	T defaultValue;

	std::vector<T> sourceList;
	std::vector<T> destinationList;

	std::vector<IndexType> dirtySourceList;
	std::vector<IndexType> dirtyDestinationList;

	SwapperLayers(T defaultValue = 0) :
		defaultValue(defaultValue),
		sourceList(MAX_SIZE),
		destinationList(MAX_SIZE),
		dirtySourceList(0),
		dirtyDestinationList(0) {

		for(size_t i = 0; i < MAX_SIZE; i++) {
			sourceList[i] = defaultValue;
			destinationList[i] = defaultValue;
		}
	}

	void clearSource() {
		for(IndexType indexToClear : dirtySourceList) {
			sourceList[indexToClear] = defaultValue;
		}

		dirtySourceList.clear();
	}
	void clearDestination() {
		for(IndexType indexToClear : dirtyDestinationList) {
			destinationList[indexToClear] = defaultValue;
		}

		dirtyDestinationList.clear();
	}

	void pushNext() {
		std::swap(sourceList, destinationList);
		std::swap(dirtySourceList, dirtyDestinationList);

		for(IndexType indexToClear : dirtyDestinationList) {
			destinationList[indexToClear] = defaultValue;
		}

		dirtyDestinationList.clear();
	}

	// expects a function of the form void(IndexType index, const T& item)
	// the destination may be altered while this is running
	template<typename Func>
	void forEachSourceElement(const Func& func) {
		for(const IndexType& dirtyIndex : dirtySourceList) {
			func(dirtyIndex, sourceList[dirtyIndex]);
		}
	}

	auto begin() { return this->dirtySourceList.begin(); }
	const auto begin() const { return this->dirtySourceList.begin(); }
	auto end() { return this->dirtySourceList.end(); }
	const auto end() const { return this->dirtySourceList.end(); }

	T& operator[](IndexType index) { assert(index < MAX_SIZE); return sourceList[index]; }
	const T& operator[](IndexType index) const { assert(index < MAX_SIZE); return sourceList[index]; }

	T& dest(IndexType index) {
		assert(index < MAX_SIZE);
		if(destinationList[index] == defaultValue) {
			dirtyDestinationList.push_back(index);
		}

		return destinationList[index];
	}

	void set(IndexType to) {
		assert(to < MAX_SIZE);
		if(destinationList[to] == defaultValue) {
			dirtyDestinationList.push_back(to);
		}

		destinationList[to] = true;
	}
};
