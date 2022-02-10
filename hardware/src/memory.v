`timescale 1ns / 1ps

`include "ipSettings_header.v"

module MEMORY_BLOCK #(
    parameter WIDTH = 20,
    parameter DEPTH_LOG2 = 5,
    parameter IS_MLAB = 1,// = DEPTH_LOG2 <= 5
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

localparam BLOCK_SIZE = IS_MLAB ? 20 : (DEPTH_LOG2 <= 9 ? 32 : DEPTH_LOG2 == 10 ? 16 : 8);
localparam WIDTH_ENLARGED_TO_BLOCK_SIZE = WIDTH + (BLOCK_SIZE - (WIDTH % BLOCK_SIZE)) % BLOCK_SIZE;

wire[WIDTH_ENLARGED_TO_BLOCK_SIZE-1:0] dataInWide;
wire[WIDTH_ENLARGED_TO_BLOCK_SIZE-1:0] dataOutWide;
assign dataInWide[WIDTH-1:0] = dataIn;
generate
if(WIDTH_ENLARGED_TO_BLOCK_SIZE != WIDTH) assign dataInWide[WIDTH_ENLARGED_TO_BLOCK_SIZE-1:WIDTH] = 0;
endgenerate
assign dataOut = dataOutWide[WIDTH-1:0];

generate
if(IS_MLAB) begin

