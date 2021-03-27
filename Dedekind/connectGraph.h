#pragma once


#include "funcTypes.h"
#include "smallVector.h"

template<unsigned int Variables>
SmallVector<Monotonic<Variables>, getMaxLayerWidth(Variables)> splitAC(const AntiChain<Variables>& ss) {
	SmallVector<Monotonic<Variables>, getMaxLayerWidth(Variables)> res;
	ss.forEachOne([&res](size_t index) {
		res.push_back(AntiChain<Variables>{index}.asMonotonic());
	});
	return res;
}

/*
	Finds the connected groups within a given list of monotonics where they are linked by links not on d

	Combines the elements of the given list such that
	for two achains a,b in the list, a & b <= d holds
	and for two sets A,B in an achain c in the list
	not ({A & B} <= d) holds.
*/
template<unsigned int Variables>
void connectFast(SmallVector<Monotonic<Variables>, getMaxLayerWidth(Variables)>& res, const Monotonic<Variables>& d) {
	for(size_t i = 0; i < res.size(); i++) {
		doAgain:;
		for(size_t j = i + 1; j < res.size(); j++) {
			if(!((res[i] & res[j]) <= d)) {
				res[i] = res[i] | res[j];
				res[j] = res.back();
				res.pop_back();
				goto doAgain;
			}
		}
	}
}

/*
	Finds the connected groups within a given antichain ss where they are linked by links not on d

	Generate the list of subset achains of the achain ss such that
	for two achains a,b in the list, a & b <= d holds
	and for two sets A,B in an achain c in the list
	not ({A & B} <= d) holds.
*/
template<unsigned int Variables>
SmallVector<Monotonic<Variables>, getMaxLayerWidth(Variables)> connectFast(const AntiChain<Variables>& ss, const Monotonic<Variables>& d) {
	SmallVector<Monotonic<Variables>, getMaxLayerWidth(Variables)> res = splitAC(ss);

	connectFast(res, d);

	return res;
}

/*
	Counts the connected groups within a given list of monotonics where they are linked by links not on d

	Count the resulting size of the list of subsets res
	for two achains a,b in the list, a & b <= d holds
	and for two sets A,B in an achain c in the list
	not ({A & B} <= d) holds.
*/
template<unsigned int Variables>
size_t countConnected(SmallVector<Monotonic<Variables>, getMaxLayerWidth(Variables)>& res, const Monotonic<Variables>& d) {
	connectFast(res, d);
	return res.size();
}

/*
	Counts the connected groups within a given antichain ss where they are linked by links not on d

	Count the size of the list of subset achains of the achain ss such that
	for two achains a,b in the list, a & b <= d holds
	and for two sets A,B in an achain c in the list
	not ({A & B} <= d) holds.
*/
template<unsigned int Variables>
size_t countConnected(const AntiChain<Variables>& ss, const Monotonic<Variables>& d) {
	return connectFast(ss, d).size();
}

