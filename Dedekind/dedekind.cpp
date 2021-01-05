
#include <iostream>
#include "dedekindDecomposition.h"
#include "valuedDecomposition.h"
#include "toString.h"

#include "timeTracker.h"
#include "codeGen.h"

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

#ifndef RUN_TESTS
int main() {
	//genGraphVisCode(7);
	//return 0;

	std::cout << "Detected " << std::thread::hardware_concurrency() << " available threads!\n";
	TimeTracker timer;
	int dedekindOrder = 6;
	DedekindDecomposition<IntervalSize> fullDecomposition(dedekindOrder);
	
	//std::cout << "Decomposition:\n" << fullDecomposition << "\n";

	//std::cout << "Dedekind " << dedekindOrder << " = " << fullDecomposition.getDedekind() << std::endl;
	//return 0;
}
#endif