/* MLAB */
altera_syncram  altera_syncram_component (
    .clock0 (clk),
    .address_a (writeAddr),
    .address_b (readAddr),
    .addressstall_b (readAddressStall),
    .data_a (dataInWide),
    .wren_a (writeEnable),
    .q_b (dataOutWide),
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
    .data_b ({WIDTH_ENLARGED_TO_BLOCK_SIZE{1'b1}}),
    .eccencbypass (1'b0),
    .eccencparity (8'b0),
    .eccstatus (),
    .q_a (),
    .rden_a (1'b1),
    .rden_b (1'b1),
    .sclr (1'b0),
    .wren_b (1'b0));
defparam
    altera_syncram_component.address_aclr_b  = "NONE",
    altera_syncram_component.address_reg_b  = "CLOCK0",
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
    altera_syncram_component.outdata_reg_b  = "UNREGISTERED",
    altera_syncram_component.power_up_uninitialized  = "FALSE",
    altera_syncram_component.ram_block_type  = "MLAB",
    altera_syncram_component.read_during_write_mode_mixed_ports  = READ_DURING_WRITE,
    altera_syncram_component.widthad_a  = DEPTH_LOG2,
    altera_syncram_component.widthad_b  = DEPTH_LOG2,
    altera_syncram_component.width_a  = WIDTH_ENLARGED_TO_BLOCK_SIZE,
    altera_syncram_component.width_b  = WIDTH_ENLARGED_TO_BLOCK_SIZE,
    altera_syncram_component.width_byteena_a  = 1,
    altera_syncram_component.enable_force_to_zero  = "FALSE",
    altera_syncram_component.enable_ecc  = "FALSE";

assign eccStatus = 1'b0;
end else begin

/* M20K */
wire[(IS_MLAB ? -1 : 1):0] eccStatusWire;
assign eccStatus = eccStatusWire[1];

altera_syncram  altera_syncram_component (
    .clock0 (clk),
    .address_a (writeAddr),
    .address_b (readAddr),
    .addressstall_b (readAddressStall),
    .data_a (dataInWide),
    .wren_a (writeEnable),
    .q_b (dataOutWide),
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
    .data_b ({WIDTH_ENLARGED_TO_BLOCK_SIZE{1'b1}}),
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
    altera_syncram_component.outdata_reg_b  = "CLOCK0",
    altera_syncram_component.power_up_uninitialized  = "FALSE",
    altera_syncram_component.ram_block_type  = "M20K",
    altera_syncram_component.read_during_write_mode_mixed_ports  = READ_DURING_WRITE,
    altera_syncram_component.widthad_a  = DEPTH_LOG2,
    altera_syncram_component.widthad_b  = DEPTH_LOG2,
    altera_syncram_component.width_a  = WIDTH_ENLARGED_TO_BLOCK_SIZE,
    altera_syncram_component.width_b  = WIDTH_ENLARGED_TO_BLOCK_SIZE,
    altera_syncram_component.width_byteena_a  = 1,
    altera_syncram_component.enable_force_to_zero  = "TRUE",
    altera_syncram_component.enable_ecc  = "TRUE",
    altera_syncram_component.ecc_pipeline_stage_enabled  = "TRUE",
    altera_syncram_component.enable_ecc_encoder_bypass  = "FALSE",
    altera_syncram_component.width_eccstatus  = 2;

end
endgenerate

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
hyperpipe #(.CYCLES(IS_MLAB ? 0 : 2), .WIDTH(WIDTH)) dataOutPipe(clk, dataFromMem, dataOut);

assign eccStatus = 1'b0;

`endif

endmodule




module DUAL_CLOCK_MEMORY_BLOCK #(
    parameter WIDTH = 20,
    parameter DEPTH_LOG2 = 5,
    parameter IS_MLAB = 1// = DEPTH_LOG2 <= 5
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

localparam BLOCK_SIZE = IS_MLAB ? 20 : (DEPTH_LOG2 <= 9 ? 32 : DEPTH_LOG2 == 10 ? 16 : 8);
localparam WIDTH_ENLARGED_TO_BLOCK_SIZE = WIDTH + (BLOCK_SIZE - (WIDTH % BLOCK_SIZE)) % BLOCK_SIZE;

wire[WIDTH_ENLARGED_TO_BLOCK_SIZE-1:0] dataInWide;
wire[WIDTH_ENLARGED_TO_BLOCK_SIZE-1:0] dataOutWide;
assign dataInWide[WIDTH-1:0] = dataIn;
generate
if(WIDTH_ENLARGED_TO_BLOCK_SIZE != WIDTH) assign dataInWide[WIDTH_ENLARGED_TO_BLOCK_SIZE-1:WIDTH] = 0;
endgenerate
assign dataOut = dataOutWide[WIDTH-1:0];

generate
if(IS_MLAB) begin

/* MLAB */
altera_syncram  altera_syncram_component (
    .clock0 (wrclk),
    .clock1 (rdclk),
    .address_a (writeAddr),
    .address_b (readAddr),
    .data_a (dataInWide),
    .wren_a (writeEnable),
    .q_b (dataOutWide),
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
    .data_b ({WIDTH_ENLARGED_TO_BLOCK_SIZE{1'b1}}),
    .eccencbypass (1'b0),
    .eccencparity (8'b0),
    .eccstatus (),
    .q_a (),
    .rden_a (1'b1),
    .rden_b (1'b1),
    .sclr (1'b0),
    .wren_b (1'b0));
defparam
    altera_syncram_component.address_aclr_b  = "NONE",
    altera_syncram_component.address_reg_b  = "CLOCK0",
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
    altera_syncram_component.outdata_reg_b  = "CLOCK1",
    altera_syncram_component.power_up_uninitialized  = "FALSE",
    altera_syncram_component.ram_block_type  = "MLAB",
    altera_syncram_component.read_during_write_mode_mixed_ports  = "DONT_CARE",
    altera_syncram_component.widthad_a  = DEPTH_LOG2,
    altera_syncram_component.widthad_b  = DEPTH_LOG2,
    altera_syncram_component.width_a  = WIDTH_ENLARGED_TO_BLOCK_SIZE,
    altera_syncram_component.width_b  = WIDTH_ENLARGED_TO_BLOCK_SIZE,
    altera_syncram_component.width_byteena_a  = 1,
    altera_syncram_component.enable_force_to_zero  = "FALSE",
    altera_syncram_component.enable_ecc  = "FALSE";

assign eccStatus = 1'b0;
end else begin

/* M20K */
wire[(IS_MLAB ? -1 : 1):0] eccStatusWire;
assign eccStatus = eccStatusWire[1];

altera_syncram  altera_syncram_component (
    .clock0 (wrclk),
    .clock1 (rdclk),
    .address_a (writeAddr),
    .address_b (readAddr),
    .data_a (dataInWide),
    .wren_a (writeEnable),
    .q_b (dataOutWide),
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
    .data_b ({WIDTH_ENLARGED_TO_BLOCK_SIZE{1'b1}}),
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
    altera_syncram_component.outdata_reg_b  = "CLOCK1",
    altera_syncram_component.power_up_uninitialized  = "FALSE",
    altera_syncram_component.ram_block_type  = "M20K",
    altera_syncram_component.read_during_write_mode_mixed_ports  = "DONT_CARE",
    altera_syncram_component.widthad_a  = DEPTH_LOG2,
    altera_syncram_component.widthad_b  = DEPTH_LOG2,
    altera_syncram_component.width_a  = WIDTH_ENLARGED_TO_BLOCK_SIZE,
    altera_syncram_component.width_b  = WIDTH_ENLARGED_TO_BLOCK_SIZE,
    altera_syncram_component.width_byteena_a  = 1,
    altera_syncram_component.enable_force_to_zero  = "TRUE",
    altera_syncram_component.enable_ecc  = "TRUE",
    altera_syncram_component.ecc_pipeline_stage_enabled  = "TRUE",
    altera_syncram_component.enable_ecc_encoder_bypass  = "FALSE",
    altera_syncram_component.width_eccstatus  = 2;

end
endgenerate

`else

reg[WIDTH-1:0] memory[(1 << DEPTH_LOG2) - 1:0];

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

reg[DEPTH_LOG2-1:0] readAddrReg;
always @(posedge rdclk) begin
    if(!readAddressStall) readAddrReg <= readAddr;
end

wire[WIDTH-1:0] dataFromMem = memory[readAddrReg];
hyperpipe #(.CYCLES(IS_MLAB ? 1 : 2), .WIDTH(WIDTH)) dataOutPipe(rdclk, dataFromMem, dataOut);

assign eccStatus = 1'b0;

`endif

endmodule






module packingShiftRegister #(parameter WIDTH = 2, parameter DEPTH = 16) (
    input clk,
    input[WIDTH-1:0] dataIn,
    
    output[DEPTH*WIDTH-1:0] packedDataOut
);

reg[WIDTH-1:0] shifter[DEPTH-1:0];

generate
    genvar i;
    always @(posedge clk) shifter[0] <= dataIn;
    for(i = 1; i < DEPTH; i = i + 1) begin : PACKING_SHIFT_REG_SHIFTER
        always @(posedge clk) shifter[i] <= shifter[i-1];
    end
    for(i = 0; i < DEPTH; i = i + 1) begin : OUTPUT_DATA
        assign packedDataOut[WIDTH*i+:WIDTH] = shifter[i];
    end
endgenerate

endmodule

module unpackingShiftRegister #(parameter WIDTH = 2, parameter DEPTH = 16) (
    input clk,
    input startNewBatch,
    input[DEPTH*WIDTH-1:0] dataIn,
    
    output[WIDTH-1:0] unpackedDataOut
);

reg[WIDTH-1:0] shifter[DEPTH-2:0];

assign unpackedDataOut = startNewBatch ? dataIn[DEPTH*WIDTH-1:(DEPTH-1)*WIDTH] : shifter[DEPTH-2];

generate
    genvar i;
    for(i = 1; i < DEPTH-1; i = i + 1) begin : INPUT_DATA
        always @(posedge clk) begin
            shifter[i] <= startNewBatch ? dataIn[WIDTH*i+:WIDTH] : shifter[i-1];
        end
    end
    always @(posedge clk) begin
        // Last shifter does not need to be guarded, a new batch should be started right before garbage data could come in. 
        // Optimizer will remove the test due to don't care, but will show up in simulation which is nice
        shifter[0] <= startNewBatch ? dataIn[0+:WIDTH] : {WIDTH{1'bX}};
    end
endgenerate

endmodule
