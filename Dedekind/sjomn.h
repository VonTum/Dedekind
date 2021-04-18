#pragma once

#include "u192.h"
#include "funcTypes.h"
#include "smallVector.h"

// iterates all antichains that can be constructed from the given list of tops
// expects void(const Monotonic<Variables>&)
template<unsigned int Variables, typename Func>
void achainOverAcc(const SingletonAntiChain<Variables>* sosms, const SingletonAntiChain<Variables>* sosmsEnd, const Monotonic<Variables>& cur, const Func& func) {
	func(cur);
	for(const SingletonAntiChain<Variables>* curExtention = sosms; curExtention != sosmsEnd; curExtention++) {
		if(!(*curExtention <= cur)) {
			Monotonic<Variables> extended = cur | curExtention->asMonotonic();
			achainOverAcc(curExtention + 1, sosmsEnd, extended, func);
		}
	}
}

// iterates all antichains that can be constructed from the given list of tops
// expects void(const Monotonic<Variables>&)
template<unsigned int Variables, typename Func>
void achainOver(const SingletonAntiChain<Variables>* sosms, const SingletonAntiChain<Variables>* sosmsEnd, const Func& func) {
	achainOverAcc(sosms, sosmsEnd, Monotonic<Variables>::getBot(), func);
}

template<unsigned int Variables, size_t SoacSize>
SmallVector<SingletonAntiChain<Variables>, (1 << Variables)> remaining(const SingletonAntiChain<Variables>* sosms, const SingletonAntiChain<Variables>* sosmsEnd, const std::array<Monotonic<Variables>, SoacSize>& soac) {
	SmallVector<SingletonAntiChain<Variables>, (1 << Variables)> pres;
	SmallVector<SingletonAntiChain<Variables>, (1 << Variables)> edgeElements;

	for(const SingletonAntiChain<Variables>* ass = sosms; ass != sosmsEnd; ass++) {
		size_t occurences = 0;
		bool isAnEdge = false;
		for(const Monotonic<Variables>& k : soac) {
			if(*ass <= k) {
				if(k.asAntiChain().contains(*ass)) {
					isAnEdge = true;
				}
				occurences++;
				if(occurences == 2) {
					break;
				}
			}
		}
		if(occurences < 2) {
			pres.push_back(*ass);
		} else {
			if(isAnEdge) {
				edgeElements.push_back(*ass);
			}
		}
	}

	SmallVector<SingletonAntiChain<Variables>, (1 << Variables)> res;

	for(const SingletonAntiChain<Variables>& ks : pres) {
		if(!edgeElements.hasMatching([&ks](const SingletonAntiChain<Variables>& ass) {return ass <= ks; })) {
			res.push_back(ks);
		}
	}

	return res;
}

template<unsigned int Variables>
int computePermutationsSorted(const std::array<Monotonic<Variables>, 4>& ks) {
	bool eq01 = ks[0] == ks[1];
	bool eq12 = ks[1] == ks[2];
	bool eq23 = ks[2] == ks[3];

	if(eq01 && eq12 && eq23) { // 0-0-0-0
		return 1;
	} else if(eq12 && (eq01 || eq23)) { // 0-0-0-1
		return 4;
	} else if(eq01 && eq23) { // 0-0-1-1
		return 6;
	} else if(eq01 || eq12 || eq23) { // 0-0-1-2
		return 12;
	} else { // 0-1-2-3
		return 24;
	}
}

// expects a function of the form void(std::array<Monotonic<Variables>, 4> ks, int equivalents)
template<unsigned int Variables, typename Func>
void fourkappassymmetric(const Func& func) {
	SingletonAntiChain<Variables> available1[1 << Variables]; for(size_t i = 0; i < (1 << Variables); i++) available1[i] = SingletonAntiChain<Variables>(i);

	forEachMonotonicFunction<Variables>([&](const Monotonic<Variables>& k1) {
		forEachMonotonicFunction<Variables>([&](const Monotonic<Variables>& k2) {
			if(k2.bf.bitset > k1.bf.bitset) return; // continue - deduplicate: not MBF Domination, but just an arbitrary total ordering
			
			SmallVector<SingletonAntiChain<Variables>, 1 << Variables> available2 = remaining(available1, available1 + (1 << Variables), std::array<Monotonic<Variables>, 2>{k1, k2});
			achainOver(available2.begin(), available2.end(), [&](const Monotonic<Variables>& k3) {
				if(k3.bf.bitset > k2.bf.bitset) return; // continue - deduplicate: not MBF Domination, but just an arbitrary total ordering
				SmallVector<SingletonAntiChain<Variables>, 1 << Variables> available3 = remaining(available2.begin(), available2.end(), std::array<Monotonic<Variables>, 3>{k1, k2, k3});

				achainOver(available3.begin(), available3.end(), [&](const Monotonic<Variables>& k4) {
					if(k4.bf.bitset > k3.bf.bitset) return; // continue - deduplicate: not MBF Domination, but just an arbitrary total ordering
					
					std::array<Monotonic<Variables>, 4> ks{k1, k2, k3, k4};
					
					func(ks, computePermutationsSorted(ks));
				});
			});
		});
	});
}

