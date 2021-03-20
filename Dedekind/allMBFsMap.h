#pragma once


#include "bufferedMap.h"
#include "booleanFunction.h"
#include "funcTypes.h"
#include "knownData.h"
#include "serialization.h"
#include <istream>
#include <string>


// expects a function of the form T(std::ifstream&)
template<unsigned int Variables, typename T, typename ValueDeserializer>
KeyValue<Monotonic<Variables>, T>* readBufFromFile(std::ifstream& mapFile, size_t layerIndex, const ValueDeserializer& deserializer) {
	size_t size = getLayerSize<Variables>(layerIndex);
	KeyValue<Monotonic<Variables>, T>* buf = new KeyValue<Monotonic<Variables>, T>[size];

	for(size_t i = 0; i < size; i++) {
		Monotonic<Variables> m = deserializeMBF<Variables>(mapFile);

		buf[i].key = m;
		buf[i].value = deserializer(mapFile);
	}

	return buf;
}

// expects a function of the form T(std::ifstream&)
template<unsigned int Variables, typename T, typename ValueDeserializer>
BakedMap<Monotonic<Variables>, T> readLayerFromFile(std::ifstream& mapFile, size_t layerIndex, const ValueDeserializer& deserializer) {
	size_t size = getLayerSize<Variables>(layerIndex);
	
	return BakedMap<Monotonic<Variables>, T>(readBufFromFile<Variables, T, ValueDeserializer>(mapFile, layerIndex, deserializer), size);
}

// expects a function of the form void(const T&, std::ofstream&)
template<unsigned int Variables, typename T, typename ValueSerializer>
void writeBufToFile(std::ofstream& mapFile, const KeyValue<Monotonic<Variables>, T>* buf, size_t layerIndex, const ValueSerializer& serializer) {
	size_t size = getLayerSize<Variables>(layerIndex);

	for(size_t i = 0; i < size; i++) {
		const KeyValue<Monotonic<Variables>, T>& cur = buf[i];
		serializeMBF<Variables>(cur.key, mapFile);
		serializer(cur.value, mapFile);
	}
}

// expects a function of the form void(const T&, std::ofstream&)
template<unsigned int Variables, typename T, typename ValueSerializer>
void writeLayerToFile(std::ofstream& mapFile, const BakedMap<Monotonic<Variables>, T>& map, size_t layerIndex, const ValueSerializer& serializer) {
	writeBufToFile<Variables, T, ValueSerializer>(mapFile, map.begin(), layerIndex, serializer);
}

template<unsigned int Variables>
void skipLayersInFile(std::ifstream& file, size_t startLayer, size_t endLayer, size_t extraDataSize = 0) {
	size_t sizePerElement = getMBFSizeInBytes<Variables>() + extraDataSize;

	std::streamsize totalToSkip = 0;
	for(size_t l = startLayer; l < endLayer; l++) {
		totalToSkip += getLayerSize<Variables>(l) * sizePerElement;
	}

	file.ignore(totalToSkip);
}

template<unsigned int Variables, typename T>
struct AllMBFMap {
	constexpr static int LAYER_COUNT = (1 << Variables) + 1;

	std::vector<BakedMap<Monotonic<Variables>, T>> layers;

	AllMBFMap() = default;
	AllMBFMap(std::vector<BakedMap<Monotonic<Variables>, T>>&& layers) : layers(std::move(layers)) {}

	static AllMBFMap readKeysFile(std::ifstream& mbfFile) {
		std::vector<BakedMap<Monotonic<Variables>, T>> layers(LAYER_COUNT);

		size_t maxLayerSize = getMaxLayerSize<Variables>();
		char* fileBuf = new char[maxLayerSize * getMBFSizeInBytes<Variables>()];

		for(int layer = 0; layer < LAYER_COUNT; layer++) {
			size_t size = getLayerSize<Variables>(layer);

			size_t fileBytes = size * getMBFSizeInBytes<Variables>();
			std::cout << "Read " << fileBytes << " file bytes" << std::endl;

			mbfFile.read(fileBuf, fileBytes);

			KeyValue<Monotonic<Variables>, T>* buf = new KeyValue<Monotonic<Variables>, T>[size];

			for(size_t i = 0; i < size; i++) {
				Monotonic<Variables> m = deserializeMBF<Variables>(reinterpret_cast<uint8_t*>(fileBuf + i * getMBFSizeInBytes<Variables>()));

				buf[i].key = m;
			}

			layers[layer] = BakedMap<Monotonic<Variables>, T>(buf, size);
		}
		delete[] fileBuf;
		return AllMBFMap(std::move(layers));
	}

	// expects a function of the form T(std::ifstream&)
	template<typename ValueDeserializer>
	static AllMBFMap readMapFile(std::ifstream& mapFile, const ValueDeserializer& deserializer) {
		std::vector<BakedMap<Monotonic<Variables>, T>> layers(LAYER_COUNT);
		for(int layer = 0; layer < LAYER_COUNT; layer++) {
			layers[layer] = readLayerFromFile<Variables, T, ValueDeserializer>(mapFile, layer, deserializer);
		}
		return AllMBFMap(std::move(layers));
	}

	// expects a function of the form void(const T&, std::ofstream&)
	template<typename ValueSerializer>
	void writeToFile(std::ofstream& mapFile, const ValueSerializer& serializer) {
		for(int layer = 0; layer < LAYER_COUNT; layer++) {
			writeLayerToFile(mapFile, this->layers[layer], layer, serializer);
		}
	}

	auto begin() const { return layers.begin(); }
	auto end() const { return layers.end(); }
};


