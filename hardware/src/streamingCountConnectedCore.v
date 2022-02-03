`timescale 1ns / 1ps

`define ADDR_WIDTH 9
`define ADDR_DEPTH 512

`define INPUT_FIFO_DEPTH_LOG2 5
`define INPUT_FIFO_ALMOST_FULL 22

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

wire collectorECC; reg collectorECC_D; always @(posedge clk) collectorECC_D <= collectorECC;
wire isBotValidECC; reg isBotValidECC_D; always @(posedge clk) isBotValidECC_D <= isBotValidECC;

always @(posedge clk) eccStatus <= collectorECC_D || isBotValidECC_D;

wire [`INPUT_FIFO_DEPTH_LOG2-1:0] usedw;
assign slowDownInput = usedw > `INPUT_FIFO_ALMOST_FULL;

reg[`ADDR_WIDTH-1:0] curBotIndex = 0;
always @(posedge clk) curBotIndex <= curBotIndex + 1;

wire readRequestFromComputeModule2x;
wire[127:0] graphToComputeModule2x;
wire[`ADDR_WIDTH-1:0] botToComputeModuleAddr2x;
wire dataValidToComputeModule2x;

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
    .DEPTH_LOG2(`INPUT_FIFO_DEPTH_LOG2)
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
    .dataOutValid(dataValidToComputeModule2xPreDelay)
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
