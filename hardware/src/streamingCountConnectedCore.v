`timescale 1ns / 1ps

`include "pipelineGlobals_header.v"

module streamingCountConnectedCore #(parameter EXTRA_DATA_WIDTH = 1) (
    input clk,
    input clk2x,
    input rst,
    
    // Input side
    input isBotValid,
    input[127:0] graphIn,
    input[EXTRA_DATA_WIDTH-1:0] extraDataIn,
    output slowDownInput,
    
    // Output side
    output resultValid,
    output[5:0] connectCount,
    output[EXTRA_DATA_WIDTH-1:0] extraDataOut,
    output reg eccStatus
);

(* dont_merge *) reg pipelineRST; always @(posedge clk) pipelineRST <= rst;

wire inputFifoECC; reg inputFifoECC_D; always @(posedge clk) inputFifoECC_D <= inputFifoECC;
wire collectorECC; reg collectorECC_D; always @(posedge clk) collectorECC_D <= collectorECC;
wire isBotValidECC; reg isBotValidECC_D; always @(posedge clk) isBotValidECC_D <= isBotValidECC;

always @(posedge clk) eccStatus <= inputFifoECC_D || collectorECC_D || isBotValidECC_D;

`define PIPELINE_FIFO_DEPTH_LOG2 9

wire [`PIPELINE_FIFO_DEPTH_LOG2-1:0] usedw;
assign slowDownInput = usedw > `ADDR_DEPTH / 22 - 50; // 22 cycles is the longest a bot could spend in the pipeline

reg[`ADDR_WIDTH-1:0] curBotIndex = 0;
always @(posedge clk) curBotIndex <= curBotIndex + 1;

wire readRequestFromComputeModule2x;
wire[127:0] graphToComputeModule2x;
wire[`ADDR_WIDTH-1:0] botToComputeModuleAddr2x;
wire dataValidToComputeModule2x;

wire inputFifoECC2x;
reg inputFifoECCReg2x; always @(posedge clk) inputFifoECCReg2x <= inputFifoECC2x;
reg inputFifoECCReg2x_D; always @(posedge clk) inputFifoECCReg2x_D <= inputFifoECCReg2x;
reg inputFifoECCReg2x_DD; always @(posedge clk) inputFifoECCReg2x_DD <= inputFifoECCReg2x_D;
reg inputFifoECC2x_LONGER; always @(posedge clk) inputFifoECC2x_LONGER <= inputFifoECCReg2x || inputFifoECCReg2x_D || inputFifoECCReg2x_DD;
synchronizer inputFIFOECCSynchronizer(clk2x, inputFifoECC2x_LONGER, clk, inputFifoECC);

wire[128+`ADDR_WIDTH-1:0] dataFromFIFO2xPreDelay;
wire dataValidToComputeModule2xPreDelay;
wire readRequestFromComputeModule2xPreDelay;
hyperpipe #(.CYCLES(1), .WIDTH(1)) readRequestToFIFOPipe(clk2x, readRequestFromComputeModule2xPreDelay, readRequestFromComputeModule2x);
hyperpipe #(.CYCLES(1), .WIDTH(128+`ADDR_WIDTH+1)) dataFromFIFOPipe(clk2x, {dataFromFIFO2xPreDelay, dataValidToComputeModule2xPreDelay}, {graphToComputeModule2x, botToComputeModuleAddr2x, dataValidToComputeModule2x});

wire pipelineRST2x;
synchronizer pipelineRSTSynchronizer(clk, pipelineRST, clk2x, pipelineRST2x);
// request Pipe has 1 cycle, FIFO has 1 cycle read latency, dataOut pipe has 1
`define FIFO_READ_LATENCY (1+1+1)
dualClockFIFOWithDataValid #(
    .WIDTH(128+`ADDR_WIDTH),
    .DEPTH_LOG2(`PIPELINE_FIFO_DEPTH_LOG2)
) inputFIFO (
    // input side
    .wrclk(clk),
    .writeEnable(isBotValid),
    .dataIn({graphIn, curBotIndex}),
    .wrusedw(usedw),
    
    // output side
    .rdclk(clk2x),
    .rdclr(pipelineRST2x),
    .readEnable(readRequestFromComputeModule2x), // FIFO2 has read protection, no need to check empty
    .dataOut(dataFromFIFO2xPreDelay),
    .dataOutValid(dataValidToComputeModule2xPreDelay),
    .eccStatus(inputFifoECC2x)
);

wire writeToCollector2x;
wire[5:0] connectCountToCollector2x;
wire[`ADDR_WIDTH-1:0] addrToCollector2x;

