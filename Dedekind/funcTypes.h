#pragma once

#include "functionInputBitSet.h"
#include <initializer_list>
#include <ostream>

template<unsigned int Variables>
struct Monotonic;

template<unsigned int Variables>
struct AntiChain {
	FunctionInputBitSet<Variables> fibs;

	AntiChain() = default;
	explicit AntiChain(const BitSet<(1 << Variables)>& bitset) : fibs(bitset) {
		assert(fibs.isAntiChain());
	}
	explicit AntiChain(const FunctionInputBitSet<Variables>& fibs) : fibs(fibs) {
		assert(fibs.isAntiChain());
	}
	explicit AntiChain(std::initializer_list<size_t> from) : fibs(FunctionInputBitSet<Variables>::empty()) {
		for(unsigned int item : from) {
			fibs.add(item);
		}

		assert(fibs.isAntiChain());
	}

	Monotonic<Variables> asMonotonic() const {
		return Monotonic<Variables>(fibs.monotonizeDown());
	}

	AntiChain intersection(const AntiChain<Variables>& other) const {
		return AntiChain(this->fibs.bitset & other.fibs.bitset);
	}

	// expects a function of the form void(const AntiChain<Variables>&)
	template<typename Func>
	void forEachSubSet(const Func& func) const {
		this->fibs.forEachSubSet([&func](const FunctionInputBitSet<Variables>& f) {
			func(AntiChain(f));
		});
	}

	// expects a function of the form void(size_t index)
	template<typename Func>
	void forEachOne(const Func& func) const {
		this->fibs.forEachOne(func);
	}

	size_t getFirst() const {
		return fibs.bitset.getFirstOnBit();
	}

	bool isEmpty() const {
		return this->fibs.isEmpty();
	}

	unsigned int getUniverse() const {
		return fibs.getUniverse();
	}

	template<unsigned int Variables>
	AntiChain<Variables>& operator-=(const AntiChain<Variables>& other) {
		this->fibs = andnot(this->fibs, other.fibs);
		return *this;
	}
};
template<unsigned int Variables>
bool operator==(const AntiChain<Variables>& a, const AntiChain<Variables>& b) {
	return a.fibs == b.fibs;
}
template<unsigned int Variables>
bool operator!=(const AntiChain<Variables>& a, const AntiChain<Variables>& b) {
	return a.fibs != b.fibs;
}
template<unsigned int Variables>
AntiChain<Variables> operator-(const AntiChain<Variables>& a, const AntiChain<Variables>& b) {
	return AntiChain<Variables>(andnot(a.fibs, b.fibs));
}
template<unsigned int Variables>
AntiChain<Variables> operator*(const AntiChain<Variables>& a, const AntiChain<Variables>& b) {
	FunctionInputBitSet<Variables> result = FunctionInputBitSet<Variables>::empty();

	a.forEachOne([&](size_t aIndex) {
		b.forEachOne([&](size_t bIndex) {
			result.bitset.set(aIndex | bIndex);
		});
	});

	return AntiChain<Variables>(result.asAntiChain());
}

template<unsigned int Variables>
struct Monotonic {
	FunctionInputBitSet<Variables> fibs;

	Monotonic() = default;
	explicit Monotonic(const BitSet<(1 << Variables)>& bitset) : fibs(bitset) {
		assert(fibs.isMonotonic());
	}
	explicit Monotonic(const FunctionInputBitSet<Variables>& fibs) : fibs(fibs) {
		assert(fibs.isMonotonic());
	}

	AntiChain<Variables> asAntiChain() const {
		return AntiChain<Variables>(andnot(fibs, fibs.prev()));
	}

	Monotonic prev() const {
		return Monotonic(fibs.prev());
	}
	Monotonic next() const {
		return Monotonic(fibs.next());
	}

	void add(size_t newBit) {
		this->fibs.add(newBit);
		assert(fibs.isMonotonic());
	}

	// expects a function of the form void(size_t index)
	template<typename Func>
	void forEachOne(const Func& func) const {
		this->fibs.forEachOne(func);
	}

