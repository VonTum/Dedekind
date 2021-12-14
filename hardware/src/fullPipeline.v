`timescale 1ns / 1ps
`include "pipelineGlobals.vh"

module fullPipeline4 (
    input clk,
    input rst,
    
    input[127:0] top,
    input[127:0] botA, // botB = varSwap(5,6)(A)
    input[127:0] botC, // botD = varSwap(5,6)(C)
    input[`ADDR_WIDTH-1:0] botIndex,
    input isBotValid,
    input validBotA,
    input validBotB,
    input validBotC,
    input validBotD,
    output full,
    output almostFull,
    output[37:0] summedDataOut,
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



module fullPipeline (
    input clk,
    input rst,
    input[127:0] top,
    
    input[127:0] bot, // Represents all final 3 var swaps
    input[`ADDR_WIDTH-1:0] botIndex,
    input isBotValid,
    input[5:0] validBotPermutations, // == {vABCin, vACBin, vBACin, vBCAin, vCABin, vCBAin}
    output[4:0] fifoFullness,
    output[37:0] summedDataOut,
    output[2:0] pcoeffCountOut
);

wire anyBotPermutIsValid = isBotValid & (|validBotPermutations);
wire botAvailableFifo;
wire coreRequests, coreDone;
wire[127:0] botFromFifo;
wire[`ADDR_WIDTH-1:0] addrIn;
wire[2:0] subAddrIn;

inputModule6 #(.EXTRA_DATA_WIDTH(`ADDR_WIDTH)) inputHandler (
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
module inputModule6 #(parameter EXTRA_DATA_WIDTH = `ADDR_WIDTH) (
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
wire[2:0] subAddrOut;
wire[5:0] countOut;

computeModule #(.EXTRA_DATA_WIDTH(`ADDR_WIDTH + 3)) computeCore(
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
    .readAddr(botIndex),
    .summedDataOut(summedDataOut),
    .pcoeffCount(pcoeffCountOut)
);

endmodule

