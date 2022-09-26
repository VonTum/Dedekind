mkdir build_numa
cmake -DCMAKE_BUILD_TYPE=Release -D USE_NUMA=true -S . -B build_numa

mkdir build_debug_numa
cmake -DCMAKE_BUILD_TYPE=Debug -D USE_NUMA=true -S . -B build_debug_numa
