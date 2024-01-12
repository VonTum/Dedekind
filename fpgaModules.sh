# module load cray
# newgrp dialout

alias powerMeasure="/usr/share/nallatech/520n/bist/utilities/nalla_serial_cardmon/bin/nalla_serial_cardmon -c "

module load toolchain/gompi/2021b devel/CMake/3.21.1-GCCcore-11.2.0 fpga bittware/520n intel/opencl_sdk toolchain/intel/2021b tools/amduprof/4.0-341

export AOCL_BOARD_PACKAGE_ROOT=/upb/departments/pc2/users/d/dedek001/scratchDedek/customBoardPackage/s10_hpc_default
# export LD_LIBRARY_PATH=/opt/software/FPGA/patches/bittware_reliable_transfers/20.4.0:$LD_LIBRARY_PATH

