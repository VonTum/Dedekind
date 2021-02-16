#include "../toString.h"

#include "testsMain.h"
#include "testUtils.h"
#include "generators.h"

#include "../functionInput.h"
#include "../functionInputBitSet.h"

#include <random>
#include <iostream>


template<size_t Size>
static bool operator==(const BitSet<Size>& bs, const std::vector<bool>& expectedState) {
	assert(expectedState.size() == Size);
	for(size_t i = 0; i < Size; i++) {
		if(expectedState[i] != bs.get(i)) return false;
	}
	return true;
}
template<size_t Size>
static bool operator!=(const BitSet<Size>& bs, const std::vector<bool>& expectedState) {
	return !(bs == expectedState);
}
template<unsigned int Variables>
static bool operator==(const FunctionInputBitSet<Variables>& fibs, const std::vector<bool>& expectedState) {
	return fibs.bitset == expectedState;
}
template<unsigned int Variables>
static bool operator!=(const FunctionInputBitSet<Variables>& fibs, const std::vector<bool>& expectedState) {
	return fibs.bitset != expectedState;
}
template<size_t Size>
static bool operator==(const std::vector<bool>& expectedState, const BitSet<Size>& bs) {
	return bs == expectedState;
}
template<size_t Size>
static bool operator!=(const std::vector<bool>& expectedState, const BitSet<Size>& bs) {
	return bs != expectedState;
}
template<unsigned int Variables>
static bool operator==(const std::vector<bool>& expectedState, const FunctionInputBitSet<Variables>& fibs) {
	return fibs == expectedState;
}
template<unsigned int Variables>
static bool operator!=(const std::vector<bool>& expectedState, const FunctionInputBitSet<Variables>& fibs) {
	return fibs != expectedState;
}

template<size_t Size>
static std::vector<bool> toVector(const BitSet<Size>& bs) {
	std::vector<bool> result(Size);

	for(size_t i = 0; i < Size; i++) {
		result[i] = bs.get(i);
	}

	return result;
}

template<unsigned int Variables>
static std::vector<bool> toVector(const FunctionInputBitSet<Variables>& fibs) {
	return toVector(fibs.bitset);
}

template<unsigned int Variables>
struct SetResetTest {
	static void run() {
		FunctionInputBitSet<Variables> fis = FunctionInputBitSet<Variables>::empty();
		std::vector<bool> expectedState(fis.maxSize(), false);

		ASSERT(fis == expectedState);

		for(size_t iter = 0; iter < LARGE_ITER; iter++) {
			FunctionInput::underlyingType index = rand() % FunctionInputBitSet<Variables>::maxSize();
			
			if constexpr(Variables == 7) {
				logStream << "data: " << std::hex << fis.bitset.data.m128i_i64[0] << ", " << fis.bitset.data.m128i_i64[1] << std::dec << " ";
			}

			logStream << "index: " << index << "\n";

			if(expectedState[index]) {
				fis.remove(FunctionInput{index});
				expectedState[index] = false;
			} else {
				fis.add(FunctionInput{index});
				expectedState[index] = true;
			}

			ASSERT(fis == expectedState);
		}
	}
};

template<unsigned int Variables>
struct ShiftLeftTest {
	static void run() {
		for(size_t iter = 0; iter < LARGE_ITER; iter++) {
			FunctionInputBitSet<Variables> fibs = generateFibs<Variables>();
			std::vector<bool> expectedState = toVector(fibs);

			int shift = rand() % FunctionInputBitSet<Variables>::maxSize();

			if constexpr(Variables == 7) {
				logStream << "data: " << std::hex << fibs.bitset.data.m128i_i64[0] << ", " << fibs.bitset.data.m128i_i64[1] << std::dec << " ";
			}

			logStream << "Original: " << fibs << "\n";
			logStream << "Shift: " << shift << "\n";

			fibs <<= shift;
			for(int i = fibs.maxSize() - 1; i >= shift; i--) {
				expectedState[i] = expectedState[i - shift];
			}
			for(int i = 0; i < shift; i++) {
				expectedState[i] = false;
			}

			ASSERT(fibs == expectedState);
		}
	}
};

