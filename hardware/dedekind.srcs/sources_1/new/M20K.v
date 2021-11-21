`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 10/09/2021 01:36:06 AM
// Design Name: 
// Module Name: M20K
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: 
// 
// Dependencies: 
// 
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
// 
//////////////////////////////////////////////////////////////////////////////////


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

reg[9:0] memoryA[1023:0];
reg[9:0] memoryB[1023:0];

always @(posedge clk) begin
    if(writeEnable) begin
        if(writeMask[0]) memoryA[writeAddr] <= writeData[9:0];
        if(writeMask[1]) memoryB[writeAddr] <= writeData[19:10];
    end
end

assign readData[09:00] = readEnable ? memoryA[readAddr] : 10'b0000000000;
assign readData[19:10] = readEnable ? memoryB[readAddr] : 10'b0000000000;
endmodule





module simpleDualPortM20K_20b1024Registered (
    input clk,
    
    input writeEnable,
    input[9:0] writeAddr,
    input[1:0] writeMask,
    input[19:0] writeData,
    
    input readEnable, // if not enabled, outputs 0
    input[9:0] readAddr,
    output reg[19:0] readData
);

reg[9:0] writeAddrD;
reg[1:0] writeMaskD;
reg[19:0] writeDataD;

reg readEnableD;
reg writeEnableD;
reg[9:0] readAddrD;


reg[9:0] memoryA[1023:0];
reg[9:0] memoryB[1023:0];

always @(posedge clk) begin
    writeAddrD <= writeAddr;
    writeMaskD <= writeMask;
    writeDataD <= writeData;
    writeEnableD <= writeEnable;
    readEnableD <= readEnable;
    readAddrD <= readAddr;
    
    if(writeEnableD) begin
        if(writeMaskD[0]) memoryA[writeAddrD] <= writeDataD[9:0];
        if(writeMaskD[1]) memoryB[writeAddrD] <= writeDataD[19:10];
    end
    
    readData[09:00] <= readEnableD ? memoryA[readAddrD] : 10'b0000000000;
    readData[19:10] <= readEnableD ? memoryB[readAddrD] : 10'b0000000000;
end

endmodule
