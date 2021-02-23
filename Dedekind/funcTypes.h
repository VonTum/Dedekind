#pragma once

#include "booleanFunction.h"
#include <initializer_list>
#include <ostream>

template<unsigned int Variables>
struct Monotonic;

template<unsigned int Variables>
struct AntiChain {
	BooleanFunction<Variables> func;

	AntiChain() = default;
	explicit AntiChain(const BitSet<(1 << Variables)>& bitset) : func(bitset) {
		assert(func.isAntiChain());
	}
	explicit AntiChain(const BooleanFunction<Variables>& func) : func(func) {
		assert(func.isAntiChain());
	}
	explicit AntiChain(std::initializer_list<size_t> from) : func(BooleanFunction<Variables>::empty()) {
		for(unsigned int item : from) {
			func.add(item);
		}

		assert(func.isAntiChain());
	}

	Monotonic<Variables> asMonotonic() const {
		return Monotonic<Variables>(func.monotonizeDown());
	}

	void add(size_t index) {
		this->func.add(index);
		assert(this->func.isAntiChain());
	}

	void remove(size_t index) {
		this->func.remove(index);
		assert(this->func.isAntiChain());
	}

	AntiChain intersection(const AntiChain<Variables>& other) const {
		return AntiChain(this->func.bitset & other.func.bitset);
	}

	// expects a function of the form void(const AntiChain<Variables>&)
	template<typename Func>
	void forEachSubSet(const Func& func) const {
		this->func.forEachSubSet([&func](const BooleanFunction<Variables>& f) {
			func(AntiChain(f));
		});
	}

	// expects a function of the form void(size_t index)
	template<typename Func>
	void forEachOne(const Func& func) const {
		this->func.forEachOne(func);
	}

	size_t getFirst() const {
		return func.bitset.getFirstOnBit();
	}

	bool isEmpty() const {
		return this->func.isEmpty();
	}

	unsigned int getUniverse() const {
		return func.getUniverse();
	}

	AntiChain& operator-=(const AntiChain& other) {
		this->func = andnot(this->func, other.func);
		return *this;
	}

	AntiChain canonize() const {
		return AntiChain(this->func.canonize());
	}

	size_t size() const {
		return this->func.size();
	}
	
	uint64_t hash() const {
		return this->func.hash();
	}
};
template<unsigned int Variables>
bool operator==(const AntiChain<Variables>& a, const AntiChain<Variables>& b) {
	return a.func == b.func;
}
template<unsigned int Variables>
bool operator!=(const AntiChain<Variables>& a, const AntiChain<Variables>& b) {
	return a.func != b.func;
}
template<unsigned int Variables>
AntiChain<Variables> operator-(const AntiChain<Variables>& a, const AntiChain<Variables>& b) {
	return AntiChain<Variables>(andnot(a.func, b.func));
}
template<unsigned int Variables>
AntiChain<Variables> operator*(const AntiChain<Variables>& a, const AntiChain<Variables>& b) {
	BooleanFunction<Variables> result = BooleanFunction<Variables>::empty();

	a.forEachOne([&](size_t aIndex) {
		b.forEachOne([&](size_t bIndex) {
			result.bitset.set(aIndex | bIndex);
		});
	});

	return AntiChain<Variables>(result.asAntiChain());
}

template<unsigned int Variables>
struct Monotonic {
	BooleanFunction<Variables> func;

	Monotonic() = default;
	explicit Monotonic(const BitSet<(1 << Variables)>& bitset) : func(bitset) {
		assert(func.isMonotonic());
	}
	explicit Monotonic(const BooleanFunction<Variables>& func) : func(func) {
		assert(func.isMonotonic());
	}

	AntiChain<Variables> asAntiChain() const {
		return AntiChain<Variables>(andnot(func, func.pred()));
	}

	Monotonic pred() const {
		return Monotonic(func.pred());
	}
	Monotonic succ() const {
		return Monotonic(func.succ());
	}

	void add(size_t index) {
		this->func.add(index);
		assert(this->func.isMonotonic());
	}

	void remove(size_t index) {
		this->func.remove(index);
		assert(this->func.isMonotonic());
	}

	// expects a function of the form void(size_t index)
	template<typename Func>
	void forEachOne(const Func& func) const {
		this->func.forEachOne(func);
	}

