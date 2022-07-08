#!/bin/bash
#SBATCH -N 1
#SBATCH -J buildrtl
#SBATCH -A hpc-prf-dedek
#SBATCH -p batch
#SBATCH -t 10:00:00
#SBATCH --mem=60000MB
#SBATCH --mail-type all
#SBATCH --mail-user lennart.vanhirtum@gmail.com

source ../fpgaModules.sh

./prepareLibrary.sh

aoc -march=simulator dedekindAccelerator.cl -o dedekindAccelerator.aocx -I scr -L . -l pcoeffProcessor.aoclib

