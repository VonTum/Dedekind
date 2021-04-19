#pragma once

#include "iteratorEnd.h"
#include "iteratorFactory.h"
#include "smallVector.h"

template<size_t BlockSize, typename Iter, typename IterEnd = Iter>
class BlockIterator {
	Iter iter;
	IterEnd iterEnd;

public:
	BlockIterator(Iter iter, IterEnd iterEnd) : iter(iter), iterEnd(iterEnd) {}

	using ItemType = typename std::remove_reference<decltype(*iter)>::type;

	SmallVector<ItemType*, BlockSize> operator*() const {
		Iter iterCopy = iter;

		SmallVector<ItemType*, BlockSize> result;

		for(size_t i = 0; i < BlockSize; i++) {
			if(!(iterCopy != iterEnd)) break;

			ItemType& item = *iterCopy;
			result.push_back(&item);
			
			++iterCopy;
		}

		return result;
	}

	BlockIterator& operator++() {
		for(size_t i = 0; i < BlockSize; i++) {
			if(!(iter != iterEnd)) break;

			++iter;
		}
		return *this;
	}

	bool operator!=(IteratorEnd) {
		return iter != iterEnd;
	}
};

template<size_t BlockSize, typename Collection>
auto makeBlockIterFor(Collection& c) -> BlockIterator<BlockSize, decltype(c.begin()), decltype(c.end())> {
	return BlockIterator<BlockSize, decltype(c.begin()), decltype(c.end())>(c.begin(), c.end());
}

template<size_t BlockSize, typename Collection>
auto makeBlockIterFor(const Collection& c) -> BlockIterator<BlockSize, decltype(c.begin()), decltype(c.end())> {
	return BlockIterator<BlockSize, decltype(c.begin()), decltype(c.end())>(c.begin(), c.end());
}

template<size_t BlockSize, typename Collection>
auto iterInBlocks(Collection& c) {
	return IteratorFactoryWithEnd<decltype(makeBlockIterFor<BlockSize>(c))>(makeBlockIterFor<BlockSize>(c));
}

template<size_t BlockSize, typename Collection>
auto iterInBlocks(const Collection& c) {
	return IteratorFactoryWithEnd<decltype(makeBlockIterFor<BlockSize>(c))>(makeBlockIterFor<BlockSize>(c));
}
