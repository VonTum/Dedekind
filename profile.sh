
cmake --build . --parallel --target production &&
taskset -c 0 perf record -g ./production $1 &&
hotspot ./perf.data
