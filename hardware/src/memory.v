`timescale 1ns / 1ps

`include "ipSettings_header.v"

module MEMORY_MLAB #(
    parameter WIDTH = 20,
    parameter DEPTH_LOG2 = 5,
    parameter READ_DURING_WRITE = "DONT_CARE", // Options are "DONT_CARE", "OLD_DATA" and "NEW_DATA",
    parameter OUTPUT_REGISTER = 0
) (
    input clk,
    
    // Write Side
    input writeEnable,
    input[DEPTH_LOG2-1:0] writeAddr,
    input[WIDTH-1:0] dataIn,
    
    // Read Side
    input[DEPTH_LOG2-1:0] readAddr,
    output[WIDTH-1:0] dataOut
);

`ifdef USE_MLAB_IP

altera_syncram  altera_syncram_component (
    .clock0 (clk),
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
    .addressstall_b (1'b0),
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
    .eccstatus (),
    .q_a (),
    .rden_a (1'b1),
    .rden_b (1'b1),
    .sclr (1'b0),
    .wren_b (1'b0));
defparam
    altera_syncram_component.ram_block_type  = "MLAB",
    altera_syncram_component.address_reg_b  = "CLOCK0",
    altera_syncram_component.outdata_reg_b  = OUTPUT_REGISTER ? "CLOCK0" : "UNREGISTERED",
    altera_syncram_component.address_aclr_b  = "NONE",
    altera_syncram_component.outdata_aclr_b  = "NONE",
    altera_syncram_component.outdata_sclr_b  = "NONE",
    altera_syncram_component.width_a  = WIDTH,
    altera_syncram_component.width_b  = WIDTH,
    altera_syncram_component.widthad_a  = DEPTH_LOG2,
    altera_syncram_component.widthad_b  = DEPTH_LOG2,
    altera_syncram_component.numwords_a  = (1 << DEPTH_LOG2),
    altera_syncram_component.numwords_b  = (1 << DEPTH_LOG2),
    altera_syncram_component.clock_enable_input_a  = "BYPASS",
    altera_syncram_component.clock_enable_input_b  = "BYPASS",
    altera_syncram_component.clock_enable_output_b  = "BYPASS",
    altera_syncram_component.intended_device_family  = "Stratix 10",
    altera_syncram_component.lpm_type  = "altera_syncram",
    altera_syncram_component.operation_mode  = "DUAL_PORT",
    altera_syncram_component.power_up_uninitialized  = "FALSE",
    altera_syncram_component.read_during_write_mode_mixed_ports  = READ_DURING_WRITE,
    altera_syncram_component.width_byteena_a  = 1,
    altera_syncram_component.enable_force_to_zero  = "FALSE",
    altera_syncram_component.enable_ecc  = "FALSE";
`else

reg[WIDTH-1:0] memory[(1 << DEPTH_LOG2) - 1:0];

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

