#pragma once


#include "bufferedMap.h"
#include "booleanFunction.h"
#include "funcTypes.h"
#include "knownData.h"
#include "serialization.h"
#include <istream>
#include <string>

template<unsigned int Variables, typename T>
struct AllMBFMap {
	constexpr static int LAYER_COUNT = (1 << Variables) + 1;

	std::vector<BakedMap<Monotonic<Variables>, T>> layers;

	AllMBFMap() = default;
	AllMBFMap(std::vector<BakedMap<Monotonic<Variables>, T>>&& layers) : layers(std::move(layers)) {}

	static AllMBFMap readKeysFile(std::ifstream& mbfFile) {
		std::vector<BakedMap<Monotonic<Variables>, T>> layers(LAYER_COUNT);
		for(int layer = 0; layer < LAYER_COUNT; layer++) {
			size_t size = getLayerSize<Variables>(layer);
			KeyValue<Monotonic<Variables>, T>* buf = new KeyValue<Monotonic<Variables>, T>[size];

			for(size_t i = 0; i < size; i++) {
				Monotonic<Variables> m = deserializeMBF<Variables>(mbfFile);

				buf[i].key = m;
			}

			layers[layer] = BakedMap<Monotonic<Variables>, T>(buf, size);
		}
		return AllMBFMap(std::move(layers));
	}

	// expects a function of the form Monotonic<Variables>(std::ifstream&)
	template<typename ValueDeserializer>
	static AllMBFMap readMapFile(std::ifstream& mapFile, const ValueDeserializer& deserializer) {
		std::vector<BakedMap<Monotonic<Variables>, T>> layers(LAYER_COUNT);
		for(int layer = 0; layer < LAYER_COUNT; layer++) {
			size_t size = getLayerSize<Variables>(layer);
			KeyValue<Monotonic<Variables>, T>* buf = new KeyValue<Monotonic<Variables>, T>[size];

			for(size_t i = 0; i < size; i++) {
				Monotonic<Variables> m = deserializeMBF<Variables>(mapFile);

				buf[i].key = m;
				buf[i].value = deserializer(mapFile);
			}

			layers[layer] = BakedMap<Monotonic<Variables>, T>(buf, size);
		}
		return AllMBFMap(std::move(layers));
	}

	// expects a function of the form void(const T&, std::ofstream&)
	template<typename ValueSerializer>
	void writeToFile(std::ofstream& mapFile, const ValueSerializer& serializer) {
		for(int layer = 0; layer < LAYER_COUNT; layer++) {
			size_t size = getLayerSize<Variables>(layer);

			for(size_t i = 0; i < size; i++) {
				KeyValue<Monotonic<Variables>, T>& cur = this->layers[layer][i];
				serializeMBF<Variables>(cur.key, mapFile);
				serializer(cur.value, mapFile);
			}
		}
	}
};


