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
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -march=native")
  
  # profiling
  # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fno-omit-frame-pointer -fno-inline-small-functions")
  # add_definitions(-DNO_MULTITHREAD)

  # set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -lasan -fsanitize=address")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -Wno-unused-variable")
endif()

add_library(dedelib
  dedelib/aligned_alloc.cpp
  dedelib/codeGen.cpp
  dedelib/equivalenceClass.cpp
  dedelib/intervalDecomposition.cpp
  dedelib/layerDecomposition.cpp
  dedelib/valuedDecomposition.cpp
  dedelib/fileNames.cpp
  dedelib/terminalColor.cpp

  dedelib/bigint/uint128_t.cpp
  dedelib/bigint/uint256_t.cpp
)

add_executable(production
  production/dedekind.cpp
)
add_executable(indev
  indev/indev.cpp
)
add_executable(tests
  tests/testsMain.cpp
  tests/bigintTests.cpp
  tests/indent.cpp
  tests/intervalTests.cpp
  tests/tjomnTests.cpp
)
add_executable(benchmarks
  benchmarks/benchmark.cpp
  benchmarks/connectBenchmarks.cpp
)

target_link_libraries(benchmarks dedelib)
target_link_libraries(tests dedelib)
target_link_libraries(production dedelib)
target_link_libraries(indev dedelib)
target_link_libraries(dedelib pthread)

