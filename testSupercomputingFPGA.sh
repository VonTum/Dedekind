source ../fpgaModules.sh

rm -r ./supercomputeTest

./production initializeSupercomputingProject:./supercomputeTest,9,1000000,100 &&

# ./production initializeValidationFiles:./supercomputeTest,9,cpuSMT_n2login1,cpuSMT_n2login2,cpuSMT_n2login3,cpuSMT_n2login4,cpuSMT_n2login5,cpuSMT_n2login6 &&

./fpga_production ./supercomputeTest 0 &&
./fpga_production ./supercomputeTest 1 &&

# ./production collectAllSupercomputingProjectResults:8,./supercomputeTest

./production checkProjectIntegrity:9,./supercomputeTest
