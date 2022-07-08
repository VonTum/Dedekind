cmake_minimum_required(VERSION 3.16.3)

project(Dedekind VERSION 1.0)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Building with: ${CMAKE_CXX_COMPILER_ID}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  add_compile_options(/arch:AVX2)

  add_compile_options("$<$<CONFIG:RELEASE>:/O2 /Oi /ot /GL>")
else()
  add_compile_options(-Wall -Wextra -march=native)
  
  # profiling
  # add_compile_options(-g -fno-omit-frame-pointer -fno-inline-small-functions)
  # add_definitions(-DNO_MULTITHREAD)

  # add_compile_options("$<$<CONFIG:DEBUG>:-g -lasan -fsanitize=address>")
  add_compile_options("$<$<CONFIG:RELEASE>:-Wno-unused-variable>")
  add_compile_options("$<$<CONFIG:RELEASE>:-O3>")
endif()

add_library(dedelib
  dedelib/aligned_alloc.cpp
  dedelib/equivalenceClass.cpp
  dedelib/intervalDecomposition.cpp
  dedelib/layerDecomposition.cpp
  dedelib/valuedDecomposition.cpp
  dedelib/fileNames.cpp
  dedelib/flatBufferManagement.cpp
  dedelib/configure.cpp
  dedelib/terminalColor.cpp
  dedelib/flatPCoeffProcessing.cpp
  dedelib/supercomputerJobs.cpp
  dedelib/bottomBufferCreator.cpp
  dedelib/threadUtils.cpp
  dedelib/pcoeffClasses.cpp
  dedelib/resultCollection.cpp

  dedelib/bigint/uint128_t.cpp
  dedelib/bigint/uint256_t.cpp
)

# Must also enable Position Independent Code, or fpga_production gets angry
target_compile_options(dedelib PRIVATE -fPIC)

add_executable(production
  production/main.cpp
  production/miscCommands.cpp
  production/flatCommands.cpp
  production/processingCommands.cpp
  production/dataGenerationCommands.cpp
  production/testSetCommands.cpp
  production/codeGenCommands.cpp
  production/bottomBufferCommands.cpp
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
  # fpga_production/FullPermutePipelineTestSetOpenCL_generated7.cpp
  # fpga_production/pipeline6PackTestSetForOpenCL_generated7.cpp
  fpga_production/AOCLUtils/opencl.cpp
  fpga_production/AOCLUtils/options.cpp
)

# Compiling with defences enabled
target_compile_definitions(fpga_production PRIVATE "-D_FORTIFY_SOURCE=2")
target_compile_options(fpga_production PRIVATE -fstack-protector -D_FORTIFY_SOURCE=2 -Wformat -Wformat-security -fPIE)

# We must force GCC to never assume that it can shove in its own
# sse2/sse3 versions of strlen and strcmp because they will CRASH.
# Very hard to debug!
target_compile_options(fpga_production PRIVATE "-fPIC")

# /cm/shared/opt/intelFPGA_pro/21.4.0/hld/host/include
execute_process(COMMAND aocl compile-config OUTPUT_VARIABLE AOCL_COMPILE_CONFIG)

# -Wl,--no-as-needed -lalteracl -lbitt_s10_pcie_mmd -lelf
execute_process(COMMAND aocl ldlibs OUTPUT_VARIABLE AOCL_LINK_LIBS)

# -L/cm/shared/opt/intelFPGA_pro/20.4.0/hld/board/bittware_pcie/s10/linux64/lib -L/cm/shared/opt/intelFPGA_pro/21.4.0/hld/host/linux64/lib
execute_process(COMMAND aocl ldflags OUTPUT_VARIABLE AOCL_LINK_FLAGS)

string(REGEX REPLACE " \n$" "" AOCL_COMPILE_CONFIG "${AOCL_COMPILE_CONFIG}")
string(REGEX REPLACE "^-I" "" AOCL_INCLUDE_DIRECTORIES "${AOCL_COMPILE_CONFIG}") 
string(REGEX REPLACE "\n$" "" AOCL_LINK_LIBS "${AOCL_LINK_LIBS}")
string(REGEX REPLACE "\n$" "" AOCL_LINK_FLAGS "${AOCL_LINK_FLAGS}")

# Messages, these are the values these commands *should* have. If these change then manual edits are required
message(STATUS "AOCL_INCLUDE_DIRECTORIES: ${AOCL_INCLUDE_DIRECTORIES}")   # /cm/shared/opt/intelFPGA_pro/21.4.0/hld/host/include
message(STATUS "AOCL_LINK_LIBS: ${AOCL_LINK_LIBS}")                       # -Wl,--no-as-needed -lalteracl -lbitt_s10_pcie_mmd -lelf
message(STATUS "AOCL_LINK_FLAGS: ${AOCL_LINK_FLAGS}")                     # -L/cm/shared/opt/intelFPGA_pro/20.4.0/hld/board/bittware_pcie/s10/linux64/lib -L/cm/shared/opt/intelFPGA_pro/21.4.0/hld/host/linux64/lib

# OpenCL compile and link flags.
target_include_directories(fpga_production PRIVATE ${AOCL_INCLUDE_DIRECTORIES})
# Add linker flags

target_link_options(fpga_production PRIVATE ${AOCL_LINK_FLAGS})
target_link_directories(fpga_production PRIVATE /opt/software/FPGA/IntelFPGA/opencl_sdk/21.4.0/hld/host/linux64/lib)

# Defensive linker flags
target_link_options(fpga_production PRIVATE -z noexecstack -Wl,-z,relro,-z,now -Wl,-Bsymbolic -pie)
# Add linker include directory
# target_link_options(fpga_production PRIVATE ${AOCL_LINK_LIBS})
# Instead of that, which seems to be kaput, we'll have to link manually
target_link_options(fpga_production PRIVATE -Wl,--no-as-needed)
# target_link_libraries(fpga_production alteracl)
# target_link_libraries(fpga_production bitt_s10_pcie_mmd)
target_link_libraries(fpga_production OpenCL)
target_link_libraries(fpga_production elf)


target_include_directories(fpga_production PRIVATE fpga_production)

target_link_libraries(fpga_production dedelib)

# Link to known libraries
target_link_libraries(fpga_production rt)
target_link_libraries(fpga_production pthread)
endif()

target_link_libraries(benchmarks dedelib)
target_link_libraries(tests dedelib)
target_link_libraries(production dedelib)
target_link_libraries(indev dedelib)
target_link_libraries(dedelib pthread)

