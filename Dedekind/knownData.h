#pragma once

constexpr unsigned int factorial(unsigned int value) {
	unsigned int total = 1;
	for(unsigned int i = 2; i <= value; i++) {
		total *= i;
	}
	return total;
}
constexpr unsigned int choose(unsigned int from, unsigned int count) {
	if(count <= from / 2) count = from - count;
	unsigned int total = 1;
	for(unsigned int i = count + 1; i <= from; i++) {
		total *= i;
	}
	total /= factorial(from - count);
	return total;
}

constexpr size_t mbfCounts[]{2, 3, 5, 10, 30, 210, 16353, 490013148};
constexpr size_t MAX_EXPANSION = 40; // greater than 35, which is the max for 7, leave some leeway

constexpr size_t dedekindNumbers[]{2, 3, 6, 20, 168, 7581, 7828354, 2414682040998/*, 56130437228687557907788, ??????????????????????????????????????????*/}; //need bigint for D(8)

constexpr size_t layerSizes1[]{1, 1, 1};
constexpr size_t layerSizes2[]{1, 1, 1, 1, 1};
constexpr size_t layerSizes3[]{1, 1, 1, 1, 2, 1, 1, 1, 1};
constexpr size_t layerSizes4[]{1, 1, 1, 1, 2, 2, 2, 3, 4, 3, 2, 2, 2, 1, 1, 1, 1};
constexpr size_t layerSizes5[]{1, 1, 1, 1, 2, 2, 3, 4, 6, 7, 9, 10, 12, 12, 13, 13, 16, 13, 13, 12, 12, 10, 9, 7, 6, 4, 3, 2, 2, 1, 1, 1, 1};
constexpr size_t layerSizes6[]{1, 1, 1, 1, 2, 2, 3, 5, 7, 9, 14, 20, 29, 39, 53, 69, 93, 116, 146, 180, 225, 269, 328, 387, 459, 529, 611, 686, 769, 832, 892, 923, 951, 923, 892, 832, 769, 686, 611, 529, 459, 387, 328, 269, 225, 180, 146, 116, 93, 69, 53, 39, 29, 20, 14, 9, 7, 5, 3, 2, 2, 1, 1, 1, 1};
constexpr size_t layerSizes7[]{1, 1, 1, 1, 2, 2, 3, 5, 8, 10, 16, 25, 40, 62, 101, 156, 249, 385, 594, 899, 1367, 2036, 3032, 4468, 6571, 9572, 13933, 20131, 29014, 41556, 59306, 84099, 118719, 166406, 231794, 320296, 439068, 596093, 801197, 1064468, 1396828, 1807837, 2305705, 2894434, 3574182, 4338853, 5177869, 6075361, 7013439, 7971830, 8931651, 9874300, 10784604, 11648558, 12456475, 13199408, 13872321, 14470219, 14991439, 15433196, 15795759, 16077423, 16279195, 16399768, 16440466, 16399768, 16279195, 16077423, 15795759, 15433196, 14991439, 14470219, 13872321, 13199408, 12456475, 11648558, 10784604, 9874300, 8931651, 7971830, 7013439, 6075361, 5177869, 4338853, 3574182, 2894434, 2305705, 1807837, 1396828, 1064468, 801197, 596093, 439068, 320296, 231794, 166406, 118719, 84099, 59306, 41556, 29014, 20131, 13933, 9572, 6571, 4468, 3032, 2036, 1367, 899, 594, 385, 249, 156, 101, 62, 40, 25, 16, 10, 8, 5, 3, 2, 2, 1, 1, 1, 1};

constexpr size_t layerWidths[]{1, 1, 2, 3, 6, 10, 20, 35, 70};

template<unsigned int Variables>
constexpr size_t getLayerSize(size_t layer) {
	if constexpr(Variables == 1) {
		return layerSizes1[layer];
	} else if constexpr(Variables == 2) {
		return layerSizes2[layer];
	} else if constexpr(Variables == 3) {
		return layerSizes3[layer];
	} else if constexpr(Variables == 4) {
		return layerSizes4[layer];
	} else if constexpr(Variables == 5) {
		return layerSizes5[layer];
	} else if constexpr(Variables == 6) {
		return layerSizes6[layer];
	} else if constexpr(Variables == 7) {
		return layerSizes7[layer];
	} else {
		static_assert(Variables <= 7 && Variables != 0, "Not defined for >7");
	}
}