template<unsigned int Variables>
struct ShiftRightTest {
	static void run() {
		for(size_t iter = 0; iter < LARGE_ITER; iter++) {
			FunctionInputBitSet<Variables> fis = generateFibs<Variables>();
			std::vector<bool> expectedState = toVector(fis);

			int shift = rand() % FunctionInputBitSet<Variables>::maxSize();

			logStream << "Original: " << fis << "\n";
			logStream << "Shift: " << shift << "\n";

			fis >>= shift;
			for(int i = 0; i < FunctionInputBitSet<Variables>::maxSize() - shift; i++) {
				expectedState[i] = expectedState[i + shift];
			}
			for(int i = FunctionInputBitSet<Variables>::maxSize() - shift; i < FunctionInputBitSet<Variables>::maxSize(); i++) {
				expectedState[i] = false;
			}

			ASSERT(fis == expectedState);
		}
	}
};

template<unsigned int Variables>
struct ReverseTest {
	static void run() {
		for(size_t iter = 0; iter < LARGE_ITER; iter++) {
			BitSet<(1 << Variables)> bs = generateBitSet<(1 << Variables)>();
			std::vector<bool> expectedState = toVector(bs);
			std::reverse(expectedState.begin(), expectedState.end());
			BitSet<(1 << Variables)> reversed = bs.reverse();
			ASSERT(reversed == expectedState);
		}
	}
};

// not explicitly needed
// compare need only be transitive, no other requirements
/*template<unsigned int Variables>
struct CompareBitsTest {
	static void run() {
		for(size_t bit = 0; bit < (1 << Variables) - 1; bit++) {
			BitSet<(1 << Variables)> a;
			BitSet<(1 << Variables)> b;

			a.set(bit);
			b.set(bit + 1);

			logStream << "bit: " << bit << "\n";

			ASSERT(a < b);
			ASSERT(a <= b);
			ASSERT(a != b);
			ASSERT(!(a > b));
			ASSERT(!(a >= b));
			ASSERT(!(a == b));
		}
	}
};*/

template<unsigned int Variables>
struct CompareTransitiveTest {
	static void run() {
		for(size_t iter = 0; iter < LARGE_ITER; iter++) {
			constexpr size_t Size = (1 << Variables);
			BitSet<Size> a = generateBitSet<Size>();
			BitSet<Size> b = generateBitSet<Size>();
			BitSet<Size> c = generateBitSet<Size>();

			if(a < b && b < c) {
				ASSERT(a < c);
			}
			if(a <= b && b <= c) {
				ASSERT(a <= c);
			}
		}
	}
};


template<unsigned int Variables>
struct CompareTest {
	static void run() {
		for(size_t iter = 0; iter < LARGE_ITER; iter++) {
			constexpr size_t Size = (1 << Variables);
			BitSet<Size> a = generateBitSet<Size>();
			BitSet<Size> b = generateBitSet<Size>();
			
			logStream << "a: " << a << "\nb: " << b << "\n";

			ASSERT((a < b) == (b > a));
			ASSERT((a <= b) == (b >= a));
			if(a < b) {
				ASSERT(!(b < a));
				ASSERT(a <= b);
			} else {
				ASSERT(a >= b);
			}
		}
	}
};

template<unsigned int Variables>
struct VarMaskTest {
	static void run() {
		for(unsigned int var = 0; var < Variables; var++) {
			FunctionInputBitSet<Variables> result(FunctionInputBitSet<Variables>::varMask(var));

			logStream << result << "\n";

			for(FunctionInput::underlyingType i = 0; i < result.maxSize(); i++) {
				bool shouldBeEnabled = (i & (1 << var)) != 0;
				ASSERT(result.contains(i) == shouldBeEnabled);
			}
		}
	}
};

