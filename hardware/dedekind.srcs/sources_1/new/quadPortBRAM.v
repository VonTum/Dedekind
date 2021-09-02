`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 08/25/2021 12:48:18 AM
// Design Name: 
// Module Name: quadPortBRAM
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


module quadPortBRAM #(parameter DATA_WIDTH = 40, parameter ADDR_WIDTH = 12, parameter READ_LATENCY = 2) (
    input clkA,
    input wEnableA,
    input[ADDR_WIDTH-1:0] wAddrA,
    input[ADDR_WIDTH-1:0] rAddrA,
    input[DATA_WIDTH-1:0] wDataA,
    output[DATA_WIDTH-1:0] rDataA,
    
    input clkB,
    input wEnableB,
    input[ADDR_WIDTH-1:0] wAddrB,
    input[ADDR_WIDTH-1:0] rAddrB,
    input[DATA_WIDTH-1:0] wDataB,
    output[DATA_WIDTH-1:0] rDataB
);

reg[DATA_WIDTH-1:0] memory[(1 << ADDR_WIDTH)-1:0];

registerPipe #(.WIDTH(DATA_WIDTH), .DEPTH(READ_LATENCY)) pipelineA(.clk(clkA), .dataIn(memory[rAddrA]), .dataOut(rDataA));
registerPipe #(.WIDTH(DATA_WIDTH), .DEPTH(READ_LATENCY)) pipelineB(.clk(clkB), .dataIn(memory[rAddrB]), .dataOut(rDataB));

always @(posedge clkA) if(wEnableA) memory[wAddrA] <= wDataA;
always @(posedge clkB) if(wEnableB) memory[wAddrB] <= wDataB;

endmodule
