/**
* The algorithms described in this file are the work of Patrick De Causmaeker.
*
* https://www.kuleuven.be/wieiswie/en/person/00003471
* https://arxiv.org/pdf/1103.2877v1.pdf
* https://arxiv.org/pdf/1407.4288.pdf
* https://arxiv.org/pdf/1602.04675v1.pdf
*/

#pragma once


#include "interval.h"
#include "intervalSizeCache.h"
#include "MBFDecomposition.h"
#include "u192.h"

#include <array>
#include <cstdint>
#include <map>

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

// expects a function of the form void(const AntiChain<Variables>& p0, const AntiChain<Variables>& p1, const AntiChain<Variables>& p2, const AntiChain<Variables>& p3, unsigned int classSize)
// classSize is either 1, 3, or 6
template<unsigned int Variables, typename Func>
void fourPartNonEquivalent(const AntiChain<Variables>& v, const Func& func) {
	using AC = AntiChain<Variables>;
	using BF = BooleanFunction<Variables>;

	AC e = AC{};
	v.forEachSubSet([&](AC p4) {
		AC l0 = v - p4;
		if(l0.isEmpty()) {
			func(e, e, e, p4, 1);
			return;
		}
		size_t f1 = l0.getFirst();
		l0.remove(f1);
		l0.forEachSubSet([&](AC p1) {
			AC l1 = l0 - p1;
			p1.add(f1);
			if(l1.isEmpty()) {
				func(p1, e, e, p4, 3);
				return;
			}
			size_t f2 = l1.getFirst();
			l1.remove(f2);
			l1.forEachSubSet([&](AC p2) {
				AC p3 = l1 - p2;
				p2.add(f2);
				func(p1, p2, p3, p4, 6);
			});
		});
	});
}

// expects a function of the form void(const AntiChain<Variables>& p0, const AntiChain<Variables>& p1, const AntiChain<Variables>& p2, const AntiChain<Variables>& p3, unsigned int classSize)
// classSize is either 1, 3, or 6
template<unsigned int Variables, typename Func>
void fourPartNonEquivalentWithBuffers(const AntiChain<Variables>& v, BufferedMap<AntiChain<Variables>, int>& bufSet, const Func& func) {
	using AC = AntiChain<Variables>;
	using BF = BooleanFunction<Variables>;

	
	bufSet.clear();
	v.forEachSubSet([&](const AC& p4) {
		AC canon = AC(p4.bf.canonizePreserving(v.bf));
		KeyValue<AC, int>* found = bufSet.find(canon);
		if(found) {
			found->value++;
		} else {
			bool wasAdded = bufSet.add(canon, 1);
			assert(wasAdded);
		}
	});

	AC e = AC{};
	for(const KeyValue<AC, int>& item : bufSet) {
		AC p4 = item.key;
		AC l0 = v - p4;
		if(l0.isEmpty()) {
			func(e, e, e, p4, 1 * item.value);
			continue;
		}
		size_t f1 = l0.getFirst();
		l0.remove(f1);
		l0.forEachSubSet([&](AC p1) {
			AC l1 = l0 - p1;
			p1.add(f1);
			if(l1.isEmpty()) {
				func(p1, e, e, p4, 3 * item.value);
				return;
			}
			size_t f2 = l1.getFirst();
			l1.remove(f2);
			l1.forEachSubSet([&](AC p2) {
				AC p3 = l1 - p2;
				p2.add(f2);
				func(p1, p2, p3, p4, 6 * item.value);
			});
		});
	}
}

