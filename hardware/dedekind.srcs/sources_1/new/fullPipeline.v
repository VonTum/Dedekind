`timescale 1ns / 1ps
`include "bramProperties.vh"

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


module computeModule(
    input clk,
    input start,
    input[127:0] graphIn,
    input[11:0] batchId,
    output ready,
    output started,
    output done,
    output[5:0] resultCount,
    output reg[11:0] resultBatchId
);

wire[127:0] leafEliminatedGraph;
wire[127:0] singletonEliminatedGraph;

wire[5:0] connectCountIn;

leafEliminationDown le(graphIn, leafEliminatedGraph);
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

always @ (posedge clk) begin
    if(started) begin
        resultBatchId = batchId;
    end
end

endmodule


module fullPipeline(
    input busClk,
    input coreClk,
    
    input[127:0] top,
    input[127:0] botA, // botB = varSwap(5,6)(A)
    input[127:0] botC, // botD = varSwap(5,6)(C)
    input[11:0] botIndex,
    input isBotValid,
    output full,
    output almostFull,
    output[`DATA_WIDTH-1:0] summedDataOut
);

wire coreStart, coreReady, coreStarted, coreDone;
wire[127:0] graphIn;
wire[11:0] addrIn;

inputModule inputHandler(
    // input side
    .inputClk(busClk),
    .top(top),
    .botA(botA), // botB = varSwap(5,6)(A)
    .botC(botC), // botD = varSwap(5,6)(C)
    .botIndex(botIndex),
    .isBotValid(isBotValid),
    .full(full),
    .almostFull(almostFull),
    
    // output side, to countConnectedCore
    .outputClk(coreClk),
    .readEnable(coreStarted),
    .graphAvailable(coreStart),
    .graphOut(graphIn),
    .graphIndex(addrIn)
);

wire[11:0] addrOut;
wire[5:0] countOut;

computeModule computeCore(
    .clk(coreClk),
    .start(coreStart),
    .graphIn(graphIn),
    .batchId(addrIn),
    .ready(coreReady),
    .started(coreStarted),
    .done(coreDone),
    .resultCount(countOut),
    .resultBatchId(addrOut)
);

collectionModule collector(
    // input side
    .dataInClk(coreClk),
    .write(coreDone),
    .dataInAddr(addrOut),
    .addBit(countOut),
    
    // output side
    .dataOutClk(busClk),
    .sumAddrValid(isBotValid),
    .sumAddr(botIndex),
    .summedDataOut(summedDataOut)
);

endmodule

