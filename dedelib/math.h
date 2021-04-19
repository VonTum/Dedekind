#pragma once

#include <cstdint>
#include <vector>
#include <cassert>

int64_t factorial(int n) {
	int64_t total = 1;
	for(int i = 2; i <= n; i++) {
		total *= i;
	}
	return total;
}

int choose(int of, int num) {
	assert(of > 0 && num > 0 && num <= of);
	if(num <= of / 2) {
		num = of - num;
	}
	int64_t total = 1;
	for(int i = num + 1; i <= of; i++) {
		total *= i;
	}
	total /= factorial(of - num);
	assert(total <= std::numeric_limits<int>::max());
	return int(total);
}


std::vector<int> computeLayerSizes(int n) {
	n++;
	std::vector<int> layerSizes(n);

	for(int i = 0; i < n; i++) {
		layerSizes[i] = choose(n, i + 1);
	}

	return layerSizes;
}

