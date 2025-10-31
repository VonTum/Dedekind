#!/bin/bash
#SBATCH -N 1
#SBATCH -J buildrtl
#SBATCH -A hpc-prf-dedek
#SBATCH -p normal
#SBATCH -t 7-00:00:00
#SBATCH --exclusive
#SBATCH --mail-type all
#SBATCH --mail-user lennart.vanhirtum@gmail.com

source ../fpgaModules.sh

./prepareLibrary.sh

# -num-reorder=3
# -high-effort
# -seed=<value>
# -ecc DOES NOT WORK

aoc -seed=10 -high-effort -force-single-store-ring -no-interleaving=default dedekindAccelerator.cl -o dedekindAccelerator.aocx -L . -l pcoeffProcessor.aoclib

cp dedekindAccelerator.aocx ../build