pipelinedCountConnectedCoreWithSingletonElimination #(.EXTRA_DATA_WIDTH(`ADDR_WIDTH), .REQUEST_LATENCY(`FIFO_READ_LATENCY)) countConnectedCore (
    .clk(clk2x),
    .rst(pipelineRST2x),
    
    // input side
    .leafEliminatedGraph(graphToComputeModule2x),
    .start(dataValidToComputeModule2x),
    .extraDataIn(botToComputeModuleAddr2x),
    .requestGraph(readRequestFromComputeModule2xPreDelay),
    
    // output side
    .done(writeToCollector2x),
    .resultCount(connectCountToCollector2x),
    .extraDataOut(addrToCollector2x)
);

DUAL_CLOCK_MEMORY_BLOCK #(.WIDTH(6), .DEPTH_LOG2(`ADDR_WIDTH), .IS_MLAB(0)) collectorMemory (
    // Write Side
    .wrclk(clk2x),
    .writeEnable(writeToCollector2x),
    .writeAddr(addrToCollector2x),
    .dataIn(connectCountToCollector2x),
    
    // Read Side
    .rdclk(clk),
    .readAddr(curBotIndex),
    .dataOut(connectCount),
    .eccStatus(collectorECC)
);

botDataMemoryModule #(1+EXTRA_DATA_WIDTH) isValidExtraDataMemory (
    clk,
    curBotIndex,
    {isBotValid, extraDataIn},
    {resultValid, extraDataOut},
    isBotValidECC
);

endmodule

module botDataMemoryModule #(parameter WIDTH = 2) (
    input clk,
    input[`ADDR_WIDTH-1:0] curBotIndex,
    
    input[WIDTH-1:0] dataIn,
    output[WIDTH-1:0] dataOut,
    output isBotValidECC
);

// Annoyingly ECC memory requires a width of block size 32 bits, so we have to chunk data together before writing to the memory
`define IS_VALID_MEMORY_CHUNK_SIZE_LOG2 4 // 16 elements per chunk
`define IS_VALID_MEMORY_CHUNK_SIZE (1 << `IS_VALID_MEMORY_CHUNK_SIZE_LOG2)

wire[WIDTH * `IS_VALID_MEMORY_CHUNK_SIZE-1:0] packedIsValidDataToMem;
wire[WIDTH * `IS_VALID_MEMORY_CHUNK_SIZE-1:0] packedIsValidDataFromMem;
packingShiftRegister #(.WIDTH(WIDTH), .DEPTH(`IS_VALID_MEMORY_CHUNK_SIZE)) packer (
    .clk(clk),
    .dataIn(dataIn),
    .packedDataOut(packedIsValidDataToMem)
);
wire[`ADDR_WIDTH - `IS_VALID_MEMORY_CHUNK_SIZE_LOG2-1:0] curBotIndexMSB = curBotIndex[`ADDR_WIDTH-1:`IS_VALID_MEMORY_CHUNK_SIZE_LOG2];
wire[`IS_VALID_MEMORY_CHUNK_SIZE_LOG2-1:0] curBotIndexLSB = curBotIndex[`IS_VALID_MEMORY_CHUNK_SIZE_LOG2-1:0];

reg[`ADDR_WIDTH - `IS_VALID_MEMORY_CHUNK_SIZE_LOG2-1:0] prevBotIndexMSB; always @(posedge clk) prevBotIndexMSB <= curBotIndexMSB;
reg pushValidBlock; always @(posedge clk) pushValidBlock <= curBotIndexLSB == `IS_VALID_MEMORY_CHUNK_SIZE-1;

reg[`ADDR_WIDTH - `IS_VALID_MEMORY_CHUNK_SIZE_LOG2-1:0] validChunkWriteAddr; always @(posedge clk) validChunkWriteAddr <= curBotIndexMSB;
MEMORY_BLOCK #(.WIDTH((WIDTH) * `IS_VALID_MEMORY_CHUNK_SIZE), .DEPTH_LOG2(`ADDR_WIDTH - `IS_VALID_MEMORY_CHUNK_SIZE_LOG2), .IS_MLAB(0)) isValidMemory (
    .clk(clk),
    
    // Write Side
    .writeEnable(pushValidBlock),
    .writeAddr(validChunkWriteAddr),
    .dataIn(packedIsValidDataToMem),
    
    // Read Side
    .readAddressStall(1'b0),
    .readAddr(curBotIndexMSB),
    .dataOut(packedIsValidDataFromMem),
    .eccStatus(isBotValidECC)
);

wire validBlockAvailableReadSide; hyperpipe #(.CYCLES(3)) validBlockPipe(clk, pushValidBlock, validBlockAvailableReadSide);
unpackingShiftRegister #(.WIDTH(WIDTH), .DEPTH(`IS_VALID_MEMORY_CHUNK_SIZE)) unpacker (
    .clk(clk),
    .startNewBatch(validBlockAvailableReadSide),
    .dataIn(packedIsValidDataFromMem),
    .unpackedDataOut(dataOut)
);


endmodule
