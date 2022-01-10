`timescale 1ns / 1ps
`include "pipelineGlobals_header.v"

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

// Some extra slack for a better fitting
// WARNING: fifo fullness lags behind actual fifo fullness, avoid overflow by using slmostFull!
wire[127:0] botD;
wire[`ADDR_WIDTH-1:0] botIndexD;
wire[5:0] validBotPermutationsD;
wire anyBotPermutIsValidD;
hyperpipe #(.CYCLES(2), .WIDTH(128+1+6+`ADDR_WIDTH)) inputsPipe(
    clk, 
    {bot, anyBotPermutIsValid, validBotPermutations, botIndex}, 
    {botD, anyBotPermutIsValidD, validBotPermutationsD, botIndexD}
);

wire[4:0] usedw_writeSide;
wire readyForBotBurst;
// Some slack because this may be a long-ish wire, also exact timing isn't really important, delay cycles vs max fifo fullness tradeoff
hyperpipe #(.CYCLES(2), .WIDTH(1)) readyForBotBurstPipe(clk, 
    usedw_writeSide <= 20,
    readyForBotBurst
);
wire botAvailableFromInputModule;
wire[127:0] botFromInputModule;
wire[`ADDR_WIDTH-1:0] addrIn;
wire[2:0] subAddrIn;

inputModule6 #(.EXTRA_DATA_WIDTH(`ADDR_WIDTH)) inputHandler (
    .clk(clk),
    .rst(rst),
    
    // input side
    .bot(botD),
    .anyBotPermutIsValid(anyBotPermutIsValidD),
    .validBotPermutesIn(validBotPermutationsD),
    .fifoFullness(fifoFullness),
    .extraDataIn(botIndexD),
    
    // output side, to countConnectedCore
    .readyForBotBurst(readyForBotBurst),
    .botOutValid(botAvailableFromInputModule),
    .botOut(botFromInputModule),
    .extraDataOut(addrIn),
    .selectedBotPermutation(subAddrIn)
);

`define COMPUTE_MODULE_EXTRA_DATA_WIDTH (`ADDR_WIDTH + 3)

// Compute Module Side
wire[127:0] botToComputeModule;
wire[`COMPUTE_MODULE_EXTRA_DATA_WIDTH-1:0] extraDataToComputeModule;
wire readRequestFromComputeModule;
wire dataValidToComputeModule;

wire coreRequestsPreDelay;
`define REQUEST_LATENCY 3
hyperpipe #(.CYCLES(`REQUEST_LATENCY), .WIDTH(1)) requestPipe(clk, coreRequestsPreDelay, readRequestFromComputeModule);

`define FIFO2_LATENCY 0
FastFIFO #(.WIDTH(128+`COMPUTE_MODULE_EXTRA_DATA_WIDTH)) fifoToComputeModule(
    .clk(clk),
    .rst(rst),
     
    // input side
    .writeEnable(botAvailableFromInputModule),
    .dataIn({botFromInputModule, {addrIn, subAddrIn}}),
    .usedw(usedw_writeSide),
    
    // output side
    .readRequest(readRequestFromComputeModule), // FIFO2 has read protection, no need to check empty
    .dataOut({botToComputeModule, extraDataToComputeModule}),
    .dataOutValid(dataValidToComputeModule)
);

wire coreDone;
wire[5:0] countOut;
wire[`ADDR_WIDTH-1:0] addrOut;
wire[2:0] subAddrOut;

computeModule #(.EXTRA_DATA_WIDTH(`COMPUTE_MODULE_EXTRA_DATA_WIDTH), .REQUEST_LATENCY(`REQUEST_LATENCY + `FIFO2_LATENCY)) computeCore(
    .clk(clk),
    .rst(rst),
    .top(top),
    
    // input side
    .botIn(botToComputeModule),
    .start(dataValidToComputeModule),
    .extraDataIn(extraDataToComputeModule),
    .requestGraph(coreRequestsPreDelay),
    
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

