#include "../toString.h"

#include "testsMain.h"

#include "../functionInput.h"
#include "../functionInputBitSet.h"


#include <random>

template<int Start, int End, template<int> typename FuncClass>
void runFunctionRange() {
	if constexpr(Start <= End) {
		FuncClass<Start>::run();
		runFunctionRange<Start + 1, End, FuncClass>();
	}
}

template<int Variables>
static bool operator==(const FunctionInputBitSet<Variables>& fibs, const std::vector<bool>& expectedState) {
	for(FunctionInput::underlyingType i = 0; i < FunctionInputBitSet<Variables>::maxSize(); i++) {
		if(expectedState[i] != fibs.contains(FunctionInput{i})) return false;
	}
	return true;
}
template<int Variables>
static bool operator!=(const FunctionInputBitSet<Variables>& fibs, const std::vector<bool>& expectedState) {
	return !(fibs == expectedState);
}
template<int Variables>
static bool operator==(const std::vector<bool>& expectedState, const FunctionInputBitSet<Variables>& fibs) {
	return fibs == expectedState;
}
template<int Variables>
static bool operator!=(const std::vector<bool>& expectedState, const FunctionInputBitSet<Variables>& fibs) {
	return fibs != expectedState;
}

bool genBool() {
	return rand() % 2 == 1;
}

template<int Variables>
static FunctionInputBitSet<Variables> generateFibs() {
	FunctionInputBitSet<Variables> fis = FunctionInputBitSet<Variables>::empty();
	
	for(FunctionInput::underlyingType i = 0; i < FunctionInputBitSet<Variables>::maxSize(); i++) {
		if(genBool()) {
			fis.add(FunctionInput{i});
		}
	}

	return fis;
}

template<int Variables>
static std::vector<bool> toVector(const FunctionInputBitSet<Variables>& fibs) {
	std::vector<bool> result(FunctionInputBitSet<Variables>::maxSize());

	for(FunctionInput::underlyingType i = 0; i < FunctionInputBitSet<Variables>::maxSize(); i++) {
		result[i] = fibs.contains(FunctionInput{i});
	}

	return result;
}

template<int Variables>
struct SetResetTest {
	static void run() {
		FunctionInputBitSet<Variables> fis = FunctionInputBitSet<Variables>::empty();
		std::vector<bool> expectedState(fis.maxSize(), false);

		ASSERT(fis == expectedState);

		for(size_t iter = 0; iter < 1000; iter++) {
			FunctionInput::underlyingType index = rand() % FunctionInputBitSet<Variables>::maxSize();
			
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

template<int Variables>
struct ShiftLeftTest {
	static void run() {
		for(size_t iter = 0; iter < 1000; iter++) {
			FunctionInputBitSet<Variables> fibs = generateFibs<Variables>();
			std::vector<bool> expectedState = toVector(fibs);

			int shift = rand() % FunctionInputBitSet<Variables>::maxSize();

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

template<int Variables>
struct ShiftRightTest {
	static void run() {
		for(size_t iter = 0; iter < 1000; iter++) {
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

template<int Variables>
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

template<int Variables>
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

template<int Variables>
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

TEST_CASE(testSetResetTestBit) {
	runFunctionRange<1, 9, SetResetTest>();
}

TEST_CASE(testShiftLeft) {
	runFunctionRange<1, 9, ShiftLeftTest>();
}

TEST_CASE(testShiftRight) {
	runFunctionRange<1, 9, ShiftRightTest>();
}

TEST_CASE(testFullVar) {
	runFunctionRange<1, 9, VarMaskTest>();
}

TEST_CASE(testMoveVar) {
	runFunctionRange<1, 9, MoveVariableTest>();
}

TEST_CASE(testSwapVar) {
	runFunctionRange<1, 9, SwapVariableTest>();
}
