#pragma once

#include "booleanFunction.h"
#include <initializer_list>
#include <ostream>

template<unsigned int Variables>
struct Monotonic;

template<unsigned int Variables>
struct Layer;

template<unsigned int Variables>
struct AntiChain {
	BooleanFunction<Variables> bf;

	AntiChain() = default;
	explicit AntiChain(const BitSet<(1 << Variables)>& bitset) : bf(bitset) {
		assert(bf.isAntiChain());
	}
	explicit AntiChain(const BooleanFunction<Variables>& bf) : bf(bf) {
		assert(bf.isAntiChain());
	}
	explicit AntiChain(std::initializer_list<size_t> from) : bf(BooleanFunction<Variables>::empty()) {
		for(unsigned int item : from) {
			bf.add(item);
		}

		assert(bf.isAntiChain());
	}

	Monotonic<Variables> asMonotonic() const {
		return Monotonic<Variables>(bf.monotonizeDown());
	}

	void add(size_t index) {
		this->bf.add(index);
		assert(this->bf.isAntiChain());
	}

	void remove(size_t index) {
		this->bf.remove(index);
		assert(this->bf.isAntiChain());
	}

	AntiChain intersection(const AntiChain<Variables>& other) const {
		return AntiChain(this->bf.bitset & other.bf.bitset);
	}

	// expects a function of the form void(const AntiChain<Variables>&)
	template<typename Func>
	void forEachSubSet(const Func& bf) const {
		this->bf.forEachSubSet([&bf](const BooleanFunction<Variables>& f) {
			bf(AntiChain(f));
		});
	}

	// expects a function of the form void(size_t index)
	template<typename Func>
	void forEachOne(const Func& bf) const {
		this->bf.forEachOne(bf);
	}

	template<typename Func>
	void forEachPermutation(const Func& func) const {
		this->bf.forEachPermutation([&func](const BooleanFunction<Variables>& bf) {func(AntiChain(bf)); });
	}

	Layer<Variables> getTopLayer() const {
		return Layer<Variables>(this->bf.getTopLayer());
	}

	size_t getFirst() const {
		return bf.bitset.getFirstOnBit();
	}

	bool isEmpty() const {
		return this->bf.isEmpty();
	}

	unsigned int getUniverse() const {
		return bf.getUniverse();
	}

	AntiChain& operator-=(const AntiChain& other) {
		this->bf = andnot(this->bf, other.bf);
		return *this;
	}

	// only equals *this -= other.asAntiChain() if other <= *this
	AntiChain& operator-=(const Monotonic<Variables>& other) {
		this->bf = andnot(this->bf, other.bf);
		return *this;
	}

	AntiChain canonize() const {
		return AntiChain(this->bf.canonize());
	}

	size_t size() const {
		return this->bf.size();
	}
	