template<unsigned int Variables>
constexpr size_t getMaxLayerSize() {
	return getLayerSize<Variables>((1 << Variables) / 2);
}



constexpr size_t linkCounts1[]{1, 1};
constexpr size_t linkCounts2[]{1, 1, 1, 1};
constexpr size_t linkCounts3[]{1, 1, 1, 2, 2, 1, 1, 1};
constexpr size_t linkCounts4[]{1, 1, 1, 2, 3, 3, 4, 6, 6, 4, 3, 3, 2, 1, 1, 1};
constexpr size_t linkCounts5[]{1, 1, 1, 2, 3, 4, 6, 10, 15, 20, 25, 30, 35, 37, 38, 42, 42, 38, 37, 35, 30, 25, 20, 15, 10, 6, 4, 3, 2, 1, 1, 1};
constexpr size_t linkCounts6[]{1, 1, 1, 2, 3, 4, 7, 12, 19, 31, 51, 82, 127, 190, 269, 376, 510, 670, 867, 1112, 1405, 1754, 2169, 2650, 3195, 3807, 4456, 5137, 5791, 6368, 6804, 7068, 7068, 6804, 6368, 5791, 5137, 4456, 3807, 3195, 2650, 2169, 1754, 1405, 1112, 867, 670, 510, 376, 269, 190, 127, 82, 51, 31, 19, 12, 7, 4, 3, 2, 1, 1, 1};
constexpr size_t linkCounts7[]{1, 1, 1, 2, 3, 4, 7, 13, 21, 35, 63, 113, 204, 368, 654, 1143, 1962, 3273, 5335, 8541, 13470, 20929, 32199, 49111, 74334, 111817, 167274, 248888, 368438, 542617, 794726, 1157219, 1674356, 2405346, 3428465, 4844205, 6777795, 9380040, 12824514, 17297834, 22985529, 30048791, 38596505, 48657595, 60160556, 72926502, 86684609, 101101716, 115817231, 130479629, 144772616, 158432622, 171253992, 183090657, 193847879, 203474411, 211951993, 219286796, 225498415, 230615206, 234666381, 237678987, 239674579, 240668898, 240668898, 239674579, 237678987, 234666381, 230615206, 225498415, 219286796, 211951993, 203474411, 193847879, 183090657, 171253992, 158432622, 144772616, 130479629, 115817231, 101101716, 86684609, 72926502, 60160556, 48657595, 38596505, 30048791, 22985529, 17297834, 12824514, 9380040, 6777795, 4844205, 3428465, 2405346, 1674356, 1157219, 794726, 542617, 368438, 248888, 167274, 111817, 74334, 49111, 32199, 20929, 13470, 8541, 5335, 3273, 1962, 1143, 654, 368, 204, 113, 63, 35, 21, 13, 7, 4, 3, 2, 1, 1, 1};

template<unsigned int Variables>
constexpr size_t getLinkCount(size_t layer) {
	if constexpr(Variables == 1) {
		return linkCounts1[layer];
	} else if constexpr(Variables == 2) {
		return linkCounts2[layer];
	} else if constexpr(Variables == 3) {
		return linkCounts3[layer];
	} else if constexpr(Variables == 4) {
		return linkCounts4[layer];
	} else if constexpr(Variables == 5) {
		return linkCounts5[layer];
	} else if constexpr(Variables == 6) {
		return linkCounts6[layer];
	} else if constexpr(Variables == 7) {
		return linkCounts7[layer];
	} else {
		static_assert(Variables <= 7 && Variables != 0, "Not defined for >7");
	}
}

template<unsigned int Variables>
constexpr size_t getMaxLinkCount() {
	return getLinkCount<Variables>((1 << Variables) / 2);
}
template<unsigned int Variables>
constexpr size_t getTotalLinkCount() {
	size_t totalLinkCount = 0;
	for(size_t size = 0; size < (1 << Variables); size++) {
		totalLinkCount += getLinkCount<Variables>(size);
	}
	return totalLinkCount;
}