// expects a function of the form void(std::array<Monotonic<Variables>, 4> ks, int equivalents)
template<unsigned int Variables, typename Func>
void fourkappassymmetricminimal(const Func& func) {
	// TODO, atm just do the same as fourkappassymmetric
	fourkappassymmetric<Variables, Func>(func);
}

// expects a function of the form void(Monotonic<Variables> mbf)
template<unsigned int Variables, typename Func>
void kappasigmas(const std::array<Monotonic<Variables>, 4>& kappas, const Func& func) {
	BooleanFunction<Variables> allsetsk = BooleanFunction<Variables>::empty();
	for(const Monotonic<Variables>& m : kappas) {
		allsetsk |= m.asAntiChain().bf; // OR
	}
	std::cout << "allsetsk " << allsetsk << "\n";
	SmallVector<SingletonAntiChain<Variables>, 1 << Variables> ok;
	for(int assIdx = (1 << Variables) - 1; assIdx >= 0; assIdx--) {
		SingletonAntiChain<Variables> ass(assIdx);
		size_t count = 0;
		for(const Monotonic<Variables>& k : kappas) {
			if(ass <= k) {
				count++;
			}
		}

		if(count > 2) continue;

		if((allsetsk & ass.asMonotonic().bf).isEmpty()) {
			ok.push_back(ass);
		}
	}

	std::cout << "ok " << ok << "\n";
	achainOver(ok.begin(), ok.end(), func);
}

template<unsigned int Variables>
std::array<Monotonic<Variables>, 6> kappasigmaetas(const std::array<Monotonic<Variables>, 4>& kappas, const Monotonic<Variables>& sigma) {
	return std::array<Monotonic<Variables>, 6>{
		kappas[0] | kappas[1] | sigma,
		kappas[0] | kappas[2] | sigma,
		kappas[0] | kappas[3] | sigma,
		kappas[1] | kappas[2] | sigma,
		kappas[1] | kappas[3] | sigma,
		kappas[2] | kappas[3] | sigma,
	};
}

// expects a function of the form void(Monotonic<Variables> delta)
template<unsigned int Variables, typename Func>
void kappasetasdelta(const std::array<Monotonic<Variables>, 4>& kappas, const std::array<Monotonic<Variables>, 6>& etas, const Func& func) {
	Monotonic<Variables> bot = kappas[0] & kappas[1] & kappas[2] & kappas[3];
	Monotonic<Variables> top = etas[0] & etas[1] & etas[2] & etas[3] & etas[4] & etas[5];

	forEachMonotonicFunctionBetween(bot, top, func);
}

template<unsigned int Variables, typename Func>
void sjomnumbertablesymmetric(const Func& func) {
	fourkappassymmetricminimal<Variables>([&](const std::array<Monotonic<Variables>, 4>& kappas, int equivalents) {
		std::cout << kappas << "\n";
		kappasigmas<Variables>(kappas, [&](const Monotonic<Variables>& sigma) {
			std::cout << "    " << sigma << "\n";
			std::array<Monotonic<Variables>, 6> etas = kappasigmaetas<Variables>(kappas, sigma);
			kappasetasdelta<Variables>(kappas, etas, [&](const Monotonic<Variables>& delta) {
				//TODO//AntiChain<Variables> diff = sigma.asAntiChain() - delta;
			});
		});
	});
}


/*template<unsigned int Variables>
u192 sizeofspace() {
	std::count << "Computing D(" << (Variables + 4) << "using a memory - mapped sjom algorithm";


}
*/