template<unsigned int Variables>
struct MoveVariableTest {
	static void run() {
		std::vector<unsigned int> funcInputs;
		funcInputs.reserve(1 << Variables);
		for(unsigned int var1 = 0; var1 < Variables; var1++) {
			for(unsigned int var2 = 0; var2 < Variables; var2++) {
				FunctionInputBitSet<Variables> fibs = generateFibs<Variables>();

				fibs &= FunctionInputBitSet<Variables>(~FunctionInputBitSet<Variables>::varMask(var2));

				logStream << "Moving " << var1 << " to " << var2 << " in " << fibs << "\n";


				for(unsigned int i = 0; i < fibs.maxSize(); i++) {
					if(fibs.contains(FunctionInput{i})) {
						funcInputs.push_back(i);
					}
				}


				// do operation on both
				fibs.move(var1, var2);
				for(unsigned int& item : funcInputs) {
					if(item & (1 << var1)) {
						item &= ~(1 << var1);
						item |= 1 << var2;
					}
				}

				FunctionInputBitSet<Variables> checkFibs;
				for(unsigned int item : funcInputs) {
					checkFibs.add(item);
				}

				ASSERT(fibs == checkFibs);

				funcInputs.clear();
			}
		}
	}
};

template<unsigned int Variables>
struct SwapVariableTest {
	static void run() {
		std::vector<unsigned int> funcInputs;
		funcInputs.reserve(1 << Variables);
		for(unsigned int var1 = 0; var1 < Variables; var1++) {
			for(unsigned int var2 = 0; var2 < Variables; var2++) {
				FunctionInputBitSet<Variables> fibs = generateFibs<Variables>();

				logStream << "Swapping " << var1 << " and " << var2 << " in " << fibs << "\n";


				for(unsigned int i = 0; i < fibs.maxSize(); i++) {
					if(fibs.contains(FunctionInput{i})) {
						funcInputs.push_back(i);
					}
				}


				// do operation on both
				fibs.swap(var1, var2);
				for(unsigned int& item : funcInputs) {
					bool isVar1Active = item & (1 << var1);
					bool isVar2Active = item & (1 << var2);

					item = item & ~(1 << var1) & ~(1 << var2);

					if(isVar1Active) item |= (1 << var2);
					if(isVar2Active) item |= (1 << var1);
				}

				FunctionInputBitSet<Variables> checkFibs;
				for(unsigned int item : funcInputs) {
					checkFibs.add(item);
				}

				ASSERT(fibs == checkFibs);

				funcInputs.clear();
			}
		}
	}
};

template<unsigned int Variables>
struct CanonizeTest {
	static void run() {
		for(int iter = 0; iter < SMALL_ITER; iter++) {
			FunctionInputBitSet<Variables> fibs = generateFibs<Variables>();

			FunctionInputBitSet<Variables> canonizedFibs = fibs.canonize();

			fibs.forEachPermutation(0, Variables, [&canonizedFibs](const FunctionInputBitSet<Variables>& permut) {
				ASSERT(permut.canonize() == canonizedFibs);
			});
		}
	}
};

template<unsigned int Variables>
bool isMonotonicNaive(const FunctionInputBitSet<Variables>& fibs) {
	for(size_t i = 0; i < (1 << Variables); i++) {
		if(fibs.bitset.get(i)) {
			for(size_t j = 0; j < (1 << Variables); j++) {
				if((j & i) == j) { // j is subset of i
					if(fibs.bitset.get(j) == false) return false;
				}
			}
		} else {
			for(size_t j = 0; j < (1 << Variables); j++) {
				if((i & j) == i) { // j is superset of i
					if(fibs.bitset.get(j) == true) return false;
				}
			}
		}
	}
	return true;
}

template<unsigned int Variables>
struct MBFTest {
	static void run() {
		for(int iter = 0; iter < SMALL_ITER; iter++) {
			FunctionInputBitSet<Variables> mfibs = generateMBF<Variables>();

			logStream << mfibs << "\n";

			ASSERT(isMonotonicNaive(mfibs));
			ASSERT(mfibs.isMonotonic());

			for(size_t i = 0; i < (1 << Variables); i++) {
				FunctionInputBitSet<Variables> copy = mfibs;
				copy.bitset.toggle(i);
				logStream << "+" << copy << "\n";

				ASSERT(isMonotonicNaive(copy) == copy.isMonotonic());
			}
		}
	}
};