reg[DEPTH_LOG2-1:0] readAddrReg; always @(posedge clk) readAddrReg <= readAddr;
wire[WIDTH-1:0] dataFromMem = (writeEnableReg && READ_DURING_WRITE == "DONT_CARE" && writeAddrReg == readAddrReg) ? {WIDTH{1'bX}} : memory[readAddrReg];

generate
if(OUTPUT_REGISTER) begin
    reg[WIDTH-1:0] dataFromMemD;
    always @(posedge clk) dataFromMemD <= dataFromMem;
    assign dataOut = dataFromMemD;
end else
    assign dataOut = dataFromMem;
endgenerate

`endif

endmodule


module MEMORY_M20K #(
    parameter WIDTH = 20,
    parameter DEPTH_LOG2 = 9,
    parameter READ_DURING_WRITE = "DONT_CARE" // Options are "DONT_CARE", "OLD_DATA" and "NEW_DATA"
) (
    input clk,
    
    // Write Side
    input writeEnable,
    input[DEPTH_LOG2-1:0] writeAddr,
    input[WIDTH-1:0] dataIn,
    
    // Read Side
    input readEnable, // if not readEnable then the output is forced to 0 (thanks to force_to_zero)
    input[DEPTH_LOG2-1:0] readAddr,
    output[WIDTH-1:0] dataOut,
    output eccStatus
);

`ifdef USE_M20K_IP

wire[1:0] eccStatusWire;
assign eccStatus = |eccStatusWire;

altera_syncram  altera_syncram_component (
    .clock0 (clk),
    .address_a (writeAddr),
    .address_b (readAddr),
    .data_a (dataIn),
    .q_b (dataOut),
    .wren_a (writeEnable),
    .rden_b (readEnable),
    .sclr (1'b0),
    .aclr0 (1'b0),
    .aclr1 (1'b0),
    .address2_a (1'b1),
    .address2_b (1'b1),
    .addressstall_a (1'b0),
    .addressstall_b (1'b0),
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
    .wren_b (1'b0));
defparam
    altera_syncram_component.ram_block_type  = "M20K",
    altera_syncram_component.address_reg_b  = "CLOCK0",
    altera_syncram_component.outdata_reg_b  = "CLOCK0",
    altera_syncram_component.address_aclr_b  = "NONE",
    altera_syncram_component.outdata_aclr_b  = "NONE",
    altera_syncram_component.outdata_sclr_b  = "NONE",
    altera_syncram_component.width_a  = WIDTH,
    altera_syncram_component.width_b  = WIDTH,
    altera_syncram_component.widthad_a  = DEPTH_LOG2,
    altera_syncram_component.widthad_b  = DEPTH_LOG2,
    altera_syncram_component.numwords_a  = (1 << DEPTH_LOG2),
    altera_syncram_component.numwords_b  = (1 << DEPTH_LOG2),
    altera_syncram_component.clock_enable_input_a  = "BYPASS",
    altera_syncram_component.clock_enable_input_b  = "BYPASS",
    altera_syncram_component.clock_enable_output_b  = "BYPASS",
    altera_syncram_component.intended_device_family  = "Stratix 10",
    altera_syncram_component.lpm_type  = "altera_syncram",
    altera_syncram_component.operation_mode  = "DUAL_PORT",
    altera_syncram_component.power_up_uninitialized  = "FALSE",
    altera_syncram_component.read_during_write_mode_mixed_ports  = READ_DURING_WRITE,
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
reg readEnableReg;

always @(posedge clk) begin
    readAddrReg <= readAddr;
    readEnableReg <= readEnable;
    
    if(writeEnableReg) begin
        memory[writeAddrReg] <= writeDataReg;
    end
    
    writeAddrReg <= writeAddr;
    writeDataReg <= dataIn;
    writeEnableReg <= writeEnable;
end

wire[WIDTH-1:0] dataFromMem = !readEnableReg ? 0 : (writeEnableReg && READ_DURING_WRITE == "DONT_CARE" && writeAddrReg == readAddrReg) ? {WIDTH{1'bX}} : memory[readAddrReg];
reg[WIDTH-1:0] dataFromMemD; always @(posedge clk) dataFromMemD <= dataFromMem;
reg[WIDTH-1:0] dataFromMemDD; always @(posedge clk) dataFromMemDD <= dataFromMemD;

assign dataOut = dataFromMemDD;

assign eccStatus = 1'b0;

`endif

endmodule




module DUAL_CLOCK_MEMORY_MLAB #(
    parameter WIDTH = 20,
    parameter DEPTH_LOG2 = 5,
    parameter OUTPUT_REGISTER = 1
) (
    // Write Side
    input wrclk,
    input writeEnable,
    input[DEPTH_LOG2-1:0] writeAddr,
    input[WIDTH-1:0] dataIn,
    
    // Read Side
    input rdclk,
    input[DEPTH_LOG2-1:0] readAddr,
    output[WIDTH-1:0] dataOut
);

`ifdef USE_MLAB_IP

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
    .addressstall_b (1'b0),
    .byteena_a (1'b1),
    .byteena_b (1'b1),
    .clocken0 (1'b1),
    .clocken1 (1'b1),
    .clocken2 (1'b1),
    .clocken3 (1'b1),
    .data_b ({WIDTH{1'b1}}),
    .eccencbypass (1'b0),
    .eccencparity (8'b0),
    .eccstatus (),
    .q_a (),
    .rden_a (1'b1),
    .rden_b (1'b1),
    .sclr (1'b0),
    .wren_b (1'b0));
defparam
    altera_syncram_component.ram_block_type  = "MLAB",
    altera_syncram_component.address_reg_b  = "CLOCK1",
    altera_syncram_component.outdata_reg_b  = OUTPUT_REGISTER ? "CLOCK1" : "UNREGISTERED",
    altera_syncram_component.address_aclr_b  = "NONE",
    altera_syncram_component.outdata_aclr_b  = "NONE",
    altera_syncram_component.outdata_sclr_b  = "NONE",
    altera_syncram_component.width_a  = WIDTH,
    altera_syncram_component.width_b  = WIDTH,
    altera_syncram_component.widthad_a  = DEPTH_LOG2,
    altera_syncram_component.widthad_b  = DEPTH_LOG2,
    altera_syncram_component.numwords_a  = (1 << DEPTH_LOG2),
    altera_syncram_component.numwords_b  = (1 << DEPTH_LOG2),
    altera_syncram_component.clock_enable_input_a  = "BYPASS",
    altera_syncram_component.clock_enable_input_b  = "BYPASS",
    altera_syncram_component.clock_enable_output_b  = "BYPASS",
    altera_syncram_component.intended_device_family  = "Stratix 10",
    altera_syncram_component.lpm_type  = "altera_syncram",
    altera_syncram_component.operation_mode  = "DUAL_PORT",
    altera_syncram_component.power_up_uninitialized  = "FALSE",
    altera_syncram_component.read_during_write_mode_mixed_ports  = "DONT_CARE",
    altera_syncram_component.width_byteena_a  = 1,
    altera_syncram_component.enable_force_to_zero  = "FALSE",
    altera_syncram_component.enable_ecc  = "FALSE";
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

reg[DEPTH_LOG2-1:0] readAddrReg; always @(posedge rdclk) readAddrReg <= readAddr;
wire[WIDTH-1:0] dataFromMem = memory[readAddrReg];

generate
if(OUTPUT_REGISTER) begin
    reg[WIDTH-1:0] dataFromMemD;
    always @(posedge rdclk) dataFromMemD <= dataFromMem;
    assign dataOut = dataFromMemD;
end else
    assign dataOut = dataFromMem;
endgenerate

`endif

endmodule



module DUAL_CLOCK_MEMORY_M20K #(
    parameter WIDTH = 20,
    parameter DEPTH_LOG2 = 9
) (
    // Write Side
    input wrclk,
    input writeEnable,
    input[DEPTH_LOG2-1:0] writeAddr,
    input[WIDTH-1:0] dataIn,
    
    // Read Side
    input rdclk,
    input readEnable, // if not readEnable then the output is forced to 0 (thanks to force_to_zero)
    input[DEPTH_LOG2-1:0] readAddr,
    output[WIDTH-1:0] dataOut,
    output eccStatus
);

`ifdef USE_M20K_IP

wire[1:0] eccStatusWire;
assign eccStatus = |eccStatusWire;

altera_syncram  altera_syncram_component (
    .clock0 (wrclk),
    .clock1 (rdclk),
    .address_a (writeAddr),
    .address_b (readAddr),
    .data_a (dataIn),
    .q_b (dataOut),
    .wren_a (writeEnable),
    .rden_b (readEnable),
    .sclr (1'b0),
    .aclr0 (1'b0),
    .aclr1 (1'b0),
    .address2_a (1'b1),
    .address2_b (1'b1),
    .addressstall_a (1'b0),
    .addressstall_b (1'b0),
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
    .wren_b (1'b0));
defparam
    altera_syncram_component.ram_block_type  = "M20K",
    altera_syncram_component.address_reg_b  = "CLOCK1",
    altera_syncram_component.outdata_reg_b  = "CLOCK1",
    altera_syncram_component.address_aclr_b  = "NONE",
    altera_syncram_component.outdata_aclr_b  = "NONE",
    altera_syncram_component.outdata_sclr_b  = "NONE",
    altera_syncram_component.width_a  = WIDTH,
    altera_syncram_component.width_b  = WIDTH,
    altera_syncram_component.widthad_a  = DEPTH_LOG2,
    altera_syncram_component.widthad_b  = DEPTH_LOG2,
    altera_syncram_component.numwords_a  = (1 << DEPTH_LOG2),
    altera_syncram_component.numwords_b  = (1 << DEPTH_LOG2),
    altera_syncram_component.clock_enable_input_a  = "BYPASS",
    altera_syncram_component.clock_enable_input_b  = "BYPASS",
    altera_syncram_component.clock_enable_output_b  = "BYPASS",
    altera_syncram_component.intended_device_family  = "Stratix 10",
    altera_syncram_component.lpm_type  = "altera_syncram",
    altera_syncram_component.operation_mode  = "DUAL_PORT",
    altera_syncram_component.power_up_uninitialized  = "FALSE",
    altera_syncram_component.read_during_write_mode_mixed_ports  = "DONT_CARE",
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
reg readEnableReg;

always @(posedge wrclk) begin
    if(writeEnableReg) begin
        memory[writeAddrReg] <= writeDataReg;
    end
    
    writeAddrReg <= writeAddr;
    writeDataReg <= dataIn;
    writeEnableReg <= writeEnable;
end

always @(posedge rdclk) begin
    readAddrReg <= readAddr;
    readEnableReg <= readEnable;
end

wire[WIDTH-1:0] dataFromMem = readEnableReg ? memory[readAddrReg] : 0;
reg[WIDTH-1:0] dataFromMemD; always @(posedge rdclk) dataFromMemD <= dataFromMem;
reg[WIDTH-1:0] dataFromMemDD; always @(posedge rdclk) dataFromMemDD <= dataFromMemD;

assign dataOut = dataFromMemDD;

assign eccStatus = 1'b0;

`endif

endmodule



module NO_READ_CLOCK_MEMORY_MLAB #(
    parameter WIDTH = 20,
    parameter DEPTH_LOG2 = 5
) (
    // Write Side
    input wrclk,
    input writeEnable,
    input[DEPTH_LOG2-1:0] writeAddr,
    input[WIDTH-1:0] dataIn,
    
    // Read Side
    input[DEPTH_LOG2-1:0] readAddr,
    output[WIDTH-1:0] dataOut
);

`ifdef USE_MLAB_IP

altdpram  altdpram_component (
    .inclock (wrclk),
    .data (dataIn),
    .wraddress (writeAddr),
    .wren (writeEnable),
    .rdaddress (readAddr),
    .q (dataOut),
    .aclr (1'b0),
    .sclr (1'b0),
    .byteena (1'b1),
    .inclocken (1'b1),
    .outclock (1'b1),
    .outclocken (1'b1),
    .rdaddressstall (1'b0),
    .rden (1'b1),
    .wraddressstall (1'b0));
defparam
    altdpram_component.ram_block_type  = "MLAB",
    altdpram_component.indata_reg  = "INCLOCK",
    altdpram_component.wraddress_reg  = "INCLOCK",
    altdpram_component.wrcontrol_reg  = "INCLOCK",
    altdpram_component.intended_device_family  = "Stratix 10",
    altdpram_component.lpm_type  = "altdpram",
    altdpram_component.indata_aclr  = "OFF",
    altdpram_component.outdata_aclr  = "OFF",
    altdpram_component.rdaddress_aclr  = "OFF",
    altdpram_component.rdcontrol_aclr  = "OFF",
    altdpram_component.wraddress_aclr  = "OFF",
    altdpram_component.wrcontrol_aclr  = "OFF",
    altdpram_component.outdata_sclr  = "OFF",
    altdpram_component.outdata_reg  = "UNREGISTERED",
    altdpram_component.rdaddress_reg  = "UNREGISTERED",
    altdpram_component.rdcontrol_reg  = "UNREGISTERED",
    altdpram_component.width  = WIDTH,
    altdpram_component.widthad  = DEPTH_LOG2,
    altdpram_component.width_byteena  = 1;
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

assign dataOut = memory[readAddr];

`endif

endmodule
