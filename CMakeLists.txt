cmake_minimum_required(VERSION 3.10)

project(Dedekind VERSION 1.0)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Building with: ${CMAKE_CXX_COMPILER_ID}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:AVX2")
	
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /Oi /ot /GL")
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
	
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -lasan -fsanitize=address")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
endif()

add_library(dedelib
  Dedekind/aligned_alloc.cpp
  Dedekind/codeGen.cpp
  Dedekind/equivalenceClass.cpp
  Dedekind/intervalDecomposition.cpp
  Dedekind/layerDecomposition.cpp
  Dedekind/valuedDecomposition.cpp

  Dedekind/bigint/uint128_t.cpp
  Dedekind/bigint/uint256_t.cpp
)

add_executable(dedekind
  Dedekind/dedekind.cpp
)
add_executable(tests
  Dedekind/tests/testsMain.cpp
  Dedekind/tests/bigintTests.cpp
  Dedekind/tests/indent.cpp
  Dedekind/tests/intervalTests.cpp
  Dedekind/tests/terminalColor.cpp
  Dedekind/tests/tjomnTests.cpp
)
target_compile_definitions(tests PUBLIC RUN_TESTS)

target_link_libraries(dedekind dedelib)
target_link_libraries(tests dedelib)
target_link_libraries(dedekind pthread)
