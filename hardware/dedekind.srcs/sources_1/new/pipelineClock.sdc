
# Basic pipeline clock
create_clock -name pipelineClk -period 2ns [get_ports clk]

# The bus clock, running at a far slower rate such that long lines are useable
# create_generated_clock -name busClk -source [get_ports clk] -master_clock pipelineClk -divide_by 4 [get_ports busClk]

# The top bus is a slow bus, does not change between runs
set_false_path -from [get_ports "top*"]
