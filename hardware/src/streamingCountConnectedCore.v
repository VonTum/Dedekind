`timescale 1ns / 1ps

`define PIPELINE_FIFO_DEPTH_LOG2 9

module streamingCountConnectedCore #(parameter EXTRA_DATA_WIDTH = 2) (
    input clk,
    input rst,
    input[127:0] top,
    
    // Input side
    input isBotValid,
    input[127:0] bot,
    input[EXTRA_DATA_WIDTH-1:0] extraDataIn,
    output[`PIPELINE_FIFO_DEPTH_LOG2-1:0] usedw,
    
    // Output side
    output resultValid,
    output[5:0] connectCount,
    output[EXTRA_DATA_WIDTH-1:0] extraDataOut,
    output inputFifoECC,
    output collectorECC,
    output isBotValidECC
);

reg[`ADDR_WIDTH-1:0] curBotIndex = 0;
always @(posedge clk) curBotIndex <= curBotIndex + 1;

wire readRequestFromComputeModule;
wire[127:0] botToComputeModule;
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
    .dataIn({bot, curBotIndex}),
    .usedw(usedw),
    
    // output side
    .readRequest(readRequestFromComputeModule), // FIFO2 has read protection, no need to check empty
    .dataOut({botToComputeModule, botToComputeModuleAddr}),
    .dataOutValid(dataValidToComputeModule),
    .eccStatus(inputFifoECC)
);

wire writeToCollector;
wire[5:0] connectCountToCollector;
wire[`ADDR_WIDTH-1:0] addrToCollector;

computeModule #(.EXTRA_DATA_WIDTH(`ADDR_WIDTH), .REQUEST_LATENCY(`FIFO_READ_LATENCY)) core (
    .clk(clk),
    .rst(rst),
    .top(top),
    
    // input side
    .botIn(botToComputeModule),
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

MEMORY_BLOCK #(.WIDTH(1+EXTRA_DATA_WIDTH), .DEPTH_LOG2(`ADDR_WIDTH), .IS_MLAB(0), .READ_DURING_WRITE("OLD_DATA")) isValidMemory (
    .clk(clk),
    
    // Write Side
    .writeEnable(1'b1),
    .writeAddr(curBotIndex),
    .dataIn({isBotValid, extraDataIn}),
    
    // Read Side
    .readAddressStall(1'b0),
    .readAddr(curBotIndex),
    .dataOut({resultValid, extraDataOut}),
    .eccStatus(isBotValidECC)
);

endmodule
