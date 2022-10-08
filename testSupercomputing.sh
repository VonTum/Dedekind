rm -r ./supercomputeTest

./production initializeSupercomputingProject:./supercomputeTest,8,5 && 

./production initializeValidationFiles:./supercomputeTest,8,cpuSMT_n2login1,cpuSMT_n2login2,cpuSMT_n2login3,cpuSMT_n2login4,cpuSMT_n2login5,cpuSMT_n2login6 && 

./production processJobCPU6_SMT:./supercomputeTest,0,v && 
./production processJobCPU6_SMT:./supercomputeTest,1,v && 
./production processJobCPU6_SMT:./supercomputeTest,2,v && 
./production processJobCPU6_SMT:./supercomputeTest,3,v && 
./production processJobCPU6_SMT:./supercomputeTest,4,v && 

./production collectAllSupercomputingProjectResults_D8:./supercomputeTest

