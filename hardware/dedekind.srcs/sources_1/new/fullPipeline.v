`timescale 1ns / 1ps
`include "bramProperties.vh"
`include "leafElimination.vh"

module computeModule #(parameter EXTRA_DATA_WIDTH = 14) (
    input clk,
    input rst,
    input[127:0] top,
    
    // input side
    input[127:0] botIn,
    input graphAvailable,
    input[EXTRA_DATA_WIDTH-1:0] extraDataIn,
    output requestGraph,
    
    // output side
    output done,
    output[5:0] resultCount,
    output[EXTRA_DATA_WIDTH-1:0] extraDataOut
);

wire[127:0] leafEliminatedGraph;
wire[127:0] singletonEliminatedGraph;

wire[5:0] connectCountIn;

wire[127:0] graphIn = top & ~botIn;

leafElimination #(.DIRECTION(`DOWN)) le(graphIn, leafEliminatedGraph);

singletonElimination se(leafEliminatedGraph, singletonEliminatedGraph, connectCountIn);

wire coreRequestsGraph;
reg requestGraphD;
always @(posedge clk) requestGraphD <= coreRequestsGraph;
assign requestGraph = requestGraphD;

pipelinedCountConnectedCore #(.EXTRA_DATA_WIDTH(EXTRA_DATA_WIDTH), .DATA_IN_LATENCY(1)) core(
    .clk(clk), 
    .rst(rst),
    .top(top),
    
    // input side
    .request(coreRequestsGraph), 
    .graphIn(singletonEliminatedGraph), 
    .start(graphAvailable & requestGraphD), 
    .connectCountIn(connectCountIn), 
    .extraDataIn(extraDataIn),
    
    // output side
    .done(done),
    .connectCount(resultCount),
    .extraDataOut(extraDataOut)
);

endmodule


module fullPipeline4 (
    input clk,
    input rst,
    
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

wire botAvailableFifo;
wire coreRequests, coreDone;
wire[127:0] botFromFifo;
wire[`ADDR_WIDTH-1:0] addrIn;
wire[1:0] subAddrIn;

inputModule4 inputHandler(
    .clk(clk),
    
    // input side
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
    .botOutAvailable(botAvailableFifo),
    .botOut(botFromFifo),
    .botOutIndex(addrIn),
    .botOutSubIndex(subAddrIn)
);

wire[`ADDR_WIDTH-1:0] addrOut;
wire[1:0] subAddrOut;
wire[5:0] countOut;

computeModule #(.EXTRA_DATA_WIDTH(`ADDR_WIDTH + 2)) computeCore(
    .clk(clk),
    .rst(rst),
    .top(top),
    
    // input side
    .botIn(botFromFifo),
    .graphAvailable(botAvailableFifo),
    .extraDataIn({addrIn, subAddrIn}),
    .requestGraph(coreRequests),
    
    // output side
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



module fullPipeline6 (
    input clk,
    input rst,
    input[127:0] top,
    
    input[127:0] bot, // Represents all final 3 var swaps
    input[11:0] botIndex,
    input isBotValid,
    input[5:0] validBotPermutations, // == {vABCin, vACBin, vBACin, vBCAin, vCABin, vCBAin}
    output[4:0] fifoFullness,
    output[`DATA_WIDTH-1:0] summedDataOut,
    output[2:0] pcoeffCountOut
);

wire anyBotPermutIsValid = isBotValid & (|validBotPermutations);
wire botAvailableFifo;
wire coreRequests, coreDone;
wire[127:0] botFromFifo;
wire[`ADDR_WIDTH-1:0] addrIn;
wire[1:0] subAddrIn;

inputModule6 #(.EXTRA_DATA_WIDTH(12)) inputHandler (
    .clk(clk),
    
    // input side
    .bot(bot),
    .anyBotPermutIsValid(anyBotPermutIsValid),
    .validBotPermutesIn(validBotPermutations),
    .fifoFullness(fifoFullness),
    .extraDataIn(botIndex),
    
    // output side, to countConnectedCore
    .requestBot(coreRequests),
    .botOutValid(botAvailableFifo),
    .botOut(botFromFifo),
    .extraDataOut(addrIn),
    .selectedBotPermutation(subAddrIn)
);

/*
module inputModule6 #(parameter EXTRA_DATA_WIDTH = 12) (
    input clk,
    
    // input side
    input[127:0] bot,
    input anyBotPermutIsValid, // == botIsValid & &validBotPermutesIn
    input[5:0] validBotPermutesIn, // == {vABCin, vACBin, vBACin, vBCAin, vCABin, vCBAin}
    input[EXTRA_DATA_WIDTH-1:0] extraDataIn,
    output[4:0] fifoFullness,
    
    // output side
    input requestBot,
    output[127:0] botOut,
    output botOutValid,
    output[2:0] selectedBotPermutation,
    output[EXTRA_DATA_WIDTH-1:0] extraDataOut
*/

wire[`ADDR_WIDTH-1:0] addrOut;
wire[1:0] subAddrOut;
wire[5:0] countOut;

computeModule #(.EXTRA_DATA_WIDTH(`ADDR_WIDTH + 2)) computeCore(
    .clk(clk),
    .rst(rst),
    .top(top),
    
    // input side
    .botIn(botFromFifo),
    .graphAvailable(botAvailableFifo),
    .extraDataIn({addrIn, subAddrIn}),
    .requestGraph(coreRequests),
    
    // output side
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

