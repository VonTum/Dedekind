#pragma once

#include <vector>

#include "functionInput.h"
#include "functionInputSet.h"

struct LayerStack {
	std::vector<FullLayer> layers;
};

// returns a LayerStack of size n+1
inline LayerStack generateLayers(size_t n) {
	std::vector<FullLayer> result(n + 1);

	FunctionInput::underlyingType max = 1 << n;

	for(FunctionInput::underlyingType cur = 0; cur < max; cur++) {
		FunctionInput input{cur};

		int layer = input.getNumberEnabled();

		result[layer].push_back(input);
	}

	return LayerStack{std::move(result)};
}
