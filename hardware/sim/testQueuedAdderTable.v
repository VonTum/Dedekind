`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 08/17/2021 04:31:21 PM
// Design Name: 
// Module Name: testQueuedAdderTable
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


module testQueuedAdderTable;

reg clk = 0;
reg[5:0] countIn = 0;
reg[8:0] inIdx = 0;
reg dataInAvailable = 0;
reg[8:0] outIdx = 0;
reg isSelected = 0;

wire fifoAlmostFull;
wire fifoFull;

wire[39:0] currentSum;


queuedAdderTable tableWithQueue(
    .clk(clk),
    
    // input side
    .countIn(countIn),
    .inIdx(inIdx),
    .dataInAvailable(dataInAvailable),
    .fifoAlmostFull(fifoAlmostFull),
    .fifoFull(fifoFull),
    
    // output side
    .outIdx(outIdx),
    .isSelected(isSelected), // should perform a read/reset cycle, if not, data can be written
    .currentSum(currentSum)
);

always #1 clk = !clk;


endmodule
