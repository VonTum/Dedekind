#pragma once


#include "interval.h"

#include <array>
#include <cstdint>

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
			for(size_t j = i+1; j < res.size(); j++) {
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
The number of simultaneous solutions to equations of the form
x1 | x2 = tau[0], x1 | x3 = tau[1], x2 | x3 = tau[2]
and
x1 & x2 & x3 = delta

See factorizing paper, Theorem Three-join-one-meet (TJOM) number
*/
template<unsigned int Variables>
uint64_t threejoinmeetnumberveryfast(const AntiChain<Variables>& t0, const AntiChain<Variables>& t1, const AntiChain<Variables>& t2, const Monotonic<Variables>& delta) {
	using AC = AntiChain<Variables>;
	using MBF = Monotonic<Variables>;
	
	if(!(delta <= (t0.asMonotonic() & t1.asMonotonic() & t2.asMonotonic()))) return 0;

	AC r01 = t0.intersection(t1);
	AC r02 = t0.intersection(t2);
	AC r12 = t1.intersection(t2);

	AC rhoIntersect = r01.intersection(r02).intersection(r12);
	AC diffTerms = ((t0 - r01 - r02).asMonotonic() | (t1 - r01 - r12).asMonotonic() | (t2 - r02 - r12).asMonotonic()).asAntiChain();
	AC rhoJoin = (r01.asMonotonic() | r02.asMonotonic() | r12.asMonotonic()).asAntiChain();

	AC epsilonsigma = (rhoIntersect.asMonotonic() | (diffTerms - rhoJoin).asMonotonic()).asAntiChain();

	r01 -= epsilonsigma;
	r02 -= epsilonsigma;
	r12 -= epsilonsigma;

	std::vector<MBF> connectedSets = connectFast(epsilonsigma - delta.asAntiChain(), delta);

	// bound to 64 bit integer, for even D(7) this can at most be 3^(7 choose 3) (7 choose 3 is max antiChain size, hence max size of connectedSets)
	uint64_t res = 1;
	for(const MBF& es : connectedSets) {
		unsigned int nr = 0;
		if((es & r01.asMonotonic()) <= delta) nr++;
		if((es & r02.asMonotonic()) <= delta) nr++;
		if((es & r12.asMonotonic()) <= delta) nr++;
		res *= nr;
	}
	return res;
}
