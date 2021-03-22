#pragma once


#include "funcTypes.h"
#include <vector>

/*
	Finds the connected groups within a given antichain ss where they are linked by links not on d

	Generate the list of subset achains of the achain ss such that
	for two achains a,b in the list, a & b <= d holds
	and for two sets A,B in an achain c in the list
	not ({A & B} <= d) holds.
*/
template<unsigned int Variables>
std::vector<Monotonic<Variables>> connect(const AntiChain<Variables>& ss, const Monotonic<Variables>& d) {
	using AC = AntiChain<Variables>;
	using MBF = Monotonic<Variables>;

	std::vector<MBF> res;
	ss.forEachOne([&res](size_t index) {
		res.push_back(AC{index}.asMonotonic());
	});

	bool running = true;
	while(running) {
		running = false;
		std::vector<MBF> vres;
		std::vector<MBF> nres;
		for(size_t i = 0; i < res.size(); i++) {
			for(size_t j = i + 1; j < res.size(); j++) {
				if(!((res[i] & res[j]) <= d)) {
					running = true;
					MBF nnode = res[i] | res[j];
					if(nnode != res[i]) vres.push_back(res[i]);
					if(nnode != res[j]) vres.push_back(res[j]);
					nres.push_back(nnode);
				}
			}
		}

		for(const MBF& newItem : nres) {
			for(const MBF& existingItem : res) {
				if(newItem == existingItem) {
					goto alreadyExists;
				}
			}
			// does not exist yet
			res.push_back(newItem);
			alreadyExists:;
		}

		for(size_t i = 0; i < res.size(); ) {
			for(const MBF& vresItem : vres) {
				if(vresItem == res[i]) {
					res[i] = res.back();
					res.pop_back();
					goto found;
				}
			}
			i++;
			found:;
		}
	}

	return res;
}

/*
	Finds the connected groups within a given antichain ss where they are linked by links not on d

	Generate the list of subset achains of the achain ss such that
	for two achains a,b in the list, a & b <= d holds
	and for two sets A,B in an achain c in the list
	not ({A & B} <= d) holds.
*/
template<unsigned int Variables>
std::vector<Monotonic<Variables>> connectFast(const AntiChain<Variables>& ss, const Monotonic<Variables>& d) {
	using AC = AntiChain<Variables>;
	using MBF = Monotonic<Variables>;

	std::vector<MBF> res;
	ss.forEachOne([&res](size_t index) {
		res.push_back(AC{index}.asMonotonic());
	});

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

	return res;
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

