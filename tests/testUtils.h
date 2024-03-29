#pragma once

#include <ostream>

#include "testsMain.h"
#include "../dedelib/toString.h"
#include "../dedelib/functionInput.h"
#include "../dedelib/booleanFunction.h"
#include "../dedelib/interval.h"

#define TEST_FROM 3
#define TEST_UPTO 9
#define SMALL_ITER 50
#define LARGE_ITER 1000

template<unsigned int Variables>
void prettyFibs(std::ostream& os, const BooleanFunction<Variables>& ac) {
	bool isFirst = true;
	ac.forEachOne([&](size_t i) {
		FunctionInput fi{i};

		os << (isFirst ? "{" : ", ");
		os << fi;
		isFirst = false;
	});
	if(isFirst) os << '{';
	os << "}";
}
template<unsigned int Variables>
void prettyInterval(std::ostream& os, const Interval<Variables>& i) {
	prettyFibs(os, i.bot.bf);
	os << " - ";
	prettyFibs(os, i.top.bf);
}


#define printAC(AC) do { std::cout << #AC << " : "; prettyFibs(std::cout, (AC).asAntiChain()); std::cout << "\n"; } while(false)

template<unsigned int Start, unsigned int End, template<unsigned int> typename FuncClass>
void runFunctionRange() {
	if constexpr(Start <= End) {
		logStream << Start << "/" << End << "\n";
		FuncClass<Start>::run();
		runFunctionRange<Start + 1, End, FuncClass>();
	}
}
