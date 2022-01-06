`timescale 1ns / 1ps

`include "ipSettings_header.v"

module simpleDualPortM20K40b512 (
    input writeClk,
    input[8:0] writeAddr,
    input[3:0] writeMask,
    input[39:0] writeData,
    
    input readClk,
    input readEnable, // if not enabled, outputs 0
    input[8:0] readAddr,
    output reg[39:0] readData
);

reg[9:0] memoryA[511:0];
reg[9:0] memoryB[511:0];
reg[9:0] memoryC[511:0];
reg[9:0] memoryD[511:0];

always @(posedge writeClk) begin
    if(writeMask[0]) memoryA[writeAddr] <= writeData[9:0];
    if(writeMask[1]) memoryB[writeAddr] <= writeData[19:10];
    if(writeMask[2]) memoryC[writeAddr] <= writeData[29:20];
    if(writeMask[3]) memoryD[writeAddr] <= writeData[39:30];
end

always @(posedge readClk) begin
    readData <= readEnable ? {memoryD[readAddr],memoryC[readAddr],memoryB[readAddr],memoryA[readAddr]} : 0;
end
endmodule

module simpleDualPortM20K_20b1024 (
    input clk,
    
    input writeEnable,
    input[9:0] writeAddr,
    input[1:0] writeMask,
    input[19:0] writeData,
    
    input readEnable, // if not enabled, outputs 0
    input[9:0] readAddr,
    output[19:0] readData
);

reg[19:0] memory[1023:0];

always @(posedge clk) begin
    if(writeEnable) begin
        if(writeMask[0]) memory[writeAddr][9:0] <= writeData[9:0];
        if(writeMask[1]) memory[writeAddr][19:10] <= writeData[19:10];
    end
end

assign readData = readEnable ? memory[readAddr] : 20'b00000000000000000000;
endmodule





module simpleDualPortM20K_20b1024Registered (
    input clk,
    
    input writeEnable,
    input[9:0] writeAddr,
    input[1:0] writeMask,
    input[19:0] writeData,
    
    input readEnable, // if not enabled, outputs 0
    input[9:0] readAddr,
    output[19:0] readData
);


`ifdef USE_M20K_IP
altera_syncram  altera_syncram_component (
                .address_a (writeAddr),
                .address_b (readAddr),
                .byteena_a (writeMask),
                .clock0 (clk),
                .data_a (writeData),
                .rden_b (readEnable),
                .wren_a (writeEnable),
                .q_b (readData),
                .aclr0 (1'b0),
                .aclr1 (1'b0),
                .address2_a (1'b1),
                .address2_b (1'b1),
                .addressstall_a (1'b0),
                .addressstall_b (1'b0),
                .byteena_b (1'b1),
                .clock1 (1'b1),
                .clocken0 (1'b1),
                .clocken1 (1'b1),
                .clocken2 (1'b1),
                .clocken3 (1'b1),
                .data_b ({20{1'b1}}),
                .eccencbypass (1'b0),
                .eccencparity (8'b0),
                .eccstatus (),
                .q_a (),
                .rden_a (1'b1),
                .sclr (1'b0),
                .wren_b (1'b0));
    defparam
        altera_syncram_component.address_aclr_b  = "NONE",
        altera_syncram_component.address_reg_b  = "CLOCK0",
        altera_syncram_component.byte_size  = 10,
        altera_syncram_component.clock_enable_input_a  = "BYPASS",
        altera_syncram_component.clock_enable_input_b  = "BYPASS",
        altera_syncram_component.clock_enable_output_b  = "BYPASS",
        altera_syncram_component.enable_force_to_zero  = "TRUE",
        altera_syncram_component.intended_device_family  = "Stratix 10",
        altera_syncram_component.lpm_type  = "altera_syncram",
        altera_syncram_component.maximum_depth  = 1024,
        altera_syncram_component.numwords_a  = 1024,
        altera_syncram_component.numwords_b  = 1024,
        altera_syncram_component.operation_mode  = "DUAL_PORT",
        altera_syncram_component.outdata_aclr_b  = "NONE",
        altera_syncram_component.outdata_sclr_b  = "NONE",
        altera_syncram_component.outdata_reg_b  = "CLOCK0",
        altera_syncram_component.power_up_uninitialized  = "TRUE",
        altera_syncram_component.ram_block_type  = "M20K",
        altera_syncram_component.rdcontrol_reg_b  = "CLOCK0",
        altera_syncram_component.read_during_write_mode_mixed_ports  = "DONT_CARE",
        altera_syncram_component.widthad_a  = 10,
        altera_syncram_component.widthad_b  = 10,
        altera_syncram_component.width_a  = 20,
        altera_syncram_component.width_b  = 20,
        altera_syncram_component.width_byteena_a  = 2;

`else

reg[9:0] writeAddrD;
reg[1:0] writeMaskD;
reg[19:0] writeDataD;

reg readEnableD;
reg writeEnableD;
reg[9:0] readAddrD;


reg[19:0] memory[1023:0];

reg[19:0] readDataReg;
assign readData = readDataReg;

always @(posedge clk) begin
    writeAddrD <= writeAddr;
    writeMaskD <= writeMask;
    writeDataD <= writeData;
    writeEnableD <= writeEnable;
    readEnableD <= readEnable;
    readAddrD <= readAddr;
    
    if(writeEnableD) begin
        if(writeMaskD[0]) memory[writeAddrD][9:0] <= writeDataD[9:0];
        if(writeMaskD[1]) memory[writeAddrD][19:10] <= writeDataD[19:10];
    end
    
    readDataReg <= readEnableD ? memory[readAddrD] : 20'b00000000000000000000;
end

`endif

endmodule
