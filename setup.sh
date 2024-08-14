mkdir build_release
cmake -DCMAKE_BUILD_TYPE=Release -S . -B build

mkdir build_debug
cmake -DCMAKE_BUILD_TYPE=Debug -S . -B build_debug
