`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 10/09/2021 08:18:08 PM
// Design Name: 
// Module Name: synchronizer
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

// data width is one bit as properly synchronizing multiple bits is impossible. 
module synchronizer #(parameter SYNC_STEPS = 3) (
    input inClk,
    input outClk,
    input dataIn,
    output dataOut
);

reg inReg;
always @(posedge inClk) inReg <= dataIn;

reg syncRegs[SYNC_STEPS-1:0];
always @(posedge outClk) syncRegs[SYNC_STEPS-1] <= inReg;

generate
for(genvar i = 0; i < SYNC_STEPS-1; i=i+1) always @(posedge outClk) syncRegs[i] <= syncRegs[i+1];
endgenerate

assign dataOut = syncRegs[0];

endmodule
