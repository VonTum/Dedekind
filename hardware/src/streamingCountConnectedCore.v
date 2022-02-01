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
    output eccStatus
);

wire inputFifoECC;
wire collectorECC;
wire isBotValidECC;

assign eccStatus = inputFifoECC || collectorECC || isBotValidECC;

`define PIPELINE_FIFO_DEPTH_LOG2 9

wire [`PIPELINE_FIFO_DEPTH_LOG2-1:0] usedw;
assign slowDownInput = usedw > `ADDR_DEPTH / 22 - 50; // 22 cycles is the longest a bot could spend in the pipeline

reg[`ADDR_WIDTH-1:0] curBotIndex = 0;
always @(posedge clk) curBotIndex <= curBotIndex + 1;

wire readRequestFromComputeModule;
wire[127:0] graphToComputeModule;
wire[`ADDR_WIDTH-1:0] botToComputeModuleAddr;
wire dataValidToComputeModule;

// FIFO has 3 cycles read latency
`define FIFO_READ_LATENCY 3
FastFIFO #(
    .WIDTH(128+`ADDR_WIDTH),
    .DEPTH_LOG2(`PIPELINE_FIFO_DEPTH_LOG2),
    .IS_MLAB(0),
    .READ_ADDR_STAGES(1)
) inputFIFO (
    .clk(clk),
    .rst(rst),
     
    // input side
    .writeEnable(isBotValid),
    .dataIn({graphIn, curBotIndex}),
    .usedw(usedw),
    
    // output side
    .readRequest(readRequestFromComputeModule), // FIFO2 has read protection, no need to check empty
    .dataOut({graphToComputeModule, botToComputeModuleAddr}),
    .dataOutValid(dataValidToComputeModule),
    .empty(),
    .eccStatus(inputFifoECC)
);

wire writeToCollector;
wire[5:0] connectCountToCollector;
wire[`ADDR_WIDTH-1:0] addrToCollector;

pipelinedCountConnectedCoreWithSingletonElimination #(.EXTRA_DATA_WIDTH(`ADDR_WIDTH), .REQUEST_LATENCY(`FIFO_READ_LATENCY)) countConnectedCore (
    .clk(clk),
    .rst(rst),
    
    // input side
    
    .leafEliminatedGraph(graphToComputeModule),
    .start(dataValidToComputeModule),
    .extraDataIn(botToComputeModuleAddr),
    .requestGraph(readRequestFromComputeModule),
    
    // output side
    .done(writeToCollector),
    .resultCount(connectCountToCollector),
    .extraDataOut(addrToCollector)
);

MEMORY_BLOCK #(.WIDTH(6), .DEPTH_LOG2(`ADDR_WIDTH), .IS_MLAB(0)) collectorMemory (
    .clk(clk),
    
    // Write Side
    .writeEnable(writeToCollector),
    .writeAddr(addrToCollector),
    .dataIn(connectCountToCollector),
    
    // Read Side
    .readAddressStall(1'b0),
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
