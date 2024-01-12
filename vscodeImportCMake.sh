module load devel/CMake/3.21.1-GCCcore-11.2.0

cmake -D USE_NUMA=true "$@"
