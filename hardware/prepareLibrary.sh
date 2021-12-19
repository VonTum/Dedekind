#!/bin/bash

module load intelFPGA_pro nalla_pcie toolchain/gompi

aocl library hdl-comp-pkg fullPipeline.xml -o fullPipeline.aoco
aocl library create -name fullPipeline fullPipeline.aoco