	bool isEmpty() const {
		return this->fibs.isEmpty();
	}

	unsigned int getUniverse() const {
		return fibs.getUniverse();
	}
};

template<unsigned int Variables>
bool operator==(const Monotonic<Variables>& a, const Monotonic<Variables>& b) {
	return a.fibs == b.fibs;
}
template<unsigned int Variables>
bool operator!=(const Monotonic<Variables>& a, const Monotonic<Variables>& b) {
	return a.fibs != b.fibs;
}

template<unsigned int Variables>
Monotonic<Variables> operator&(const Monotonic<Variables>& a, const Monotonic<Variables>& b) {
	return Monotonic<Variables>(a.fibs & b.fibs);
}
template<unsigned int Variables>
Monotonic<Variables> operator|(const Monotonic<Variables>& a, const Monotonic<Variables>& b) {
	return Monotonic<Variables>(a.fibs | b.fibs);
}

template<unsigned int Variables>
Monotonic<Variables>& operator&=(Monotonic<Variables>& a, const Monotonic<Variables>& b) {
	a.fibs &= b.fibs;
	return a;
}
template<unsigned int Variables>
Monotonic<Variables>& operator|=(Monotonic<Variables>& a, const Monotonic<Variables>& b) {
	a.fibs |= b.fibs;
	return a;
}

template<unsigned int Variables>
bool operator<=(const Monotonic<Variables>& a, const Monotonic<Variables>& b) {
	return a.fibs.isSubSetOf(b.fibs);
}


template<unsigned int Variables>
bool isUniqueExtention(const Monotonic<Variables>& bs, size_t bit) {
	AntiChain<Variables> possibleBits = bs.asAntiChain();

	return possibleBits.getFirst() == bit;
}

template<unsigned int Variables, typename Func>
void forEachMonotonicFunctionRecursive(const Monotonic<Variables>& cur, const Func& func) {
	func(cur);

	AntiChain<Variables> newBits(andnot(cur.fibs.next(), cur.fibs));

	newBits.forEachOne([&](size_t bit) {
		Monotonic<Variables> newMBF = cur;
		newMBF.add(FunctionInput::underlyingType(bit));

		if(isUniqueExtention(newMBF, bit)) {
			forEachMonotonicFunctionRecursive(newMBF, func);
		}
	});
}

template<unsigned int Variables, typename Func>
void forEachMonotonicFunction(const Func& func) {
	forEachMonotonicFunctionRecursive(Monotonic<Variables>(FunctionInputBitSet<Variables>::empty()), func);
}

template<unsigned int Variables>
Monotonic<Variables> acProd(const Monotonic<Variables>& a, const AntiChain<Variables>& b) {
	assert((a.getUniverse() & b.getUniverse()) == 0); // direct product is optimized if no overlap of universes
	
	BitSet<(1 << Variables)> result = BitSet<(1 << Variables)>::empty();

	b.forEachOne([&](size_t index) {
		result |= a.fibs.bitset << index;
	});

	return Monotonic<Variables>(FunctionInputBitSet<Variables>(result).monotonizeDown());
}

template<unsigned int Variables>
Monotonic<Variables> acProd(const Monotonic<Variables>& a, const Monotonic<Variables>& b) {
	return acProd(a, b.asAntiChain());
}

template<unsigned int Variables>
std::ostream& operator<<(std::ostream& os, const AntiChain<Variables>& ac) {
	bool isFirst = true;
	ac.forEachOne([&](size_t index) {
		os << (isFirst ? "{" : ", ");
		isFirst = false;
		if(index == 0) {
			os << '/';
		} else {
			for(int bit = 0; bit < Variables; bit++) {
				if(index & (1 << bit)) {
					os << char('a' + bit);
				}
			}
		}
	});
	if(isFirst) os << '{';
	os << '}';
	return os;
}

template<unsigned int Variables>
std::ostream& operator<<(std::ostream& os, const Monotonic<Variables>& ac) {
	os << ac.asAntiChain();
	return os;
}
