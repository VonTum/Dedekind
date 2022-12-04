rm -r ./supercomputeTest

./production initializeSupercomputingProject:./supercomputeTest,7,5 &&

# ./production initializeValidationFiles:./supercomputeTest,7,cpuSMT_n2login1,cpuSMT_n2login2,cpuSMT_n2login3,cpuSMT_n2login4,cpuSMT_n2login5,cpuSMT_n2login6 &&

./production processJobCPU5_SMT:./supercomputeTest,0,validate_adv &&
./production processJobCPU5_SMT:./supercomputeTest,1,validate_adv &&
./production processJobCPU5_SMT:./supercomputeTest,2,validate_adv &&
./production processJobCPU5_SMT:./supercomputeTest,3,validate_adv &&
./production processJobCPU5_SMT:./supercomputeTest,4,validate_adv &&

./production collectAllSupercomputingProjectResults:7,./supercomputeTest

