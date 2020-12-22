#include "testsMain.h"

#include "../functionInput.h"
#include "../functionInputBitSet.h"
#include "../toString.h"

#include <random>

template<int Start, int End, template<int> typename FuncClass>
void runFunctionRange() {
	if constexpr(Start <= End) {
		FuncClass<Start>::run();
		runFunctionRange<Start + 1, End, FuncClass>();
	}
}

template<int Variables>
struct SetResetTestBitAuxTester {
	static void run() {
		FunctionInputBitSet<Variables> fis = FunctionInputBitSet<Variables>::empty();
		std::vector<bool> expectedState(fis.size(), false);

		for(FunctionInput::underlyingType i = 0; i < FunctionInputBitSet<Variables>::size(); i++) {
			ASSERT(expectedState[i] == fis.contains(FunctionInput{i}));
		}

		for(size_t iter = 0; iter < 1000; iter++) {
			FunctionInput::underlyingType index = rand() % FunctionInputBitSet<Variables>::size();
			
			if(expectedState[index]) {
				fis.remove(FunctionInput{index});
				expectedState[index] = false;
			} else {
				fis.add(FunctionInput{index});
				expectedState[index] = true;
			}

			for(FunctionInput::underlyingType i = 0; i < FunctionInputBitSet<Variables>::size(); i++) {
				ASSERT(expectedState[i] == fis.contains(FunctionInput{i}));
			}
		}
	}
};

TEST_CASE(testSetResetTestBit) {
	runFunctionRange<6, 9, SetResetTestBitAuxTester>();
}
