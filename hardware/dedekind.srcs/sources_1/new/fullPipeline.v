`timescale 1ns / 1ps
`include "bramProperties.vh"
`include "leafElimination.vh"

module computeModule #(parameter EXTRA_DATA_WIDTH = 14) (
    input clk,
    input[127:0] top,
    input[127:0] graphIn,
    input graphAvailable,
    input[EXTRA_DATA_WIDTH-1:0] extraDataIn,
    output requestGraph,
    output done,
    output[5:0] resultCount,
    output[EXTRA_DATA_WIDTH-1:0] extraDataOut
);

wire[127:0] leafEliminatedGraph;
wire[127:0] singletonEliminatedGraph;

wire[5:0] connectCountIn;

leafElimination #(.DIRECTION(`DOWN)) le(graphIn, leafEliminatedGraph);

singletonElimination se(leafEliminatedGraph, singletonEliminatedGraph, connectCountIn);

`ifdef USE_OLD_CCC
countConnectedCore #(.EXTRA_DATA_WIDTH(EXTRA_DATA_WIDTH)) core(
    .clk(clk), 
    .top(top),
    .graphIn(singletonEliminatedGraph), 
    .graphInAvailable(graphAvailable), 
    .extraDataIn(extraDataIn),
    .extraDataOut(extraDataOut),
    .connectCountIn(connectCountIn), 
    .connectCount(resultCount), 
    .request(requestGraph), 
    .done(done)
);
`else
wire graphInIsZero = singletonEliminatedGraph == 0;

pipelinedCountConnectedCore #(.EXTRA_DATA_WIDTH(EXTRA_DATA_WIDTH)) core(
    .clk(clk), 
    .top(top),
    
    // input side
    .request(requestGraph), 
    .graphIn(singletonEliminatedGraph), 
    .graphInAvailable(graphAvailable), 
    .graphInIsZero(graphInIsZero),
    .connectCountIn(connectCountIn), 
    .extraDataIn(extraDataIn),
    
    // output side
    .done(done),
    .connectCount(resultCount),
    .extraDataOut(extraDataOut)
);
`endif

endmodule


module fullPipeline(
    input clk,
    
    input[127:0] top,
    input[127:0] botA, // botB = varSwap(5,6)(A)
    input[127:0] botC, // botD = varSwap(5,6)(C)
    input[11:0] botIndex,
    input isBotValid,
    input validBotA,
    input validBotB,
    input validBotC,
    input validBotD,
    output full,
    output almostFull,
    output[`DATA_WIDTH-1:0] summedDataOut,
    output[2:0] pcoeffCountOut
);

wire graphAvailable;
wire coreRequests, coreDone;
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
    .validBotA(validBotA),
    .validBotB(validBotB),
    .validBotC(validBotC),
    .validBotD(validBotD),
    .full(full),
    .almostFull(almostFull),
    
    // output side, to countConnectedCore
    .dataRequested(coreRequests),
    .graphAvailable(graphAvailable),
    .graphOut(graphIn),
    .graphIndex(addrIn),
    .graphSubIndex(subAddrIn)
);

wire[`ADDR_WIDTH-1:0] addrOut;
wire[1:0] subAddrOut;
wire[5:0] countOut;

computeModule #(.EXTRA_DATA_WIDTH(`ADDR_WIDTH + 2)) computeCore(
    .clk(clk),
    .top(top),
    .graphIn(graphIn),
    .graphAvailable(graphAvailable),
    .extraDataIn({addrIn, subAddrIn}),
    .requestGraph(coreRequests),
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

