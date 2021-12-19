./prepareLibrary.sh

aoc -rtl -v -board=p520_max_sg280l -board-package=/cm/shared/opt/intelFPGA_pro/20.4.0/hld/board/bittware_pcie/s10 fullPipelineKernel.cl -I src -L . -l fullPipeline.aoclib -o fullPipelineKernel_report