	uint64_t hash() const {
		return this->bf.hash();
	}
};
template<unsigned int Variables>
bool operator==(const AntiChain<Variables>& a, const AntiChain<Variables>& b) {
	return a.bf == b.bf;
}
template<unsigned int Variables>
bool operator!=(const AntiChain<Variables>& a, const AntiChain<Variables>& b) {
	return a.bf != b.bf;
}
template<unsigned int Variables>
AntiChain<Variables> operator-(const AntiChain<Variables>& a, const AntiChain<Variables>& b) {
	return AntiChain<Variables>(andnot(a.bf, b.bf));
}
// only equals a - b.asAntiChain() if b <= a
template<unsigned int Variables>
AntiChain<Variables> operator-(const AntiChain<Variables>& a, const Monotonic<Variables>& b) {
	return AntiChain<Variables>(andnot(a.bf, b.bf));
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
	BooleanFunction<Variables> bf;

	Monotonic() = default;
	explicit Monotonic(const BitSet<(1 << Variables)>& bitset) : bf(bitset) {
		assert(bf.isMonotonic());
	}
	explicit Monotonic(const BooleanFunction<Variables>& bf) : bf(bf) {
		assert(bf.isMonotonic());
	}

	AntiChain<Variables> asAntiChain() const {
		return AntiChain<Variables>(andnot(bf, bf.pred()));
	}

	Monotonic pred() const {
		return Monotonic(bf.pred());
	}
	Monotonic succ() const {
		return Monotonic(bf.succ());
	}
	Monotonic dual() const {
		return Monotonic(bf.dual());
	}

	void add(size_t index) {
		this->bf.add(index);
		assert(this->bf.isMonotonic());
	}

	void remove(size_t index) {
		this->bf.remove(index);
		assert(this->bf.isMonotonic());
	}

	Layer<Variables> getTopLayer() const {
		return Layer<Variables>(this->bf.getTopLayer());
	}

	// expects a function of the form void(size_t index)
	template<typename Func>
	void forEachOne(const Func& bf) const {
		this->bf.forEachOne(bf);
	}

	template<typename Func>
	void forEachPermutation(const Func& func) const {
		this->bf.forEachPermutation([&func](const BooleanFunction<Variables>& bf) {func(Monotonic(bf)); });
	}

	bool isEmpty() const {
		return this->bf.isEmpty();
	}

	bool isFull() const {
		return this->bf.isFull();
	}

	unsigned int getUniverse() const {
		return bf.getUniverse();
	}

	Monotonic canonize() const {
		return Monotonic(this->bf.canonize());
	}

	Layer<Variables> getLayer(unsigned int layer) const {
		return Layer<Variables>(this->bf.getLayer(layer));
	}

	size_t size() const {
		return this->bf.size();
	}

	uint64_t hash() const {
		return this->bf.hash();
	}

	static constexpr Monotonic getTop() {
		return Monotonic(BooleanFunction<Variables>::full());
	}

	static constexpr Monotonic getBot() {
		return Monotonic(BooleanFunction<Variables>::empty());
	}

	static constexpr Monotonic getBotSucc() {
		BooleanFunction<Variables> result = BooleanFunction<Variables>::empty();
		result.add(0);
		return Monotonic(result);
	}
};

template<unsigned int Variables>
bool operator==(const Monotonic<Variables>& a, const Monotonic<Variables>& b) {
	return a.bf == b.bf;
}
template<unsigned int Variables>
bool operator!=(const Monotonic<Variables>& a, const Monotonic<Variables>& b) {
	return a.bf != b.bf;
}

template<unsigned int Variables>
Monotonic<Variables> operator&(const Monotonic<Variables>& a, const Monotonic<Variables>& b) {
	return Monotonic<Variables>(a.bf & b.bf);
}
template<unsigned int Variables>
Monotonic<Variables> operator|(const Monotonic<Variables>& a, const Monotonic<Variables>& b) {
	return Monotonic<Variables>(a.bf | b.bf);
}

template<unsigned int Variables>
Monotonic<Variables>& operator&=(Monotonic<Variables>& a, const Monotonic<Variables>& b) {
	a.bf &= b.bf;
	return a;
}
template<unsigned int Variables>
Monotonic<Variables>& operator|=(Monotonic<Variables>& a, const Monotonic<Variables>& b) {
	a.bf |= b.bf;
	return a;
}

template<unsigned int Variables>
bool operator<=(const Monotonic<Variables>& a, const Monotonic<Variables>& b) {
	return a.bf.isSubSetOf(b.bf);
}

template<unsigned int Variables, typename Func>
void forEachMonotonicFunctionRecursive(const Monotonic<Variables>& cur, const Func& func) {
	func(cur);

	AntiChain<Variables> newBits(andnot(cur.bf.succ(), cur.bf));

	newBits.forEachOne([&](size_t bit) {
		Monotonic<Variables> newMBF = cur;
		newMBF.add(bit);

		if(newMBF.asAntiChain().getFirst() == bit) {
			forEachMonotonicFunctionRecursive(newMBF, func);
		}
	});
}

template<unsigned int Variables, typename Func>
void forEachMonotonicFunction(const Func& func) {
	forEachMonotonicFunctionRecursive(Monotonic<Variables>::getBot(), func);
}

template<unsigned int Variables, typename Func>
void forEachMonotonicFunctionUpToRecursive(const Monotonic<Variables>& top, const Monotonic<Variables>& cur, const Func& func) {
	func(cur);

	AntiChain<Variables> newBits(andnot(cur.bf.succ(), cur.bf) & top.bf);

	newBits.forEachOne([&](size_t bit) {
		Monotonic<Variables> newMBF = cur;
		newMBF.add(bit);

		if(newMBF.asAntiChain().getFirst() == bit) {
			forEachMonotonicFunctionUpToRecursive(top, newMBF, func);
		}
	});
}