template<unsigned int Variables>
struct LayerWiseTest {
	static void run() {
		for(unsigned int layer = 0; layer < Variables + 1; layer++) {
			for(int iter = 0; iter < SMALL_ITER; iter++) {
				FunctionInputBitSet<Variables> layerFibs = generateLayer<Variables>(layer);

				if(layerFibs.isEmpty()) continue;

				logStream << layerFibs << "\n";

				ASSERT(layerFibs.isLayer());

				FunctionInputBitSet<Variables> n = layerFibs.next();
				n.remove(0);
				FunctionInputBitSet<Variables> p = layerFibs.prev();

				logStream << " p:" << p << " n:" << n << "\n";

				ASSERT(p.isLayer());
				ASSERT(n.isLayer());

				for(unsigned int i = 0; i < (1 << Variables); i++) {
					FunctionInputBitSet<Variables> copy = layerFibs;
					copy.bitset.set(i);
					logStream << "+" << copy;

					bool shouldBeLayer = __popcnt(i) == layer;

					ASSERT(shouldBeLayer == copy.isLayer());
				}
			}
		}
	}
};

template<unsigned int Variables>
struct PrevTest {
	static void run() {
		for(int iter = 0; iter < LARGE_ITER; iter++) {
			FunctionInputBitSet<Variables> mbf = generateMBF<Variables>();
			FunctionInputBitSet<Variables> prev = mbf.prev();
			//prev.bitset.set(0); // 0 is undefined

			logStream << "mbf: " << mbf << "\n";
			logStream << "prev: " << prev << "\n";

			FunctionInputBitSet<Variables> correctPrev = FunctionInputBitSet<Variables>::empty();

			for(size_t checkBit = 0; checkBit < FunctionInputBitSet<Variables>::maxSize(); checkBit++) {
				for(size_t forcingBit = 0; forcingBit < FunctionInputBitSet<Variables>::maxSize(); forcingBit++) {
					if(forcingBit == checkBit) continue;
					if(checkBit == (checkBit & forcingBit)) { // checkBit is subSet of forcingBit
						if(mbf.bitset.get(forcingBit) == true) {
							correctPrev.bitset.set(checkBit);
							break;
						}
					}
				}
			}
			logStream << "correctPrev: " << correctPrev << "\n";

			ASSERT(prev == correctPrev);
		}
	}
};

template<unsigned int Variables>
struct NextTest {
	static void run() {
		for(int iter = 0; iter < LARGE_ITER; iter++) {
			FunctionInputBitSet<Variables> mbf = generateMBF<Variables>();
			FunctionInputBitSet<Variables> next = mbf.next();
			
			logStream << "mbf: " << mbf << "\n";
			logStream << "next: " << next << "\n";

			FunctionInputBitSet<Variables> correctNext = FunctionInputBitSet<Variables>::full();

			for(size_t checkBit = 0; checkBit < FunctionInputBitSet<Variables>::maxSize(); checkBit++) {
				for(size_t forcingBit = 0; forcingBit < FunctionInputBitSet<Variables>::maxSize(); forcingBit++) {
					if(forcingBit == checkBit) continue;
					if(forcingBit == (checkBit & forcingBit)) { // forcingBit is subSet of checkBit
						if(mbf.bitset.get(forcingBit) == false) {
							correctNext.bitset.reset(checkBit);
							break;
						}
					}
				}
			}
			logStream << "correctNext: " << correctNext << "\n";

			ASSERT(next == correctNext);
		}
	}
};

template<unsigned int Variables>
struct SerializationTest {
	static void run() {
		for(int iter = 0; iter < LARGE_ITER; iter++) {
			uint8_t buf[getMBFSizeInBytes<Variables>()];

			FunctionInputBitSet<Variables> fibs = generateFibs<Variables>();

			uint8_t* end = serializeMBF(fibs, buf);
			ASSERT(end == buf + getMBFSizeInBytes<Variables>());

			FunctionInputBitSet<Variables> deserialfibs = deserializeMBF<Variables>(buf);

			ASSERT(fibs == deserialfibs);
		}
	}
};

