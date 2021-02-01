mkdir build
mkdir build_debug

cmake -DCMAKE_BUILD_TYPE=Release -S . -B build
cmake -DCMAKE_BUILD_TYPE=Debug -S . -B build_debug
