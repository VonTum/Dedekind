
# Basic pipeline clock
create_clock -name DebugClock -period 250MHz [get_ports clk]
create_generated_clock -name DebugClock2x -multiply_by 2 -master_clock DebugClock -source [get_ports clk] [get_ports clk2x]

create_clock -name kernelClock -period 300MHz [get_ports clock]
#create_clock -name fastClock -period 600MHz [get_ports clock2x]
create_generated_clock -name fastClock -multiply_by 2 -master_clock kernelClock -source [get_ports clock] [get_ports clock2x]

create_clock -name dualClockFIFOrdclk -period 1.0ns [get_ports rdclk]
create_clock -name dualClockFIFOwrclk -period 1.0ns [get_ports wrclk]

# The bus clock, running at a far slower rate such that long lines are useable
# create_generated_clock -name busClk -source [get_ports clk] -master_clock pipelineClk -divide_by 4 [get_ports busClk]

# The top bus is a slow bus, does not change between runs
#Commented out, cannot use with OpenCL
#set_false_path -from [get_ports "top*"]

# create_clock -name ioClk -period 20ns [get_ports ioClk]
# set_false_path -from [get_clocks pipelineClk] -to [get_clocks ioClk]
# set_false_path -from [get_clocks ioClk] -to [get_clocks pipelineClk]

# These are for benchmarking pipelinedCountConnectedCore
#set_false_path -to [get_ports "request"]
#set_false_path -to [get_ports "rst"]
#set_false_path -from [get_ports "graphIn*"]
#set_false_path -from [get_ports "graphInAvailable"]
#set_false_path -from [get_ports "connectCountIn*"]
#set_false_path -from [get_ports "extraDataIn*"]
#set_false_path -to [get_ports "done"]
#set_false_path -to [get_ports "connectCount*"]
#set_false_path -to [get_ports "extraDataOut*"]


# set_false_path -from [get_keepers *|topMngr|topReg*] -to *
