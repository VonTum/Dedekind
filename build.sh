g++ -O3 -march=skylake-avx512 -o dedekind \
Dedekind/dedekind.cpp \
Dedekind/aligned_alloc.cpp \
Dedekind/codeGen.cpp \
Dedekind/equivalenceClass.cpp \
Dedekind/intervalDecomposition.cpp \
Dedekind/layerDecomposition.cpp \
Dedekind/valuedDecomposition.cpp \
Dedekind/bigint/uint128_t.cpp \
Dedekind/bigint/uint256_t.cpp

g++ -O3 -march=skylake-avx512 -o -D RUN_TESTS tests \
Dedekind/dedekind.cpp \
Dedekind/aligned_alloc.cpp \
Dedekind/codeGen.cpp \
Dedekind/equivalenceClass.cpp \
Dedekind/intervalDecomposition.cpp \
Dedekind/layerDecomposition.cpp \
Dedekind/valuedDecomposition.cpp \
Dedekind/bigint/uint128_t.cpp \
Dedekind/bigint/uint256_t.cpp \
Dedekind/tests/testsMain.cpp \
Dedekind/tests/bigintTests.cpp \
Dedekind/tests/indent.cpp \
Dedekind/tests/intervalTests.cpp \
Dedekind/tests/terminalColor.cpp \
Dedekind/tests/tjomnTests.cpp
