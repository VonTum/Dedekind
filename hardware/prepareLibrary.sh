#!/bin/bash

module load intelFPGA_pro nalla_pcie toolchain/gompi

aocl library hdl-comp-pkg pcoeffProcessor.xml -o pcoeffProcessor.aoco
aocl library create -name pcoeffProcessor pcoeffProcessor.aoco
