`timescale 1ns / 1ps

`define ADDR_WIDTH 9
`define ADDR_DEPTH 512

`define INPUT_FIFO_DEPTH_LOG2 5

`define INPUT_FIFO_MARGIN 12

module streamingCountConnectedCore #(parameter EXTRA_DATA_WIDTH = 1) (
    input clk,
    input clk2x,
    input rst,
    input[1:0] topChannel,
    output reg[1:0] activityMeasure, // Instrumentation wire for profiling (0-2 activity level)
    
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
wire pipelineECC;

always @(posedge clk) eccStatus <= collectorECC_D || isBotValidECC_D || pipelineECC;

reg[`ADDR_WIDTH-1:0] curBotIndex = 0;
always @(posedge clk) curBotIndex <= curBotIndex + 1;

wire[128+`ADDR_WIDTH-1:0] dataFromInputFIFO2x;
wire inputFifoAvailable2x;
wire requestGraph2x;
reg requestGraph2xD; always @(posedge clk2x) requestGraph2xD <= requestGraph2x;

reg[127:0] graphToComputeModule2x;
reg[`ADDR_WIDTH-1:0] addrToComputeModule2x;
always @(posedge clk2x) {graphToComputeModule2x, addrToComputeModule2x} <= dataFromInputFIFO2x;
reg dataValidToComputeModule2x; always @(posedge clk2x) dataValidToComputeModule2x <= inputFifoAvailable2x && requestGraph2xD;

wire pipelineRST2x;
synchronizer pipelineRSTSynchronizer(clk, pipelineRST, clk2x, pipelineRST2x);
(* dont_merge *) reg inputFIFORST2x; always @(posedge clk) inputFIFORST2x <= pipelineRST2x;
(* dont_merge *) reg cccRST2x; always @(posedge clk) cccRST2x <= pipelineRST2x;

// request Pipe has 1 cycle, FIFO has 0 cycles read latency, dataOut pipe has 1
`define FIFO_READ_LATENCY (1+0+1)
LowLatencyFastDualClockFIFO_MLAB #(.WIDTH(128+`ADDR_WIDTH), .ALMOST_FULL_MARGIN(12)) inputFIFO (// Upper 6 cycles max latency for permutation generation, 4 cycles turnaround time, 2 cycles of margin
    // input side
    .wrclk(clk),
    .wrrst(pipelineRST),
    .writeEnable(isBotValid),
    .dataIn({graphIn, curBotIndex}),
    .almostFull(slowDownInput),
    
    // output side
    .rdclk(clk2x),
    .rdrst(inputFIFORST2x),
    .readRequest(requestGraph2xD),
    .dataOut(dataFromInputFIFO2x),
    .dataOutAvailable(inputFifoAvailable2x)
);

wire writeToCollector2x;
wire[5:0] connectCountToCollector2x;
wire[`ADDR_WIDTH-1:0] addrToCollector2x;


// Instrumentation for profiling
wire isActive2x;
reg isActive2xD; always @(posedge clk2x) isActive2xD <= isActive2x;// Sample the second tick too, to avoid bias from clk-clk2x synchronization
wire isActiveA; synchronizer isActiveASync(clk2x, isActive2x, clk, isActiveA);
wire isActiveB; synchronizer isActiveBSync(clk2x, isActive2xD, clk, isActiveB);
always @(posedge clk) activityMeasure = isActiveA + isActiveB;

wire pipelineECC2x;
fastToSlowPulseSynchronizer eccSync(clk2x, pipelineECC2x, clk, pipelineECC);

wire[1:0] topChannel2x;
synchronizer #(.WIDTH(2)) topChannel2xSync(clk, topChannel, clk2x, topChannel2x);

pipelinedCountConnectedCoreWithSingletonElimination #(.EXTRA_DATA_WIDTH(`ADDR_WIDTH), .REQUEST_LATENCY(`FIFO_READ_LATENCY)) countConnectedCore (
    .clk(clk2x),
    .rst(cccRST2x),
    .topChannel(topChannel2x),
    .isActive(isActive2x),
    
    // input side
    .leafEliminatedGraph(graphToComputeModule2x),
    .start(dataValidToComputeModule2x),
    .extraDataIn(addrToComputeModule2x),
    .requestGraph(requestGraph2x),
    
    // output side
    .done(writeToCollector2x),
    .resultCount(connectCountToCollector2x),
    .extraDataOut(addrToCollector2x),
    .eccStatus(pipelineECC2x)
);

DUAL_CLOCK_MEMORY_M20K #(.WIDTH(6), .DEPTH_LOG2(`ADDR_WIDTH)) collectorMemory (
    // Write Side
    .wrclk(clk2x),
    .writeEnable(writeToCollector2x),
    .writeAddr(addrToCollector2x),
    .dataIn(connectCountToCollector2x),
    
    // Read Side
    .rdclk(clk),
    .readEnable(1'b1),
    .readAddr(curBotIndex),
    .dataOut(connectCount),
    .eccStatus(collectorECC)
);

MEMORY_M20K #(.WIDTH(1+EXTRA_DATA_WIDTH), .DEPTH_LOG2(`ADDR_WIDTH), .READ_DURING_WRITE("OLD_DATA")) isValidMemory (
    .clk(clk),
    
    // Write Side
    .writeEnable(1'b1),
    .writeAddr(curBotIndex),
    .dataIn({isBotValid, extraDataIn}),
    
    // Read Side
    .readEnable(1'b1),
    .readAddr(curBotIndex),
    .dataOut({resultValid, extraDataOut}),
    .eccStatus(isBotValidECC)
);

endmodule
