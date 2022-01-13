cmake_minimum_required(VERSION 3.10)

project(Dedekind VERSION 1.0)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Building with: ${CMAKE_CXX_COMPILER_ID}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  add_compile_options("/arch:AVX2")

  add_compile_options("$<$<CONFIG:RELEASE>: /O2 /Oi /ot /GL>")
else()
  add_compile_options("-Wall -march=native")
  
  # profiling
  # add_compile_options("-g -fno-omit-frame-pointer -fno-inline-small-functions")
  # add_definitions(-DNO_MULTITHREAD)

  # add_compile_options("$<$<CONFIG:DEBUG>: -g -lasan -fsanitize=address>")
  add_compile_options("$<$<CONFIG:RELEASE>: -O3 -Wno-unused-variable>")
endif()

add_library(dedelib
  dedelib/aligned_alloc.cpp
  dedelib/codeGen.cpp
  dedelib/equivalenceClass.cpp
  dedelib/intervalDecomposition.cpp
  dedelib/layerDecomposition.cpp
  dedelib/valuedDecomposition.cpp
  dedelib/fileNames.cpp
  dedelib/flatBufferManagement.cpp
  dedelib/configure.cpp
  dedelib/terminalColor.cpp

  dedelib/bigint/uint128_t.cpp
  dedelib/bigint/uint256_t.cpp
)

add_executable(production
  production/main.cpp
  production/miscCommands.cpp
  production/flatCommands.cpp
  production/dataGenerationCommands.cpp
  production/testSetCommands.cpp
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

# FPGA acceleration is only verified to work for G++
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
add_executable(fpga_production
  fpga_production/main.cpp
  fpga_production/AOCLUtils/opencl.cpp
  fpga_production/AOCLUtils/options.cpp
)

# Compiling with defences enabled
target_compile_definitions(fpga_production PRIVATE "-D_FORTIFY_SOURCE=2")
target_compile_options(fpga_production PRIVATE "-fstack-protector -D_FORTIFY_SOURCE=2 -Wformat -Wformat-security -fPIE")

# We must force GCC to never assume that it can shove in its own
# sse2/sse3 versions of strlen and strcmp because they will CRASH.
# Very hard to debug!
target_compile_options(fpga_production PRIVATE "-fPIC")

# OpenCL compile and link flags.
target_compile_options(fpga_production PRIVATE "$(shell aocl compile-config )")
# Add linker flags
target_link_options(fpga_production PRIVATE "$(shell aocl ldflags )")
# Defensive linker flags
target_link_options(fpga_production PRIVATE "-z noexecstack -Wl,-z,relro,-z,now -Wl,-Bsymbolic -pie $(shell aocl ldlibs )")
# Add linker include directory
target_link_options(fpga_production PRIVATE "$(shell aocl ldlibs )")

# Link to known libraries
target_link_options(fpga_production PRIVATE "-Lrt -Lpthread")

target_include_directories(fpga_production PRIVATE fpga_production)

target_link_libraries(fpga_production dedelib)
endif()

target_link_libraries(benchmarks dedelib)
target_link_libraries(tests dedelib)
target_link_libraries(production dedelib)
target_link_libraries(indev dedelib)
target_link_libraries(dedelib pthread)