template<unsigned int Variables, typename Func>
void forEachMonotonicFunctionUpTo(const Monotonic<Variables>& top, const Func& func) {
	forEachMonotonicFunctionUpToRecursive(top, Monotonic<Variables>::getBot(), func);
}

template<unsigned int Variables, typename Func>
void forEachMonotonicFunctionBetweenRecursive(const Monotonic<Variables>& bot, const Monotonic<Variables>& top, const Monotonic<Variables>& cur, const Func& func) {
	func(cur);

	// remove top and bot extentions which cannot be added
	AntiChain<Variables> newBits(andnot(cur.bf.succ(), cur.bf) & top.bf & ~bot.bf);

	newBits.forEachOne([&](size_t bit) {
		Monotonic<Variables> newMBF = cur;
		newMBF.add(bit);

		// &~bot is to remove bits that are not allowed to be added, as they should have been added from the beginning
		// the new bits that overlap with bot are bits that *must* have already been on, so cannot be added
		if((newMBF.asAntiChain().bf & ~bot.bf).getFirst() == bit) {
			forEachMonotonicFunctionBetweenRecursive(bot, top, newMBF, func);
		}
	});
}

template<unsigned int Variables, typename Func>
void forEachMonotonicFunctionBetween(const Monotonic<Variables>& bot, const Monotonic<Variables>& top, const Func& func) {
	if(bot <= top) {
		forEachMonotonicFunctionBetweenRecursive(bot, top, bot, func);
	}
}