template<unsigned int Variables>
struct CountZerosTest {
	static void run() {
		for(int i = 0; i < (1 << Variables); i++) {
			BitSet<(1 << Variables)> bits = BitSet<(1 << Variables)>::empty();

			bits.set(i);

			ASSERT(bits.getFirstOnBit() == i);

			for(int j = 0; j < (1 << Variables); j++) {
				int randomBit = rand() % (1 << Variables);

				if(randomBit < i) continue;

				bits.set(randomBit);

				ASSERT(bits.getFirstOnBit() == i);
			}
		}
	}
};

template<unsigned int Variables>
struct ForEachOneTest {
	static void run() {
		for(int iter = 0; iter < LARGE_ITER; iter++) {
			BitSet<(1 << Variables)> bits = generateBitSet<1 << Variables>();

			BitSet<(1 << Variables)> foundBits = BitSet<(1 << Variables)>::empty();

			bits.forEachOne([&](size_t bit) {
				ASSERT(foundBits.get(bit) == false);
				foundBits.set(bit);
			});

			ASSERT(foundBits == bits);
		}
	}
};


/*TEST_CASE(testBitsCompare) {
	runFunctionRange<TEST_FROM, TEST_UPTO, CompareBitsTest>();
}*/

TEST_CASE(testSetResetTestBit) {
	runFunctionRange<TEST_FROM, TEST_UPTO, SetResetTest>();
}

TEST_CASE(testShiftLeft) {
	runFunctionRange<TEST_FROM, TEST_UPTO, ShiftLeftTest>();
}

TEST_CASE(testShiftRight) {
	runFunctionRange<TEST_FROM, TEST_UPTO, ShiftRightTest>();
}

TEST_CASE(testReverse) {
	runFunctionRange<TEST_FROM, TEST_UPTO, ReverseTest>();
}

TEST_CASE(testCompareTransitive) {
	runFunctionRange<TEST_FROM, TEST_UPTO, CompareTransitiveTest>();
}

TEST_CASE(testCompare) {
	runFunctionRange<TEST_FROM, TEST_UPTO, CompareTest>();
}

TEST_CASE(testFullVar) {
	runFunctionRange<TEST_FROM, TEST_UPTO, VarMaskTest>();
}

TEST_CASE(testMoveVar) {
	runFunctionRange<TEST_FROM, TEST_UPTO, MoveVariableTest>();
}

TEST_CASE(testSwapVar) {
	runFunctionRange<TEST_FROM, TEST_UPTO, SwapVariableTest>();
}

TEST_CASE(testCanonize) {
	runFunctionRange<TEST_FROM, 7, CanonizeTest>();
}

TEST_CASE(testMBF) {
	runFunctionRange<TEST_FROM, 7, MBFTest>();
}

TEST_CASE(testLayerWise) {
	runFunctionRange<TEST_FROM, 7, LayerWiseTest>();
}

TEST_CASE(testPrev) {
	runFunctionRange<TEST_FROM, 7, PrevTest>();
}

TEST_CASE(testNext) {
	runFunctionRange<TEST_FROM, 7, NextTest>();
}

TEST_CASE(testSerialization) {
	runFunctionRange<TEST_FROM, TEST_UPTO, SerializationTest>();
}

TEST_CASE(testCountZeros) {
	runFunctionRange<TEST_FROM, TEST_UPTO, CountZerosTest>();
}

TEST_CASE(testForEachOne) {
	runFunctionRange<TEST_FROM, TEST_UPTO, ForEachOneTest>();
}

/*TEST_CASE(benchCanonize) {
	FunctionInputBitSet<7> nonOptimizer;
	size_t canonCount = 10000000;
	FunctionInputBitSet<7> fibs = generateFibs<7>();
	for(size_t iter = 0; iter < canonCount; iter++) {
		fibs = fibs << 1 ^ fibs;
		if(iter % 50 == 0) fibs ^= generateFibs<7>();
		nonOptimizer ^= fibs.canonize();
	}
	std::cout << nonOptimizer << "\n";
	std::cout << "Ran " << canonCount << " canonisations! ";
}*/