// expects a function of the form void(const Monotonic<Variables>& tau0, const Monotonic<Variables>& tau1, const Monotonic<Variables>& tau2, const Monotonic<Variables>& minDelta, const Monotonic<Variables>& maxDelta, unsigned int nr)
template<unsigned int Variables, typename Func>
void generateTaus(const Monotonic<Variables>& veem, const Func& funcToRun) {
	using AC = AntiChain<Variables>;
	using MBF = Monotonic<Variables>;
	using INT = Interval<Variables>;

	const AntiChain<Variables>& vee = veem.asAntiChain();
	fourPartNonEquivalent(vee, [&](const AC& p0, const AC& p1, const AC& p2, const AC& p3, unsigned int nr) {
		MBF fp0 = p0.asMonotonic();
		MBF fp1 = p1.asMonotonic();
		MBF fp2 = p2.asMonotonic();
		MBF fp3 = p3.asMonotonic();

		MBF tau01 = fp0 | fp1 | fp3;
		MBF tau02 = fp0 | fp2 | fp3;
		MBF tau12 = fp1 | fp2 | fp3;

		INT i2(tau01, tau01 | fp2.pred());
		INT i1(tau02, tau02 | fp1.pred());
		INT i0(tau12, tau12 | fp0.pred());

		// these indices are reversed in the original code, perhaps fix in a later iteration
		i2.forEach([&](const MBF& rt0) {
			i1.forEach([&](const MBF& rt1) {
				i0.forEach([&](const MBF& rt2) {
					MBF minDelta = fp0 & fp1 & fp2;
					MBF maxDelta = rt0 & rt1 & rt2;
					funcToRun(rt0, rt1, rt2, minDelta, maxDelta, nr);
				});
			});
		});
	});
}

// expects a function of the form void(const Monotonic<Variables>& tau0, const Monotonic<Variables>& tau1, const Monotonic<Variables>& tau2, const Monotonic<Variables>& minDelta, const Monotonic<Variables>& maxDelta, unsigned int nr)
template<unsigned int Variables, typename Func>
void generateTausWithBuffers(const Monotonic<Variables>& veem, BufferedMap<AntiChain<Variables>, int>& bufSet, const Func& funcToRun) {
	using AC = AntiChain<Variables>;
	using MBF = Monotonic<Variables>;
	using INT = Interval<Variables>;

	const AntiChain<Variables>& vee = veem.asAntiChain();
	fourPartNonEquivalentWithBuffers(vee, bufSet, [&](const AC& p0, const AC& p1, const AC& p2, const AC& p3, unsigned int nr) {
		MBF fp0 = p0.asMonotonic();
		MBF fp1 = p1.asMonotonic();
		MBF fp2 = p2.asMonotonic();
		MBF fp3 = p3.asMonotonic();

		MBF tau01 = fp0 | fp1 | fp3;
		MBF tau02 = fp0 | fp2 | fp3;
		MBF tau12 = fp1 | fp2 | fp3;

		INT i2(tau01, tau01 | fp2.pred());
		INT i1(tau02, tau02 | fp1.pred());
		INT i0(tau12, tau12 | fp0.pred());

		// these indices are reversed in the original code, perhaps fix in a later iteration
		i2.forEach([&](const MBF& rt0) {
			i1.forEach([&](const MBF& rt1) {
				i0.forEach([&](const MBF& rt2) {
					MBF minDelta = fp0 & fp1 & fp2;
					MBF maxDelta = rt0 & rt1 & rt2;
					funcToRun(rt0, rt1, rt2, minDelta, maxDelta, nr);
				});
			});
		});
	});
}

template<unsigned int Variables>
BufferedMap<Monotonic<Variables>, int> generateNonEquivalentMonotonics() {
	using MBF = Monotonic<Variables>;

	BufferedMap<MBF, int> result(mbfCounts[Variables]);

	forEachMonotonicFunction<Variables>([&](const MBF& mbf) {
		MBF canon = mbf.canonize();
		KeyValue<MBF, int>* found = result.find(canon);
		if(found) {
			found->value++;
		} else {
			result.add(canon, 1);
		}
	});

	return result;
}

template<unsigned int Variables>
struct TJOMN {
	Monotonic<Variables> taus[3];
	Monotonic<Variables> delta;
};

template<unsigned int Variables>
bool operator==(const TJOMN<Variables>& a, const TJOMN<Variables>& b) {
	for(size_t i = 0; i < 3; i++) {
		if(a.taus[i] != b.taus[i]) return false;
	}
	return a.delta == b.delta;
}
template<unsigned int Variables>
bool operator!=(const TJOMN<Variables>& a, const TJOMN<Variables>& b) {
	return !(a == b);
}
template<unsigned int Variables>
bool operator<(const TJOMN<Variables>& a, const TJOMN<Variables>& b) {
	for(size_t i = 0; i < 3; i++) {
		if(a.taus[i].bf.bitset != b.taus[i].bf.bitset) {
			return a.taus[i].bf.bitset < b.taus[i].bf.bitset;
		}
	}
	return a.delta.bf.bitset < b.delta.bf.bitset;
}
template<unsigned int Variables>
bool operator>(const TJOMN<Variables>& a, const TJOMN<Variables>& b) {
	return b < a;
}
template<unsigned int Variables>
bool operator>=(const TJOMN<Variables>& a, const TJOMN<Variables>& b) {
	return !(a < b);
}
template<unsigned int Variables>
bool operator<=(const TJOMN<Variables>& a, const TJOMN<Variables>& b) {
	return !(b < a);
}

