rm -r ./supercomputeTest

./production initializeSupercomputingProject:./supercomputeTest,7,5

./production processJobCPU5_FMT:./supercomputeTest,0
./production processJobCPU5_ST:./supercomputeTest,1
./production processJobCPU5_FMT:./supercomputeTest,2
./production processJobCPU5_ST:./supercomputeTest,3
./production processJobCPU5_FMT:./supercomputeTest,4

./production collectAllSupercomputingProjectResults_D7:./supercomputeTest

