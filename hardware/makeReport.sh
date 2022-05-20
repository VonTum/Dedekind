./prepareLibrary.sh

aoc -rtl -v -board=p520_max_sg280l -board-package=/cm/shared/opt/intelFPGA_pro/20.4.0/hld/board/bittware_pcie/s10 -force-single-store-ring -no-interleaving=default dedekindAccelerator.cl -I src -L . -l pcoeffProcessor.aoclib -o dedekindAccelerator_report
