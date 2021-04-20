#include "../dedelib/toString.h"

#include "testsMain.h"
#include "testUtils.h"
#include "../dedelib/generators.h"

#include "../dedelib/functionInput.h"
#include "../dedelib/booleanFunction.h"
#include "../dedelib/serialization.h"

#include <random>
#include <iostream>
#include <vector>


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
static bool operator==(const BooleanFunction<Variables>& bf, const std::vector<bool>& expectedState) {
	return bf.bitset == expectedState;
}
template<unsigned int Variables>
static bool operator!=(const BooleanFunction<Variables>& bf, const std::vector<bool>& expectedState) {
	return bf.bitset != expectedState;
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
static bool operator==(const std::vector<bool>& expectedState, const BooleanFunction<Variables>& bf) {
	return bf == expectedState;
}
template<unsigned int Variables>
static bool operator!=(const std::vector<bool>& expectedState, const BooleanFunction<Variables>& bf) {
	return bf != expectedState;
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
static std::vector<bool> toVector(const BooleanFunction<Variables>& bf) {
	return toVector(bf.bitset);
}

template<unsigned int Variables>
struct SetResetTest {
	static void run() {
		BooleanFunction<Variables> fis = BooleanFunction<Variables>::empty();
		std::vector<bool> expectedState(fis.maxSize(), false);

		ASSERT(fis == expectedState);

		for(size_t iter = 0; iter < LARGE_ITER; iter++) {
			FunctionInput::underlyingType index = rand() % BooleanFunction<Variables>::maxSize();
			
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
			BooleanFunction<Variables> bf = generateBF<Variables>();
			std::vector<bool> expectedState = toVector(bf);

			int shift = rand() % BooleanFunction<Variables>::maxSize();

			if constexpr(Variables == 7) {
				logStream << "data: " << std::hex << bf.bitset.data.m128i_i64[0] << ", " << bf.bitset.data.m128i_i64[1] << std::dec << " ";
			}

			logStream << "Original: " << bf << "\n";
			logStream << "Shift: " << shift << "\n";

			bf <<= shift;
			for(int i = bf.maxSize() - 1; i >= shift; i--) {
				expectedState[i] = expectedState[i - shift];
			}
			for(int i = 0; i < shift; i++) {
				expectedState[i] = false;
			}

			ASSERT(bf == expectedState);
		}
	}
};

template<unsigned int Variables>
struct ShiftRightTest {
	static void run() {
		for(size_t iter = 0; iter < LARGE_ITER; iter++) {
			BooleanFunction<Variables> fis = generateBF<Variables>();
			std::vector<bool> expectedState = toVector(fis);

			int shift = rand() % BooleanFunction<Variables>::maxSize();

			logStream << "Original: " << fis << "\n";
			logStream << "Shift: " << shift << "\n";

			fis >>= shift;
			for(int i = 0; i < BooleanFunction<Variables>::maxSize() - shift; i++) {
				expectedState[i] = expectedState[i + shift];
			}
			for(int i = BooleanFunction<Variables>::maxSize() - shift; i < BooleanFunction<Variables>::maxSize(); i++) {
				expectedState[i] = false;
			}

			ASSERT(fis == expectedState);
		}
	}
};

template<unsigned int Variables>
struct BitScanTest {
	static void run() {
		for(size_t iter = 0; iter < LARGE_ITER; iter++) {
			BooleanFunction<Variables> fis = BooleanFunction<Variables>::empty();

			size_t max = 0;
			size_t min = size_t(1) << Variables;
			for(int i = 0; i < 10; i++) {
				size_t set = generateSize_t(size_t(1) << Variables);
				max = std::max(max, set);
				min = std::min(min, set);

				fis.add(set);
			}

			ASSERT(fis.getFirst() == min);
			ASSERT(fis.getLast() == max);
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

template<unsigned int Variables>
struct MonotonizeUpDownDuality {
	static void run() {
		for(int iter = 0; iter < LARGE_ITER; iter++) {
			BooleanFunction<Variables> b = generateMonotonic<Variables>().asAntiChain().bf;

			ASSERT(b.monotonizeUp() == b.reverse().monotonizeDown().reverse());
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
			BooleanFunction<Variables> result(BooleanFunction<Variables>::varMask(var));

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
				BooleanFunction<Variables> bf = generateBF<Variables>();

				bf &= BooleanFunction<Variables>(~BooleanFunction<Variables>::varMask(var2));

				logStream << "Moving " << var1 << " to " << var2 << " in " << bf << "\n";


				for(unsigned int i = 0; i < bf.maxSize(); i++) {
					if(bf.contains(FunctionInput{i})) {
						funcInputs.push_back(i);
					}
				}


				// do operation on both
				bf.move(var1, var2);
				for(unsigned int& item : funcInputs) {
					if(item & (1 << var1)) {
						item &= ~(1 << var1);
						item |= 1 << var2;
					}
				}

				BooleanFunction<Variables> checkFibs;
				for(unsigned int item : funcInputs) {
					checkFibs.add(item);
				}

				ASSERT(bf == checkFibs);

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
				BooleanFunction<Variables> bf = generateBF<Variables>();

				logStream << "Swapping " << var1 << " and " << var2 << " in " << bf << "\n";


				for(unsigned int i = 0; i < bf.maxSize(); i++) {
					if(bf.contains(FunctionInput{i})) {
						funcInputs.push_back(i);
					}
				}


				// do operation on both
				bf.swap(var1, var2);
				for(unsigned int& item : funcInputs) {
					bool isVar1Active = item & (1 << var1);
					bool isVar2Active = item & (1 << var2);

					item = item & ~(1 << var1) & ~(1 << var2);

					if(isVar1Active) item |= (1 << var2);
					if(isVar2Active) item |= (1 << var1);
				}

				BooleanFunction<Variables> checkFibs;
				for(unsigned int item : funcInputs) {
					checkFibs.add(item);
				}

				ASSERT(bf == checkFibs);

				funcInputs.clear();
			}
		}
	}
};

template<unsigned int Variables>
struct CanonizeTest {
	static void run() {
		for(int iter = 0; iter < SMALL_ITER; iter++) {
			BooleanFunction<Variables> bf = generateBF<Variables>();

			BooleanFunction<Variables> canonizedFibs = bf.canonize();

			bf.forEachPermutation(0, Variables, [&canonizedFibs](const BooleanFunction<Variables>& permut) {
				ASSERT(permut.canonize() == canonizedFibs);
			});
		}
	}
};

template<unsigned int Variables>
bool isMonotonicNaive(const BooleanFunction<Variables>& bf) {
	for(size_t i = 0; i < (1 << Variables); i++) {
		if(bf.bitset.get(i)) {
			for(size_t j = 0; j < (1 << Variables); j++) {
				if((j & i) == j) { // j is subset of i
					if(bf.bitset.get(j) == false) return false;
				}
			}
		} else {
			for(size_t j = 0; j < (1 << Variables); j++) {
				if((i & j) == i) { // j is superset of i
					if(bf.bitset.get(j) == true) return false;
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
			BooleanFunction<Variables> mfunc = generateMBF<Variables>();

			logStream << mfunc << "\n";

			ASSERT(isMonotonicNaive(mfunc));
			ASSERT(mfunc.isMonotonic());

			for(size_t i = 0; i < (1 << Variables); i++) {
				BooleanFunction<Variables> copy = mfunc;
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
				BooleanFunction<Variables> layerFibs = generateLayer<Variables>(layer);

				if(layerFibs.isEmpty()) continue;

				logStream << layerFibs << "\n";

				ASSERT(layerFibs.isLayer());

				BooleanFunction<Variables> n = layerFibs.succ();
				n.remove(0);
				BooleanFunction<Variables> p = layerFibs.pred();

				logStream << " p:" << p << " n:" << n << "\n";

				ASSERT(p.isLayer());
				ASSERT(n.isLayer());

				for(unsigned int i = 0; i < (1 << Variables); i++) {
					BooleanFunction<Variables> copy = layerFibs;
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
			BooleanFunction<Variables> mbf = generateMBF<Variables>();
			BooleanFunction<Variables> pred = mbf.pred();
			//pred.bitset.set(0); // 0 is undefined

			logStream << "mbf: " << mbf << "\n";
			logStream << "pred: " << pred << "\n";

			BooleanFunction<Variables> correctPrev = BooleanFunction<Variables>::empty();

			for(size_t checkBit = 0; checkBit < BooleanFunction<Variables>::maxSize(); checkBit++) {
				for(size_t forcingBit = 0; forcingBit < BooleanFunction<Variables>::maxSize(); forcingBit++) {
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

			ASSERT(pred == correctPrev);
		}
	}
};

template<unsigned int Variables>
struct NextTest {
	static void run() {
		for(int iter = 0; iter < LARGE_ITER; iter++) {
			BooleanFunction<Variables> mbf = generateMBF<Variables>();
			BooleanFunction<Variables> succ = mbf.succ();
			
			logStream << "mbf: " << mbf << "\n";
			logStream << "succ: " << succ << "\n";

			BooleanFunction<Variables> correctNext = BooleanFunction<Variables>::full();

			for(size_t checkBit = 0; checkBit < BooleanFunction<Variables>::maxSize(); checkBit++) {
				for(size_t forcingBit = 0; forcingBit < BooleanFunction<Variables>::maxSize(); forcingBit++) {
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

			ASSERT(succ == correctNext);
		}
	}
};

template<unsigned int Variables>
struct SerializationTest {
	static void run() {
		for(int iter = 0; iter < LARGE_ITER; iter++) {
			uint8_t buf[getMBFSizeInBytes<Variables>()];

			BooleanFunction<Variables> bf = generateBF<Variables>();

			uint8_t* end = serializeBooleanFunction(bf, buf);
			ASSERT(end == buf + getMBFSizeInBytes<Variables>());

			BooleanFunction<Variables> deserialfunc = deserializeBooleanFunction<Variables>(buf);

			ASSERT(bf == deserialfunc);
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

template<unsigned int Variables>
struct ForEachSubSetTest {
	static void run() {
		for(int iter = 0; iter < LARGE_ITER; iter++) {
			std::vector<int> selectedBits;

			constexpr int MAX_SELECTED = 15;

			int selectCount = generateInt(std::min((1 << Variables), MAX_SELECTED));
			for(int i = 0; i < selectCount; i++) {
				int newBit = generateInt((1 << Variables));
				for(int prevSelected : selectedBits) {
					if(newBit == prevSelected) {
						goto alreadyExists;
					}
				}
				selectedBits.push_back(newBit);

				alreadyExists:;
			}
			BitSet<(1 << Variables)> bitset = BitSet<(1 << Variables)>::empty();

			for(int bit : selectedBits) {
				bitset.set(bit);
			}

			BitSet<1 << MAX_SELECTED> foundSubSets = BitSet<1 << MAX_SELECTED>::empty();

			size_t foundCount = 0;

			bitset.forEachSubSet([&](const BitSet<1 << Variables>& subSet) {
				ASSERT(isSubSet(subSet, bitset));

				unsigned int subSetFound = 0;
				for(int varIdx = 0; varIdx < selectedBits.size(); varIdx++) {
					if(subSet.get(selectedBits[varIdx])) {
						subSetFound |= (1U << varIdx);
					}
				}

				ASSERT(!foundSubSets.get(subSetFound));
				foundSubSets.set(subSetFound);

				foundCount++;
			});

			ASSERT(foundCount == 1 << selectedBits.size());
		}
	}
};

template<unsigned int Variables>
struct ACProdTest {
	static void run() {
		for(int iter = 0; iter < LARGE_ITER; iter++) {
			Monotonic<Variables> m1 = generateMonotonic<Variables>();
			Monotonic<Variables> m2 = generateMonotonic<Variables>();

			m2 = Monotonic<Variables>(andnot(m2.bf.bitset, BooleanFunction<Variables>::multiVarMask(m1.getUniverse())));

			//printVar(m1);
			//printVar(m2);

			ASSERT(acProd(m1, m2) == (m1.asAntiChain() * m2.asAntiChain()).asMonotonic());
			ASSERT(acProd(m2, m1) == (m2.asAntiChain() * m1.asAntiChain()).asMonotonic());
			ASSERT(acProd(m1, m2) == acProd(m2, m1));
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

TEST_CASE(testBitScan) {
	runFunctionRange<TEST_FROM, TEST_UPTO, BitScanTest>();
}

TEST_CASE(testReverse) {
	runFunctionRange<TEST_FROM, TEST_UPTO, ReverseTest>();
}

TEST_CASE(testMonotonizeUpDownDuality) {
	runFunctionRange<TEST_FROM, TEST_UPTO, MonotonizeUpDownDuality>();
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

TEST_CASE_SLOW(testLayerWise) {
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

TEST_CASE(testForEachSubSet) {
	runFunctionRange<TEST_FROM, TEST_UPTO, ForEachSubSetTest>();
}

TEST_CASE(testAntiChainMul) {
	runFunctionRange<TEST_FROM, TEST_UPTO, ACProdTest>();
}

/*TEST_CASE(benchCanonize) {
	BooleanFunction<7> nonOptimizer;
	size_t canonCount = 10000000;
	BooleanFunction<7> bf = generateBF<7>();
	for(size_t iter = 0; iter < canonCount; iter++) {
		bf = bf << 1 ^ bf;
		if(iter % 50 == 0) bf ^= generateBF<7>();
		nonOptimizer ^= bf.canonize();
	}
	std::cout << nonOptimizer << "\n";
	std::cout << "Ran " << canonCount << " canonisations! ";
}*/
