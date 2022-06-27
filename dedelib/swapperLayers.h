#pragma once

#include "knownData.h"

template<unsigned int Variables, typename T>
class SwapperLayers {
	static constexpr size_t MAX_SIZE = getMaxLayerSize(Variables);

	T* __restrict__ sourceList;
	T* __restrict__ destinationList;

	T defaultValue;
	int currentLayer;

	SwapperLayers(T defaultValue, T* sourceList, T* destinationList, int currentLayer) :
		sourceList(sourceList),
		destinationList(destinationList),
		defaultValue(defaultValue),
		currentLayer(currentLayer) {}
public:
	size_t getSourceLayerSize() const {
		return layerSizes[Variables][currentLayer];
	}
	size_t getDestinationLayerSize() const {
		return layerSizes[Variables][currentLayer - 1];
	}
private:
	void clearSource() {
		for(size_t i = 0; i < getSourceLayerSize(); i++) {
			sourceList[i] = defaultValue;
		}
	}
	void clearDestination() {
		for(size_t i = 0; i < getDestinationLayerSize(); i++) {
			destinationList[i] = defaultValue;
		}
	}
public:
	void resetUpward(int startingLayer = 0) {
		currentLayer = (1 << Variables) - startingLayer;
		clearSource();
		clearDestination();
	}

	void resetDownward(int startingLayer = 0) {
		currentLayer = startingLayer;
		clearSource();
		clearDestination();
	}

	SwapperLayers(T defaultValue = T()) : SwapperLayers(defaultValue, new T[MAX_SIZE], new T[MAX_SIZE], -1) {
		for(size_t i = 0; i < MAX_SIZE; i++) {
			sourceList[i] = defaultValue;
			destinationList[i] = defaultValue;
		}
	}
	
	~SwapperLayers() {
		delete[] sourceList;
		delete[] destinationList;
	}

	SwapperLayers(const SwapperLayers& other) : SwapperLayers(other.defaultValue) {
		for(size_t i = 0; i < MAX_SIZE; i++) {
			this->sourceList[i] = other.sourceList[i];
			this->destinationList[i] = other.destinationList[i];
		}
		this->currentLayer = other.currentLayer;
	}
	SwapperLayers& operator=(const SwapperLayers& other) {
		for(size_t i = 0; i < MAX_SIZE; i++) {
			this->sourceList[i] = other.sourceList[i];
			this->destinationList[i] = other.destinationList[i];
		}
		this->currentLayer = other.currentLayer;
		this->defaultValue = other.defaultValue;
		return *this;
	}
	SwapperLayers(SwapperLayers&& other) noexcept : SwapperLayers(other.defaultValue, other.sourceList, other.destinationList, other.currentLayer) {
		other.sourceList = nullptr;
		other.destinationList = nullptr;
	}
	SwapperLayers& operator=(SwapperLayers&& other) noexcept {
		std::swap(this->sourceList, other.sourceList);
		std::swap(this->destinationList, other.destinationList);
		std::swap(this->defaultValue, other.defaultValue);
		std::swap(this->currentLayer, other.currentLayer);
		return *this;
	}

	int getCurrentLayerUpward() const {
		return (1 << Variables) - currentLayer;
	}
	int getCurrentLayerDownward() const {
		return currentLayer;
	}

	bool canPushNext() const {
		return currentLayer > 0;
	}

	void pushNext() {
		assert(canPushNext()); // trying to pushNext when already at the end
		currentLayer--;
		std::swap(sourceList, destinationList);

		clearDestination();
	}

	T& source(size_t index) { assert(index < getSourceLayerSize()); return sourceList[index]; }
	const T& source(size_t index) const { assert(index < getSourceLayerSize()); return sourceList[index]; }

	T& dest(size_t index) { assert(canPushNext()); assert(index < getDestinationLayerSize()); return destinationList[index]; }
	const T& dest(size_t index) const { assert(canPushNext()); assert(index < getDestinationLayerSize()); return destinationList[index]; }
};