template<unsigned int Variables>
std::ostream& operator<<(std::ostream& os, const TJOMN<Variables>& tj) {
	os << "{" << tj.taus[0] << ", " << tj.taus[1] << ", " << tj.taus[2] << "}, " << tj.delta << "}";
	return os;
}

struct TJOMNInfo {
	uint64_t eqClassSize;
	uint64_t solutionCount;
};

inline std::ostream& operator<<(std::ostream& os, const TJOMNInfo& tj) {
	os << "{eqClassSize=" <<tj.eqClassSize << ", solutionCount=" << tj.solutionCount << "}";
	return os;
}

template<unsigned int Variables>
std::map<TJOMN<Variables>, TJOMNInfo> threeJoinOneMeetDecompositionsFast() {
	using AC = AntiChain<Variables>;
	using MBF = Monotonic<Variables>;

	std::map<TJOMN<Variables>, TJOMNInfo> result;
	BufferedMap<MBF, int> alltaus = generateNonEquivalentMonotonics<Variables>();
	for(const KeyValue<MBF, int>& veetau : alltaus) {
		generateTaus(veetau.key, [&](const MBF& tau0, const MBF& tau1, const MBF& tau2, const MBF& minDelta, const MBF& maxDelta, unsigned int nr) {
			Interval<Variables> i(minDelta, maxDelta);
			AC tac0 = tau0.asAntiChain();
			AC tac1 = tau1.asAntiChain();
			AC tac2 = tau2.asAntiChain();
			i.forEach([&](const MBF& delta) {
				uint64_t tjomn = threejoinmeetnumberveryfast(tac0, tac1, tac2, delta);
				if(tjomn != 0) {
					TJOMN<Variables> key{{tau0, tau1, tau2}, delta};
					result.insert(std::make_pair(key, TJOMNInfo{veetau.value * nr, tjomn}));
				}
			});
		});
	}

	return result;
}

// expects a function of the form void(const MBF& tau0, const MBF& tau1, const MBF& tau2, const MBF& delta, uint64_t eqClassSize, uint64_t solutionCount)
template<unsigned int Variables, typename Func>
void forEachTJOMNFast(const Func& func) {
	using AC = AntiChain<Variables>;
	using MBF = Monotonic<Variables>;

	BufferedMap<MBF, int> alltaus = generateNonEquivalentMonotonics<Variables>();
	for(const KeyValue<MBF, int>& veetau : alltaus) {
		generateTaus(veetau.key, [&](const MBF& tau0, const MBF& tau1, const MBF& tau2, const MBF& minDelta, const MBF& maxDelta, unsigned int nr) {
			Interval<Variables> i(minDelta, maxDelta);
			AC tac0 = tau0.asAntiChain();
			AC tac1 = tau1.asAntiChain();
			AC tac2 = tau2.asAntiChain();
			i.forEach([&](const MBF& delta) {
				uint64_t tjomn = threejoinmeetnumberveryfast(tac0, tac1, tac2, delta);
				if(tjomn != 0) {
					func(tau0, tau1, tau2, delta, veetau.value * nr, tjomn);
				}
			});
		});
	}
}

struct PerThreadTotals {
	uint64_t systemCount;
	uint64_t counting;
	u192 totalResult;
};

