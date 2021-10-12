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


module simpleDualPortM20K(
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
