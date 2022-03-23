#!/bin/bash
#SBATCH -N 1
#SBATCH -J buildrtl
#SBATCH -A hpc-prf-dedek
#SBATCH -p batch
#SBATCH -t 5:00:00
#SBATCH --mem=100000MB
#SBATCH --mail-type all
#SBATCH --mail-user lennart.vanhirtum@gmail.com

module load intelFPGA_pro nalla_pcie toolchain/gompi

./prepareLibrary.sh

aoc fullPipelineKernel.cl -o fullPipelineKernel.aocx -I scr -L . -l fullPipeline.aoclib

cp fullPipelineKernel.aocx ../build

