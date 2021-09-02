`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 06/28/2021 01:17:06 AM
// Design Name: 
// Module Name: countConnectedCore
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

module countConnectedCore(
    input clk,
    input[127:0] graphIn,
    input start,
    input[5:0] connectCountStart,
    output reg[5:0] connectCount,
    output ready,
    output started,
    output done
); 

// debugging
/*reg clk;
reg[127:0] graphIn;
reg start;
reg[5:0] connectCountStart;
initial begin
    clk = 0;
    #1 forever #1 clk = ~clk;
end
initial begin
    start = 0;
    connectCountStart = 0;
    graphIn = 0;
    
    #5
    graphIn[127:96] = 32'b00000000_00000001_00000001_00010110; // {a,b,c,d,e}, 5 cc
    start = 1;
    #2 start = 0;
    
    
    #15
    graphIn[127:96] = 32'b00010011_00100010_01010000_10101000; // difficult connected component, 1 cc
    start = 1;
    #2 start = 0;
    
    
    
    #15
    graphIn[127:96] = 32'b00010001_00000010_01000100_10101010; // regular test case, 3 cc
    start = 1;
    #2 start = 0;
    
    
end//*/

reg[127:0] graph;
reg[127:0] curExtending;
reg isNewSeed;

wire[127:0] newSeed;
firstBit firstBitOfGraph(graph, newSeed, ready);
assign started = ready & start;

wire[127:0] floodFillIn = isNewSeed ? newSeed : curExtending;
wire[127:0] floodFillOut;
wire ffRunComplete;
floodFillStep ffStep(graph, floodFillIn, floodFillOut, ffRunComplete);

initial graph = 0;

always @ (posedge clk) begin
    // no test needed for curExtending, just update even when not running, requires less hardware
    curExtending <= floodFillOut;
    // isNewSeed will be 1 by default when starting out. Therefore also no test needed! Very elegant!
    isNewSeed <= ffRunComplete;
end

always @ (posedge clk) begin
    // first clock, load registers
    if(started) begin
        graph <= graphIn;
        connectCount <= connectCountStart;
    end
    
    // new component discovered, explore!
    if(!ready & ffRunComplete) begin
        graph <= graph & ~floodFillOut; // remove current component to start new component
        connectCount <= connectCount + 1;
    end
end

reg prevReady = 1;
reg prevStart = 0;
always @ (posedge clk) begin
    prevReady <= ready;
    prevStart <= start;
end
assign done = ready & (!prevReady | prevStart); // outputs a 1-clock pulse for every processed graph
endmodule
