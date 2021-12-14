`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 07/23/2021 01:13:23 PM
// Design Name: 
// Module Name: varSwap
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


module varSwap #(
    parameter FIRST_VAR = 5,
    parameter SECOND_VAR = 6
) (
    input [127:0] dataIn,
    output [127:0] dataOut
);

`include "inlineVarSwap.vh"

`VAR_SWAP_INLINE(FIRST_VAR, SECOND_VAR, dataIn, dataOut)

endmodule