template<unsigned int Variables>
Monotonic<Variables> acProd(const Monotonic<Variables>& a, const AntiChain<Variables>& b) {
	assert((a.getUniverse() & b.getUniverse()) == 0); // direct product is optimized if no overlap of universes
	
	BitSet<(1 << Variables)> result = BitSet<(1 << Variables)>::empty();

	b.forEachOne([&](size_t index) {
		result |= a.bf.bitset << index;
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

template<unsigned int Variables>
struct Layer {
	using BF = BooleanFunction<Variables>;
	using MBF = Monotonic<Variables>;
	using AC = AntiChain<Variables>;
	BF bf;

	Layer() : bf(BF::empty()) {}
	explicit Layer(const BF& bf) : bf(bf) {
		assert(bf.isLayer());
	}
	Layer(std::initializer_list<size_t> initList) : bf(BF::empty()) {
		for(const size_t& item : initList) {
			this->bf.add(item);
		}
		assert(bf.isLayer());
	}

	void add(size_t index) {
		this->bf.add(index);
		assert(this->bf.isMonotonic());
	}

	void remove(size_t index) {
		this->bf.remove(index);
		assert(this->bf.isMonotonic());
	}

	AC asAntiChain() const {
		return AC(this->bf);
	}
	MBF asMonotonic() const {
		return MBF(this->bf.monotonizeDown());
	}
	// expects a function of the form void(const Layer& layer)
	template<typename Func>
	void forEachSubSet(const Func& func) const {
		bf.forEachSubSet([&func](const BF& bf) {
			func(Layer(bf)); 
		});
	}

	template<typename Func>
	void forEachPermutation(const Func& func) const {
		this->bf.forEachPermutation([&func](const BooleanFunction<Variables>& bf) {func(Layer(bf)); });
	}

	// returns the minimally required supporting layer below this layer for it to be monotonic
	// returns the layer below this layer of all elements connected to '1' elements in this layer
	Layer pred() const {
		return Layer(bf.pred());
	}
	// returns the layer above this layer of all elements connected to '1' elements in this layer
	Layer succ() const {
		return Layer(bf.succ());
	}

	size_t getFirst() const {
		return this->bf.getFirst();
	}

	// expects a function of the form void(size_t index)
	template<typename Func>
	void forEachOne(const Func& bf) const {
		this->bf.forEachOne(bf);
	}

	bool isEmpty() const {
		return this->bf.isEmpty();
	}

	unsigned int getUniverse() const {
		return bf.getUniverse();
	}

	Layer canonize() const {
		return Layer(this->bf.canonize());
	}

	size_t size() const {
		return this->bf.size();
	}

	uint64_t hash() const {
		return this->bf.hash();
	}
};

template<unsigned int Variables>
bool operator==(const Layer<Variables>& a, const Layer<Variables>& b) {
	return a.bf == b.bf;
}
template<unsigned int Variables>
bool operator!=(const Layer<Variables>& a, const Layer<Variables>& b) {
	return a.bf != b.bf;
}
template<unsigned int Variables>
std::ostream& operator<<(std::ostream& os, const Layer<Variables>& l) {
	os << l.asAntiChain();
	return os;
}

template<unsigned int Variables>
Layer<Variables> operator&(const Layer<Variables>& a, const Layer<Variables>& b) {
	return Layer<Variables>(a.bf & b.bf);
}
template<unsigned int Variables>
Layer<Variables> operator&(const Layer<Variables>& a, const Monotonic<Variables>& b) {
	return Layer<Variables>(a.bf & b.bf);
}
template<unsigned int Variables>
Layer<Variables> operator&(const Monotonic<Variables>& a, const Layer<Variables>& b) {
	return Layer<Variables>(a.bf & b.bf);
}
template<unsigned int Variables>
Layer<Variables> operator&(const Layer<Variables>& a, const AntiChain<Variables>& b) {
	return Layer<Variables>(a.bf & b.bf);
}
template<unsigned int Variables>
Layer<Variables> operator&(const AntiChain<Variables>& a, const Layer<Variables>& b) {
	return Layer<Variables>(a.bf & b.bf);
}
template<unsigned int Variables>
Layer<Variables> operator&(const Layer<Variables>& a, const BooleanFunction<Variables>& b) {
	return Layer<Variables>(a.bf & b);
}
template<unsigned int Variables>
Layer<Variables> operator&(const BooleanFunction<Variables>& a, const Layer<Variables>& b) {
	return Layer<Variables>(a & b.bf);
}

template<unsigned int Variables>
Layer<Variables> andnot(const Layer<Variables>& a, const Layer<Variables>& b) {
	return Layer<Variables>(andnot(a.bf, b.bf));
}
template<unsigned int Variables>
Layer<Variables> andnot(const Layer<Variables>& a, const Monotonic<Variables>& b) {
	return Layer<Variables>(andnot(a.bf, b.bf));
}
template<unsigned int Variables>
Layer<Variables> andnot(const Layer<Variables>& a, const AntiChain<Variables>& b) {
	return Layer<Variables>(andnot(a.bf, b.bf));
}
template<unsigned int Variables>
Layer<Variables> andnot(const Layer<Variables>& a, const BooleanFunction<Variables>& b) {
	return Layer<Variables>(andnot(a.bf, b));
}


template<unsigned int Variables>
Layer<Variables> getBiggestLayer(const BooleanFunction<Variables>& boolFunc) {
	Layer<Variables> biggestLayer(boolFunc.getLayer(1));
	int biggestLayerSize = biggestLayer.size();

	for(int i = 2; i <= Variables; i++) {
		Layer<Variables> l = boolFunc.getLayer(i);
		int lSize = l.size();
		if(lSize > biggestLayerSize) {
			biggestLayer = l;
			biggestLayerSize = lSize;
		}
	}

	return biggestLayer;
}

template<unsigned int Variables>
Layer<Variables> getTopLayer(const BooleanFunction<Variables>& bf) {
	for(int i = Variables; i > 0; i--) {
		BooleanFunction<Variables> l = bf.getLayer(i);
		if(!l.isEmpty()) {
			return Layer<Variables>(l);
		}
	}
	return Layer<Variables>(bf);
}

template<unsigned int Variables>
Layer<Variables> getBottomLayer(const BooleanFunction<Variables>& bf) {
	for(int i = 0; i < Variables; i--) {
		BooleanFunction<Variables> l = bf.getLayer(i);
		if(!l.isEmpty()) {
			return Layer<Variables>(l);
		}
	}
	return Layer<Variables>(bf);
}

template<unsigned int Variables>
Layer<Variables> getTopLayer(const Monotonic<Variables>& m) {
	return getTopLayer(m.bf);
}
template<unsigned int Variables>
Layer<Variables> getTopLayer(const AntiChain<Variables>& ac) {
	return getTopLayer(ac.bf);
}
