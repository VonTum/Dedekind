`timescale 1ns / 1ps

`include "ipSettings_header.v"

module MEMORY_MLAB #(
    parameter WIDTH = 20,
    parameter DEPTH_LOG2 = 5,
    parameter READ_DURING_WRITE = "DONT_CARE", // Options are "DONT_CARE", "OLD_DATA" and "NEW_DATA",
    parameter OUTPUT_REGISTER = 0
) (
    input clk,
    input rstReadAddr,
    
    // Write Side
    input writeEnable,
    input[DEPTH_LOG2-1:0] writeAddr,
    input[WIDTH-1:0] dataIn,
    
    // Read Side
    input readAddressStall,
    input[DEPTH_LOG2-1:0] readAddr,
    output[WIDTH-1:0] dataOut
);

`ifdef USE_FIFO_MEMORY_IP

genvar START_INDEX;
generate for(START_INDEX = 0; START_INDEX < WIDTH; START_INDEX = START_INDEX + 20) begin
// Stupid hack to get a constant value intermediate
`define BLOCK_WIDTH (WIDTH - START_INDEX >= 20 ? 20 : WIDTH - START_INDEX)

altera_syncram  altera_syncram_component (
    .clock0 (clk),
    .aclr0 (rstReadAddr),
    .address_a (writeAddr),
    .address_b (readAddr),
    .addressstall_b (readAddressStall),
    .data_a (dataIn[START_INDEX +: `BLOCK_WIDTH]),
    .wren_a (writeEnable),
    .q_b (dataOut[START_INDEX +: `BLOCK_WIDTH]),
    .aclr1 (1'b0),
    .address2_a (1'b1),
    .address2_b (1'b1),
    .addressstall_a (1'b0),
    .byteena_a (1'b1),
    .byteena_b (1'b1),
    .clock1 (1'b1),
    .clocken0 (1'b1),
    .clocken1 (1'b1),
    .clocken2 (1'b1),
    .clocken3 (1'b1),
    .data_b ({`BLOCK_WIDTH{1'b1}}),
    .eccencbypass (1'b0),
    .eccencparity (8'b0),
    .eccstatus (),
    .q_a (),
    .rden_a (1'b1),
    .rden_b (1'b1),
    .sclr (1'b0),
    .wren_b (1'b0));
defparam
    altera_syncram_component.address_aclr_b  = "CLEAR0",
    altera_syncram_component.address_reg_b  = "CLOCK0",
    altera_syncram_component.outdata_reg_b  = OUTPUT_REGISTER ? "CLOCK0" : "UNREGISTERED",
    altera_syncram_component.clock_enable_input_a  = "BYPASS",
    altera_syncram_component.clock_enable_input_b  = "BYPASS",
    altera_syncram_component.clock_enable_output_b  = "BYPASS",
    altera_syncram_component.intended_device_family  = "Stratix 10",
    altera_syncram_component.lpm_type  = "altera_syncram",
    altera_syncram_component.numwords_a  = (1 << DEPTH_LOG2),
    altera_syncram_component.numwords_b  = (1 << DEPTH_LOG2),
    altera_syncram_component.operation_mode  = "DUAL_PORT",
    altera_syncram_component.outdata_aclr_b  = "NONE",
    altera_syncram_component.outdata_sclr_b  = "NONE",
    altera_syncram_component.power_up_uninitialized  = "FALSE",
    altera_syncram_component.ram_block_type  = "MLAB",
    altera_syncram_component.read_during_write_mode_mixed_ports  = READ_DURING_WRITE,
    altera_syncram_component.widthad_a  = DEPTH_LOG2,
    altera_syncram_component.widthad_b  = DEPTH_LOG2,
    altera_syncram_component.width_a  = `BLOCK_WIDTH,
    altera_syncram_component.width_b  = `BLOCK_WIDTH,
    altera_syncram_component.width_byteena_a  = 1,
    altera_syncram_component.enable_force_to_zero  = "FALSE",
    altera_syncram_component.enable_ecc  = "FALSE";
end endgenerate
`else

reg[WIDTH-1:0] memory[(1 << DEPTH_LOG2) - 1:0];

reg[DEPTH_LOG2-1:0] readAddrReg;
reg[DEPTH_LOG2-1:0] writeAddrReg;
reg[WIDTH-1:0] writeDataReg;
reg writeEnableReg;

always @(posedge clk) begin
    if(writeEnableReg) begin
        memory[writeAddrReg] <= writeDataReg;
    end
    
    writeAddrReg <= writeAddr;
    writeDataReg <= dataIn;
    writeEnableReg <= writeEnable;
end

always @(posedge clk or posedge rstReadAddr) begin // Asynchronous to correctly emulate intel's MLAB reset
    if(rstReadAddr) readAddrReg <= 0;
    else if(!readAddressStall) readAddrReg <= readAddr;
end

wire[WIDTH-1:0] dataFromMem = (writeEnableReg && READ_DURING_WRITE == "DONT_CARE" && writeAddrReg == readAddrReg) ? {WIDTH{1'bX}} : memory[readAddrReg];
hyperpipe #(.CYCLES(OUTPUT_REGISTER), .WIDTH(WIDTH)) dataOutPipe(clk, dataFromMem, dataOut);

`endif

endmodule


module MEMORY_M20K #(
    parameter WIDTH = 20,
    parameter DEPTH_LOG2 = 5,
    parameter READ_DURING_WRITE = "DONT_CARE" // Options are "DONT_CARE", "OLD_DATA" and "NEW_DATA"
) (
    input clk,
    
    // Write Side
    input writeEnable,
    input[DEPTH_LOG2-1:0] writeAddr,
    input[WIDTH-1:0] dataIn,
    
    // Read Side
    input readAddressStall,
    input[DEPTH_LOG2-1:0] readAddr,
    output[WIDTH-1:0] dataOut,
    output eccStatus
);

`ifdef USE_FIFO_MEMORY_IP

wire[1:0] eccStatusWire;
assign eccStatus = eccStatusWire[1];

altera_syncram  altera_syncram_component (
    .clock0 (clk),
    .address_a (writeAddr),
    .address_b (readAddr),
    .addressstall_b (readAddressStall),
    .data_a (dataIn),
    .wren_a (writeEnable),
    .q_b (dataOut),
    .aclr0 (1'b0),
    .aclr1 (1'b0),
    .address2_a (1'b1),
    .address2_b (1'b1),
    .addressstall_a (1'b0),
    .byteena_a (1'b1),
    .byteena_b (1'b1),
    .clock1 (1'b1),
    .clocken0 (1'b1),
    .clocken1 (1'b1),
    .clocken2 (1'b1),
    .clocken3 (1'b1),
    .data_b ({WIDTH{1'b1}}),
    .eccencbypass (1'b0),
    .eccencparity (8'b0),
    .eccstatus (eccStatusWire),
    .q_a (),
    .rden_a (1'b1),
    .rden_b (1'b1),
    .sclr (1'b0),
    .wren_b (1'b0));
defparam
    altera_syncram_component.address_aclr_b  = "NONE",
    altera_syncram_component.address_reg_b  = "CLOCK0",
    altera_syncram_component.outdata_reg_b  = "CLOCK0",
    altera_syncram_component.clock_enable_input_a  = "BYPASS",
    altera_syncram_component.clock_enable_input_b  = "BYPASS",
    altera_syncram_component.clock_enable_output_b  = "BYPASS",
    altera_syncram_component.intended_device_family  = "Stratix 10",
    altera_syncram_component.lpm_type  = "altera_syncram",
    altera_syncram_component.numwords_a  = (1 << DEPTH_LOG2),
    altera_syncram_component.numwords_b  = (1 << DEPTH_LOG2),
    altera_syncram_component.operation_mode  = "DUAL_PORT",
    altera_syncram_component.outdata_aclr_b  = "NONE",
    altera_syncram_component.outdata_sclr_b  = "NONE",
    altera_syncram_component.power_up_uninitialized  = "FALSE",
    altera_syncram_component.ram_block_type  = "M20K",
    altera_syncram_component.read_during_write_mode_mixed_ports  = READ_DURING_WRITE,
    altera_syncram_component.widthad_a  = DEPTH_LOG2,
    altera_syncram_component.widthad_b  = DEPTH_LOG2,
    altera_syncram_component.width_a  = WIDTH,
    altera_syncram_component.width_b  = WIDTH,
    altera_syncram_component.width_byteena_a  = 1,
    altera_syncram_component.enable_force_to_zero  = "TRUE",
    altera_syncram_component.enable_ecc  = "TRUE",
    altera_syncram_component.ecc_pipeline_stage_enabled  = "TRUE",
    altera_syncram_component.enable_ecc_encoder_bypass  = "FALSE",
    altera_syncram_component.width_eccstatus  = 2;

`else

reg[WIDTH-1:0] memory[(1 << DEPTH_LOG2) - 1:0];

reg[DEPTH_LOG2-1:0] writeAddrReg;
reg[WIDTH-1:0] writeDataReg;
reg writeEnableReg;

reg[DEPTH_LOG2-1:0] readAddrReg;

always @(posedge clk) begin
    if(!readAddressStall) readAddrReg <= readAddr;
    
    if(writeEnableReg) begin
        memory[writeAddrReg] <= writeDataReg;
    end
    
    writeAddrReg <= writeAddr;
    writeDataReg <= dataIn;
    writeEnableReg <= writeEnable;
end

wire[WIDTH-1:0] dataFromMem = (writeEnableReg && READ_DURING_WRITE == "DONT_CARE" && writeAddrReg == readAddrReg) ? {WIDTH{1'bX}} : memory[readAddrReg];
hyperpipe #(.CYCLES(2), .WIDTH(WIDTH)) dataOutPipe(clk, dataFromMem, dataOut);

assign eccStatus = 1'b0;

`endif

endmodule




module DUAL_CLOCK_MEMORY_MLAB #(
    parameter WIDTH = 20,
    parameter DEPTH_LOG2 = 5
) (
    // Write Side
    input wrclk,
    input writeEnable,
    input[DEPTH_LOG2-1:0] writeAddr,
    input[WIDTH-1:0] dataIn,
    
    // Read Side
    input rdclk,
    input rstReadAddr,
    input readAddressStall,
    input[DEPTH_LOG2-1:0] readAddr,
    output[WIDTH-1:0] dataOut
);

`ifdef USE_FIFO_MEMORY_IP

genvar START_INDEX;
generate for(START_INDEX = 0; START_INDEX < WIDTH; START_INDEX = START_INDEX + 20) begin
// Reuse previous define
//BLOCK_WIDTH = WIDTH - START_INDEX >= 20 ? 20 : WIDTH - START_INDEX;

altera_syncram  altera_syncram_component (
    .clock0 (wrclk),
    .clock1 (rdclk),
    .aclr1 (rstReadAddr),
    .address_a (writeAddr),
    .address_b (readAddr),
    .data_a (dataIn[START_INDEX +: `BLOCK_WIDTH]),
    .wren_a (writeEnable),
    .q_b (dataOut[START_INDEX +: `BLOCK_WIDTH]),
    .aclr0 (1'b0),
    .address2_a (1'b1),
    .address2_b (1'b1),
    .addressstall_a (1'b0),
    .addressstall_b (readAddressStall),
    .byteena_a (1'b1),
    .byteena_b (1'b1),
    .clocken0 (1'b1),
    .clocken1 (1'b1),
    .clocken2 (1'b1),
    .clocken3 (1'b1),
    .data_b ({`BLOCK_WIDTH{1'b1}}),
    .eccencbypass (1'b0),
    .eccencparity (8'b0),
    .eccstatus (),
    .q_a (),
    .rden_a (1'b1),
    .rden_b (1'b1),
    .sclr (1'b0),
    .wren_b (1'b0));
defparam
    altera_syncram_component.address_aclr_b  = "CLEAR1",
    altera_syncram_component.address_reg_b  = "CLOCK1",
    altera_syncram_component.outdata_reg_b  = "CLOCK1",
    altera_syncram_component.clock_enable_input_a  = "BYPASS",
    altera_syncram_component.clock_enable_input_b  = "BYPASS",
    altera_syncram_component.clock_enable_output_b  = "BYPASS",
    altera_syncram_component.intended_device_family  = "Stratix 10",
    altera_syncram_component.lpm_type  = "altera_syncram",
    altera_syncram_component.numwords_a  = (1 << DEPTH_LOG2),
    altera_syncram_component.numwords_b  = (1 << DEPTH_LOG2),
    altera_syncram_component.operation_mode  = "DUAL_PORT",
    altera_syncram_component.outdata_aclr_b  = "NONE",
    altera_syncram_component.outdata_sclr_b  = "NONE",
    altera_syncram_component.power_up_uninitialized  = "FALSE",
    altera_syncram_component.ram_block_type  = "MLAB",
    altera_syncram_component.read_during_write_mode_mixed_ports  = "DONT_CARE",
    altera_syncram_component.widthad_a  = DEPTH_LOG2,
    altera_syncram_component.widthad_b  = DEPTH_LOG2,
    altera_syncram_component.width_a  = `BLOCK_WIDTH,
    altera_syncram_component.width_b  = `BLOCK_WIDTH,
    altera_syncram_component.width_byteena_a  = 1,
    altera_syncram_component.enable_force_to_zero  = "FALSE",
    altera_syncram_component.enable_ecc  = "FALSE";
end endgenerate
`else

reg[WIDTH-1:0] memory[(1 << DEPTH_LOG2) - 1:0];

reg[DEPTH_LOG2-1:0] readAddrReg;
reg[DEPTH_LOG2-1:0] writeAddrReg;
reg[WIDTH-1:0] writeDataReg;
reg writeEnableReg;

always @(posedge wrclk) begin
    if(writeEnableReg) begin
        memory[writeAddrReg] <= writeDataReg;
    end
    
    writeAddrReg <= writeAddr;
    writeDataReg <= dataIn;
    writeEnableReg <= writeEnable;
end

always @(posedge rdclk or posedge rstReadAddr) begin // Asynchronous to correctly emulate intel's MLAB reset
    if(rstReadAddr) readAddrReg <= 0;
    else if(!readAddressStall) readAddrReg <= readAddr;
end

wire[WIDTH-1:0] dataFromMem = memory[readAddrReg];
hyperpipe #(.CYCLES(1), .WIDTH(WIDTH)) dataOutPipe(rdclk, dataFromMem, dataOut);

`endif

endmodule



module DUAL_CLOCK_MEMORY_M20K #(
    parameter WIDTH = 20,
    parameter DEPTH_LOG2 = 5
) (
    // Write Side
    input wrclk,
    input writeEnable,
    input[DEPTH_LOG2-1:0] writeAddr,
    input[WIDTH-1:0] dataIn,
    
    // Read Side
    input rdclk,
    input readAddressStall,
    input[DEPTH_LOG2-1:0] readAddr,
    output[WIDTH-1:0] dataOut,
    output eccStatus
);

`ifdef USE_FIFO_MEMORY_IP

wire[1:0] eccStatusWire;
assign eccStatus = eccStatusWire[1];

altera_syncram  altera_syncram_component (
    .clock0 (wrclk),
    .clock1 (rdclk),
    .address_a (writeAddr),
    .address_b (readAddr),
    .data_a (dataIn),
    .wren_a (writeEnable),
    .q_b (dataOut),
    .aclr0 (1'b0),
    .aclr1 (1'b0),
    .address2_a (1'b1),
    .address2_b (1'b1),
    .addressstall_a (1'b0),
    .addressstall_b (readAddressStall),
    .byteena_a (1'b1),
    .byteena_b (1'b1),
    .clocken0 (1'b1),
    .clocken1 (1'b1),
    .clocken2 (1'b1),
    .clocken3 (1'b1),
    .data_b ({WIDTH{1'b1}}),
    .eccencbypass (1'b0),
    .eccencparity (8'b0),
    .eccstatus (eccStatusWire),
    .q_a (),
    .rden_a (1'b1),
    .rden_b (1'b1),
    .sclr (1'b0),
    .wren_b (1'b0));
defparam
    altera_syncram_component.address_aclr_b  = "NONE",
    altera_syncram_component.address_reg_b  = "CLOCK1",
    altera_syncram_component.outdata_reg_b  = "CLOCK1",
    altera_syncram_component.clock_enable_input_a  = "BYPASS",
    altera_syncram_component.clock_enable_input_b  = "BYPASS",
    altera_syncram_component.clock_enable_output_b  = "BYPASS",
    altera_syncram_component.intended_device_family  = "Stratix 10",
    altera_syncram_component.lpm_type  = "altera_syncram",
    altera_syncram_component.numwords_a  = (1 << DEPTH_LOG2),
    altera_syncram_component.numwords_b  = (1 << DEPTH_LOG2),
    altera_syncram_component.operation_mode  = "DUAL_PORT",
    altera_syncram_component.outdata_aclr_b  = "NONE",
    altera_syncram_component.outdata_sclr_b  = "NONE",
    altera_syncram_component.power_up_uninitialized  = "FALSE",
    altera_syncram_component.ram_block_type  = "M20K",
    altera_syncram_component.read_during_write_mode_mixed_ports  = "DONT_CARE",
    altera_syncram_component.widthad_a  = DEPTH_LOG2,
    altera_syncram_component.widthad_b  = DEPTH_LOG2,
    altera_syncram_component.width_a  = WIDTH,
    altera_syncram_component.width_b  = WIDTH,
    altera_syncram_component.width_byteena_a  = 1,
    altera_syncram_component.enable_force_to_zero  = "TRUE",
    altera_syncram_component.enable_ecc  = "TRUE",
    altera_syncram_component.ecc_pipeline_stage_enabled  = "TRUE",
    altera_syncram_component.enable_ecc_encoder_bypass  = "FALSE",
    altera_syncram_component.width_eccstatus  = 2;

`else

reg[WIDTH-1:0] memory[(1 << DEPTH_LOG2) - 1:0];

reg[DEPTH_LOG2-1:0] readAddrReg;
reg[DEPTH_LOG2-1:0] writeAddrReg;
reg[WIDTH-1:0] writeDataReg;
reg writeEnableReg;

always @(posedge wrclk) begin
    if(writeEnableReg) begin
        memory[writeAddrReg] <= writeDataReg;
    end
    
    writeAddrReg <= writeAddr;
    writeDataReg <= dataIn;
    writeEnableReg <= writeEnable;
end

always @(posedge rdclk) begin
    if(!readAddressStall) readAddrReg <= readAddr;
end

wire[WIDTH-1:0] dataFromMem = memory[readAddrReg];
hyperpipe #(.CYCLES(2), .WIDTH(WIDTH)) dataOutPipe(rdclk, dataFromMem, dataOut);

assign eccStatus = 1'b0;

`endif

endmodule

