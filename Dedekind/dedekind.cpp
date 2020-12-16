
#include <iostream>
#include <cassert>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <algorithm>

#include "functionInput.h"
#include "functionInputSet.h"
#include "equivalenceClass.h"
#include "equivalenceClassMap.h"
#include "layerDecomposition.h"
#include "dedekindDecomposition.h"
#include "valuedDecomposition.h"
#include "layerStack.h"
#include "toString.h"

#include "timeTracker.h"
#include "codeGen.h"

#include "parallelIter.h"

#include <mutex>
#include <atomic>

/*
Correct numbers
	0: 2
	1: 3
	2: 6
	3: 20
	4: 168
	5: 7581
	6: 7828354
	7: 2414682040998
	8: 56130437228687557907788
	9: ??????????????????????????????????????????
*/

int main() {
	std::cout << "Detected " << std::thread::hardware_concurrency() << " available threads!\n";
	TimeTracker timer;
	int dedekindOrder = 6;
	DedekindDecomposition<IntervalSize> fullDecomposition(dedekindOrder);
	
	//std::cout << "Decomposition:\n" << fullDecomposition << "\n";

	std::cout << "Dedekind " << dedekindOrder << " = " << fullDecomposition.getDedekind() << std::endl;

	return 0;
}