	bool isEmpty() const {
		return this->func.isEmpty();
	}

	bool isFull() const {
		return this->func.isFull();
	}

	unsigned int getUniverse() const {
		return func.getUniverse();
	}

	Monotonic canonize() const {
		return Monotonic(this->func.canonize());
	}

	size_t size() const {
		return this->func.size();
	}

	uint64_t hash() const {
		return this->func.hash();
	}

	static Monotonic getTop() {
		return Monotonic(BooleanFunction<Variables>::full());
	}

	static Monotonic getBot() {
		return Monotonic(BooleanFunction<Variables>::empty());
	}
};

template<unsigned int Variables>
bool operator==(const Monotonic<Variables>& a, const Monotonic<Variables>& b) {
	return a.func == b.func;
}
template<unsigned int Variables>
bool operator!=(const Monotonic<Variables>& a, const Monotonic<Variables>& b) {
	return a.func != b.func;
}

template<unsigned int Variables>
Monotonic<Variables> operator&(const Monotonic<Variables>& a, const Monotonic<Variables>& b) {
	return Monotonic<Variables>(a.func & b.func);
}
template<unsigned int Variables>
Monotonic<Variables> operator|(const Monotonic<Variables>& a, const Monotonic<Variables>& b) {
	return Monotonic<Variables>(a.func | b.func);
}

template<unsigned int Variables>
Monotonic<Variables>& operator&=(Monotonic<Variables>& a, const Monotonic<Variables>& b) {
	a.func &= b.func;
	return a;
}
template<unsigned int Variables>
Monotonic<Variables>& operator|=(Monotonic<Variables>& a, const Monotonic<Variables>& b) {
	a.func |= b.func;
	return a;
}

template<unsigned int Variables>
bool operator<=(const Monotonic<Variables>& a, const Monotonic<Variables>& b) {
	return a.func.isSubSetOf(b.func);
}


template<unsigned int Variables>
bool isUniqueExtention(const Monotonic<Variables>& bs, size_t bit) {
	AntiChain<Variables> possibleBits = bs.asAntiChain();

	return possibleBits.getFirst() == bit;
}

template<unsigned int Variables, typename Func>
void forEachMonotonicFunctionRecursive(const Monotonic<Variables>& cur, const Func& func) {
	func(cur);

	AntiChain<Variables> newBits(andnot(cur.func.succ(), cur.func));

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
	forEachMonotonicFunctionRecursive(Monotonic<Variables>::getBot(), func);
}

template<unsigned int Variables, typename Func>
void forEachMonotonicFunctionBetweenRecursive(const Monotonic<Variables>& bot, const Monotonic<Variables>& top, const Monotonic<Variables>& cur, const Func& func) {
	if(bot <= cur) {
		func(cur);
	}

	AntiChain<Variables> newBits(andnot(cur.func.succ(), cur.func) & top.func);

	newBits.forEachOne([&](size_t bit) {
		Monotonic<Variables> newMBF = cur;
		newMBF.add(FunctionInput::underlyingType(bit));

		if(isUniqueExtention(newMBF, bit)) {
			forEachMonotonicFunctionBetweenRecursive(bot, top, newMBF, func);
		}
	});
}

template<unsigned int Variables, typename Func>
void forEachMonotonicFunctionBetween(const Monotonic<Variables>& bot, const Monotonic<Variables>& top, const Func& func) {
	//BooleanFunction<Variables> reachableFromBot = ~((~bot.asAntiChain().func).monotonizeUp());
	//BooleanFunction<Variables> availableBits = reachableFromBot & top.func;
	if(bot <= top) {
		forEachMonotonicFunctionBetweenRecursive(bot, top, Monotonic<Variables>::getBot(), func);
	}
}

template<unsigned int Variables>
Monotonic<Variables> acProd(const Monotonic<Variables>& a, const AntiChain<Variables>& b) {
	assert((a.getUniverse() & b.getUniverse()) == 0); // direct product is optimized if no overlap of universes
	
	BitSet<(1 << Variables)> result = BitSet<(1 << Variables)>::empty();

	b.forEachOne([&](size_t index) {
		result |= a.func.bitset << index;
	});

	return Monotonic<Variables>(BooleanFunction<Variables>(result).monotonizeDown());
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
