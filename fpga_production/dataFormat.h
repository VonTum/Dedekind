
#include <cstdint>
#include <vector>

struct DataItem {
	bool isTop;
	uint64_t botOrTopUpper;
	uint64_t botOrTopLower;
	uint64_t connectCount;
	uint64_t numTerms;
};

// Generated
extern std::vector<DataItem> allData;
