#pragma once

#include <vector>

#include "functionInput.h"

struct LayerStack {
	std::vector<std::vector<FunctionInput>> layers;
};

LayerStack generateLayers(size_t n) {
	std::vector<std::vector<FunctionInput>> result(n + 1);

	int32_t max = 1 << n;

	for(int32_t cur = 0; cur < max; cur++) {
		FunctionInput input{cur};

		int layer = input.getNumberEnabled();

		result[layer].push_back(input);
	}

	return LayerStack{std::move(result)};
}
