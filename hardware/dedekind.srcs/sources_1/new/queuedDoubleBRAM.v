`timescale 1ns / 1ps
`include "bramProperties.vh"

//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 08/17/2021 07:06:54 PM
// Design Name: 
// Module Name: queuedDoubleBRAM
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


// Intel M20K blocks provide builtin Quad-Port memory, that can replace this here
module queuedDoubleBRAM(
    input clk, 
    
    input[5:0] countIn,
    input[`ADDR_WIDTH:0] inIdx,
    input dataInAvailable,
    output fifoAlmostFull,
    output fifoFull,
    
    // output side
    input[`ADDR_WIDTH:0] outIdx,
    output[39:0] selectedSum
);

wire fifoAlmostFullA, fifoAlmostFullB;
wire fifoAFullA, fifoFullB;

wire[39:0] sumA, sumB;

reg selectorHistory[`READ_LATENCY-1:0];

generate
    for(genvar i = 0; i < `READ_LATENCY-1; i=i+1) begin
        always @(posedge clk) begin
            selectorHistory[i] <= selectorHistory[i+1];
        end
    end
endgenerate
always @(posedge clk) begin
    selectorHistory[`READ_LATENCY-1] <= outIdx[0];
end

assign fifoAlmostFull = fifoAlmostFullA | fifoAlmostFullB;
assign fifoFull = fifoFullA | fifoFullB;
assign selectedSum = selectorHistory[0] ? sumA : sumB;

queuedAdderTable bankA(
    .clk(clk),
    
    // input side
    .countIn(countIn),
    .inIdx(inIdx[`ADDR_WIDTH:1]),
    .dataInAvailable(dataInAvailable & inIdx[0]),
    .fifoAlmostFull(fifoAlmostFullA),
    .fifoFull(fifoFullA),
    
    // output side
    .outIdx(outIdx[`ADDR_WIDTH:1]),
    .isSelected(outIdx[0]), // should perform a read/reset cycle, if not, data can be written
    .currentSum(sumA)
);

queuedAdderTable bankB(
    .clk(clk),
    
    // input side
    .countIn(countIn),
    .inIdx(inIdx[`ADDR_WIDTH:1]),
    .dataInAvailable(dataInAvailable & !inIdx[0]),
    .fifoAlmostFull(fifoAlmostFullB),
    .fifoFull(fifoFullB),
    
    // output side
    .outIdx(outIdx[`ADDR_WIDTH:1]),
    .isSelected(!outIdx[0]), // should perform a read/reset cycle, if not, data can be written
    .currentSum(sumB)
);

endmodule
