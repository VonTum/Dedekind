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
	for(FunctionInput::underlyingType i = 0; i < FunctionInputBitSet<Variables>::size(); i++) {
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
	
	for(FunctionInput::underlyingType i = 0; i < FunctionInputBitSet<Variables>::size(); i++) {
		if(genBool()) {
			fis.add(FunctionInput{i});
		}
	}

	return fis;
}

template<int Variables>
static std::vector<bool> toVector(const FunctionInputBitSet<Variables>& fibs) {
	std::vector<bool> result(FunctionInputBitSet<Variables>::size());

	for(FunctionInput::underlyingType i = 0; i < FunctionInputBitSet<Variables>::size(); i++) {
		result[i] = fibs.contains(FunctionInput{i});
	}

	return result;
}

template<int Variables>
struct SetResetTestBitAuxTester {
	static void run() {
		FunctionInputBitSet<Variables> fis = FunctionInputBitSet<Variables>::empty();
		std::vector<bool> expectedState(fis.size(), false);

		ASSERT(fis == expectedState);

		for(size_t iter = 0; iter < 1000; iter++) {
			FunctionInput::underlyingType index = rand() % FunctionInputBitSet<Variables>::size();
			
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
struct ShiftLeftTestBitAuxTester {
	static void run() {
		for(size_t iter = 0; iter < 1000; iter++) {
			FunctionInputBitSet<Variables> fis = generateFibs<Variables>();
			std::vector<bool> expectedState = toVector(fis);

			int shift = rand() % FunctionInputBitSet<Variables>::size();

			logStream << "Original: " << fis << "\n";
			logStream << "Shift: " << shift << "\n";

			fis <<= shift;
			for(int i = expectedState.size() - 1; i >= shift; i--) {
				expectedState[i] = expectedState[i - shift];
			}
			for(int i = 0; i < shift; i++) {
				expectedState[i] = false;
			}

			ASSERT(fis == expectedState);
		}
	}
};

template<int Variables>
struct ShiftRightTestBitAuxTester {
	static void run() {
		for(size_t iter = 0; iter < 1000; iter++) {
			FunctionInputBitSet<Variables> fis = generateFibs<Variables>();
			std::vector<bool> expectedState = toVector(fis);

			int shift = rand() % FunctionInputBitSet<Variables>::size();

			logStream << "Original: " << fis << "\n";
			logStream << "Shift: " << shift << "\n";

			fis >>= shift;
			for(int i = 0; i < FunctionInputBitSet<Variables>::size() - shift; i++) {
				expectedState[i] = expectedState[i + shift];
			}
			for(int i = FunctionInputBitSet<Variables>::size() - shift; i < FunctionInputBitSet<Variables>::size(); i++) {
				expectedState[i] = false;
			}

			ASSERT(fis == expectedState);
		}
	}
};

template<int Variables>
struct VarMaskTestBitAuxTester {
	static void run() {
		for(unsigned int var = 0; var < Variables; var++) {
			FunctionInputBitSet<Variables> result = FunctionInputBitSet<Variables>::varMask(var);

			logStream << result << "\n";

			for(FunctionInput::underlyingType i = 0; i < result.size(); i++) {
				bool shouldBeEnabled = (i & (1 << var)) != 0;
				ASSERT(result.contains(i) == shouldBeEnabled);
			}
		}
	}
};

TEST_CASE(testSetResetTestBit) {
	runFunctionRange<1, 9, SetResetTestBitAuxTester>();
}

TEST_CASE(testShiftLeft) {
	runFunctionRange<1, 9, ShiftLeftTestBitAuxTester>();
}

TEST_CASE(testShiftRight) {
	runFunctionRange<1, 9, ShiftRightTestBitAuxTester>();
}

TEST_CASE(testFullVar) {
	runFunctionRange<1, 9, VarMaskTestBitAuxTester>();
}
