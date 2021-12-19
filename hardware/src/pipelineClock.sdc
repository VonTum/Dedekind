
# Basic pipeline clock
create_clock -name pipelineClk -period 1ns [get_ports clk]
create_clock -name pipelineClk -period 1ns [get_ports clock]

# The bus clock, running at a far slower rate such that long lines are useable
# create_generated_clock -name busClk -source [get_ports clk] -master_clock pipelineClk -divide_by 4 [get_ports busClk]

# The top bus is a slow bus, does not change between runs
set_false_path -from [get_ports "top*"]

# create_clock -name ioClk -period 20ns [get_ports ioClk]
# set_false_path -from [get_clocks pipelineClk] -to [get_clocks ioClk]
# set_false_path -from [get_clocks ioClk] -to [get_clocks pipelineClk]

# These are for benchmarking pipelinedCountConnectedCore
set_false_path -to [get_ports "request"]
set_false_path -to [get_ports "rst"]
set_false_path -from [get_ports "graphIn*"]
set_false_path -from [get_ports "graphInAvailable"]
set_false_path -from [get_ports "connectCountIn*"]
set_false_path -from [get_ports "extraDataIn*"]
set_false_path -to [get_ports "done"]
set_false_path -to [get_ports "connectCount*"]
set_false_path -to [get_ports "extraDataOut*"]