// expects a function of the form void(const MBF& tau0, const MBF& tau1, const MBF& tau2, const MBF& delta, uint64_t eqClassSize, uint64_t solutionCount)
template<unsigned int Variables>
PerThreadTotals tjomnCountInParallel() {
	using AC = AntiChain<Variables>;
	using MBF = Monotonic<Variables>;
	using INT = Interval<Variables>;

	BufferedMap<MBF, int> alltaus = generateNonEquivalentMonotonics<Variables>();
	
	IntervalSizeCache<Variables> intervalSizes = IntervalSizeCache<Variables>::generate();
	return iterCollectionPartitionedWithSeparateTotalsWithBuffers(alltaus, PerThreadTotals{0, 0, 0}, [&](const KeyValue<MBF, int>& veetau, PerThreadTotals& localTotal, BufferedMap<AntiChain<Variables>, int>& bufSet) {
		std::cout << '.' << std::flush;
		
		generateTausWithBuffers(veetau.key, bufSet, [&](const MBF& tau0, const MBF& tau1, const MBF& tau2, const MBF& minDelta, const MBF& maxDelta, unsigned int nr) {
			uint64_t eqClassSize = veetau.value * nr;

			Interval<Variables> i(minDelta, maxDelta);
			AC tac0 = tau0.asAntiChain();
			AC tac1 = tau1.asAntiChain();
			AC tac2 = tau2.asAntiChain();
			i.forEach([&](const MBF& delta) {
				uint64_t tjomn = threejoinmeetnumberveryfast(tac0, tac1, tac2, delta);
				if(tjomn != 0) {
					localTotal.systemCount++;
					u128 term = 0;
					INT(tau0 | tau1 | tau2, MBF::getTop()).forEach([&](const MBF& alpha) {
						localTotal.counting++;
						uint64_t is0 = intervalSizes.getIntervalSize(tau0, alpha);
						uint64_t is1 = intervalSizes.getIntervalSize(tau1, alpha);
						uint64_t is2 = intervalSizes.getIntervalSize(tau2, alpha);
						term += umul128(is0 * is1, is2);
					});

					uint64_t dSize = intervalSizes.getIntervalSizeFromBot(delta);

					term *= tjomn; // quite a few leftover bits in term, to make up for 31 max bits of tjomn

					localTotal.totalResult += umul192(term, dSize * eqClassSize);
				}
			});
		});
	}, [](PerThreadTotals& total, const PerThreadTotals& localTotal) {
		total.totalResult += localTotal.totalResult;
		total.counting += localTotal.counting;
		total.systemCount += localTotal.systemCount;
	}, []() {
		return BufferedMap<AntiChain<Variables>, int>(dedekindNumbers[Variables]);
	});
}

template<unsigned int Variables>
struct TSize {
	Monotonic<Variables> t;
	Monotonic<Variables> alpha;
};

template<unsigned int Variables>
bool operator==(const TSize<Variables>& a, const TSize<Variables>& b) {
	return a.t == b.t && a.alpha == b.alpha;
}
template<unsigned int Variables>
bool operator!=(const TSize<Variables>& a, const TSize<Variables>& b) {
	return !(a == b);
}
template<unsigned int Variables>
bool operator<(const TSize<Variables>& a, const TSize<Variables>& b) {
	if(a.t.bf.bitset != b.t.bf.bitset) {
		return a.t.bf.bitset < b.t.bf.bitset;
	}
	return a.alpha.bf.bitset < b.alpha.bf.bitset;
}
template<unsigned int Variables>
bool operator>(const TSize<Variables>& a, const TSize<Variables>& b) {
	return b < a;
}
template<unsigned int Variables>
bool operator>=(const TSize<Variables>& a, const TSize<Variables>& b) {
	return !(a < b);
}
template<unsigned int Variables>
bool operator<=(const TSize<Variables>& a, const TSize<Variables>& b) {
	return !(b < a);
}

template<unsigned int Variables>
struct DSize {
	Monotonic<Variables> d;
};

template<unsigned int Variables>
bool operator==(const DSize<Variables>& a, const DSize<Variables>& b) {
	return a.d == b.d;
}
template<unsigned int Variables>
bool operator!=(const DSize<Variables>& a, const DSize<Variables>& b) {
	return !(a == b);
}
template<unsigned int Variables>
bool operator<(const DSize<Variables>& a, const DSize<Variables>& b) {
	return a.d.bf.bitset < b.d.bf.bitset;
}
template<unsigned int Variables>
bool operator>(const DSize<Variables>& a, const DSize<Variables>& b) {
	return b < a;
}
template<unsigned int Variables>
bool operator>=(const DSize<Variables>& a, const DSize<Variables>& b) {
	return !(a < b);
}
template<unsigned int Variables>
bool operator<=(const DSize<Variables>& a, const DSize<Variables>& b) {
	return !(b < a);
}

