`timescale 1ns / 1ps
`include "bramProperties.vh"
`include "leafElimination.vh"

//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 08/07/2021 05:54:46 PM
// Design Name: 
// Module Name: fullPipeline
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


module computeModule #(parameter EXTRA_DATA_SIZE = 14) (
    input clk,
    input start,
    input[127:0] graphIn,
    input[EXTRA_DATA_SIZE-1:0] extraDataIn,
    output ready,
    output started,
    output done,
    output[5:0] resultCount,
    output reg[EXTRA_DATA_SIZE-1:0] extraDataOut
);

always @ (posedge clk) begin
    if(started) begin
        extraDataOut = extraDataIn;
    end
end

wire[127:0] leafEliminatedGraph;
wire[127:0] singletonEliminatedGraph;

wire[5:0] connectCountIn;

leafElimination #(.DIRECTION(`DOWN)) le1(graphIn, leafEliminatedGraph);

singletonElimination se(leafEliminatedGraph, singletonEliminatedGraph, connectCountIn);

countConnectedCore core(
    .clk(clk), 
    .graphIn(singletonEliminatedGraph), 
    .start(start), 
    .connectCountStart(connectCountIn), 
    .connectCount(resultCount), 
    .ready(ready), 
    .started(started),
    .done(done)
);

endmodule


module fullPipeline(
    input clk,
    
    input[127:0] top,
    input[127:0] botA, // botB = varSwap(5,6)(A)
    input[127:0] botC, // botD = varSwap(5,6)(C)
    input[11:0] botIndex,
    input isBotValid,
    output full,
    output almostFull,
    output[`DATA_WIDTH-1:0] summedDataOut,
    output[2:0] pcoeffCountOut
);

wire inputGraphAvailable;
wire collectorQueueFull;

wire coreStart = inputGraphAvailable;
wire coreReady, coreStarted, coreDone;
wire[127:0] graphIn;
wire[`ADDR_WIDTH-1:0] addrIn;
wire[1:0] subAddrIn;

inputModule inputHandler(
    .clk(clk),
    
    // input side
    .top(top),
    .botA(botA), // botB = varSwap(5,6)(A)
    .botC(botC), // botD = varSwap(5,6)(C)
    .botIndex(botIndex),
    .isBotValid(isBotValid),
    .full(full),
    .almostFull(almostFull),
    
    // output side, to countConnectedCore
    .readEnable(coreStarted),
    .graphAvailable(inputGraphAvailable),
    .graphOut(graphIn),
    .graphIndex(addrIn),
    .graphSubIndex(subAddrIn)
);

wire[`ADDR_WIDTH-1:0] addrOut;
wire[1:0] subAddrOut;
wire[5:0] countOut;

computeModule #(.EXTRA_DATA_SIZE(`ADDR_WIDTH + 2)) computeCore(
    .clk(clk),
    .start(coreStart),
    .graphIn(graphIn),
    .extraDataIn({addrIn, subAddrIn}),
    .ready(coreReady),
    .started(coreStarted),
    .done(coreDone),
    .resultCount(countOut),
    .extraDataOut({addrOut, subAddrOut})
);

collectionModule collector(
    .clk(clk),
    
    // input side
    .write(coreDone),
    .dataInAddr(addrOut),
    .dataInSubAddr(subAddrOut),
    .addBit(countOut),
    
    // output side
    .readAddrValid(isBotValid),
    .readAddr(botIndex),
    .summedDataOut(summedDataOut),
    .pcoeffCount(pcoeffCountOut)
);

endmodule

