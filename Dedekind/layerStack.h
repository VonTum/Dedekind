#pragma once

#include <vector>

#include "functionInput.h"

struct LayerStack {
	std::vector<set<FunctionInput>> layers;
};

LayerStack generateLayers(size_t n) {
	std::vector<set<FunctionInput>> result(n + 1);

	uint32_t max = 1 << n;

	for(uint32_t cur = 0; cur < max; cur++) {
		FunctionInput input{cur};

		int layer = input.getNumberEnabled();

		result[layer].push_back(input);
	}

	return LayerStack{std::move(result)};
}
