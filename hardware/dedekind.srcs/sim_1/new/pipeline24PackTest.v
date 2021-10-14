`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 10/13/2021 11:02:21 PM
// Design Name: 
// Module Name: pipeline24PackTest
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


module pipeline24PackTest();

reg clk;
initial begin
    clk = 0;
    forever #1 clk = ~clk;
end

parameter MEMSIZE = 16384;

wire[$clog2(MEMSIZE)-1:0] index;
wire[127:0] top, bot;
wire isBotValid;
wire full, almostFull;
wire[39:0] summedData;
wire[4:0] pcoeffCount;

pipeline24Pack elementUnderTest (
    .clk(clk),
    .top(top),
    .bot(bot),
    .botIndex(index[11:0]),
    .isBotValid(isBotValid),
    .full(full),
    .almostFull(almostFull),
    .summedData(summedData),
    .pcoeffCount(pcoeffCount)
);

indexProvider #(MEMSIZE) dataProvider (
    .clk(clk),
    .index(index),
    .requestData(!almostFull),
    .dataAvailable(isBotValid)
);

reg[128*2+64+8-1:0] dataTable[MEMSIZE-1:0];
initial $readmemb("pipeline24PackTestSet7.mem", dataTable);
wire[39:0] offsetSum;
wire[4:0] offsetCount;
assign {top, bot} = dataTable[index][128*2+64+8-1 : 64+8];

assign offsetSum = dataTable[index-4096][39+8-1 : 8];
assign offsetCount = dataTable[index-4096][5-1 : 0];

endmodule
