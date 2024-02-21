#!/bin/bash
#SBATCH -N 1
#SBATCH -J generateMBF9
#SBATCH -A pc2-mitarbeiter
##SBATCH --constraint=bittware_520n_20.4.0_hpc
##SBATCH --constraint=xilinx_u280_xrt2.12
#SBATCH -p normal
#SBATCH -t 7-00:00:00
#SBATCH --exclusive

module load devel/CMake/3.21.1-GCCcore-11.2.0 fpga bittware/520n intel/opencl_sdk

## ./production parallelizeMBF9GenerationAcrossAllCores:104857600
./production parallelizeMBF9GenerationAcrossAllCores:262144