template<unsigned int Variables>
uint256_t revolution() {
	using AC = AntiChain<Variables>;
	using MBF = Monotonic<Variables>;
	using INT = Interval<Variables>;

	std::map<TJOMN<Variables>, TJOMNInfo> systems = threeJoinOneMeetDecompositionsFast<Variables>();

	std::cout << "systems : " << systems.size() << "\n";

	MBF e = MBF::getBot();
	MBF a = MBF::getTop();
	uint256_t result = 0;
	std::map<TSize<Variables>, uint64_t> tisizes;
	INT(e, a).forEach([&](const MBF& alpha) {
		INT(e, alpha).forEach([&](const MBF& t) {
			uint64_t intervalSize = intervalSizeFast(t, alpha);
			tisizes.insert(std::make_pair(TSize<Variables>{t, alpha}, intervalSize));
		});
	});
	std::map<DSize<Variables>, uint64_t> disizes;
	INT(e, a).forEach([&](const MBF& d) {
		uint64_t intervalSize = intervalSizeFast(e, d);
		disizes.insert(std::make_pair(DSize<Variables>{d}, intervalSize));
	});

	uint64_t counting = 0;
	for(const std::pair<TJOMN<Variables>, TJOMNInfo>& item : systems) {
		MBF t0 = item.first.taus[0];
		MBF t1 = item.first.taus[1];
		MBF t2 = item.first.taus[2];
		MBF d = item.first.delta;

		uint256_t term = 0;
		INT(t0 | t1 | t2, a).forEach([&](const MBF& alpha) {
			counting++;
			term +=
				uint256_t(tisizes[TSize<Variables>{t0, alpha}]) *
				uint256_t(tisizes[TSize<Variables>{t1, alpha}]) *
				uint256_t(tisizes[TSize<Variables>{t2, alpha}]);
		});

		result +=
			term *
			disizes[DSize<Variables>{d}] *
			item.second.eqClassSize * 
			item.second.solutionCount;
	}

	std::cout << "terms: " << counting << "\n";
	std::cout << "D(" << (Variables + 3) << ") = " << result << "\n";

	return result;
}

template<unsigned int Variables>
u192 revolutionMemoized() {
	using AC = AntiChain<Variables>;
	using MBF = Monotonic<Variables>;
	using INT = Interval<Variables>;

	MBF e = MBF::getBot();
	MBF a = MBF::getTop();

	IntervalSizeCache<Variables> intervalSizes = IntervalSizeCache<Variables>::generate();

	u192 result = 0;
	uint64_t counting = 0;
	uint64_t systemCount = 0;
	forEachTJOMNFast<Variables>([&](const MBF& t0, const MBF& t1, const MBF& t2, const MBF& d, uint64_t eqClassSize, uint64_t solutionCount) {
		systemCount++;
		u128 term = 0;
		INT(t0 | t1 | t2, a).forEach([&](const MBF& alpha) {
			counting++;
			uint64_t is0 = intervalSizes.getIntervalSize(t0, alpha);
			uint64_t is1 = intervalSizes.getIntervalSize(t1, alpha);
			uint64_t is2 = intervalSizes.getIntervalSize(t2, alpha);
			term += umul128(is0 * is1, is2);
		});

		uint64_t dSize = intervalSizes.getIntervalSizeFromBot(d);
		
		term *= solutionCount; // quite a few leftover bits in term, to make up for 31 max bits of solutionCount

		result += umul192(term, dSize * eqClassSize);
	});

	std::cout << "systems : " << systemCount << "\n";
	std::cout << "terms: " << counting << "\n";
	std::cout << "D(" << (Variables + 3) << ") = " << result << "\n";

	return result;
}

template<unsigned int Variables>
u192 revolutionParallel() {
	PerThreadTotals result = tjomnCountInParallel<Variables>();

	std::cout << "systems : " << result.systemCount << "\n";
	std::cout << "terms: " << result.counting << "\n";
	std::cout << "D(" << (Variables + 3) << ") = " << result.totalResult << "\n";

	return result.totalResult;
}
