`timescale 1ns / 1ps

`include "pipelineGlobals_header.v"

module aggregatingPermutePipeline (
    input clk,
    input clk2x,
    input rst,
    output[1:0] activityMeasure, // Instrumentation wire for profiling (0-2 activity level)
    input[127:0] top,
    
    // Input side
    input[128*`NUMBER_OF_PERMUTATORS-1:0] botsIn,
    input[6*`NUMBER_OF_PERMUTATORS-1:0] validBotsPermutes,
    input[`NUMBER_OF_PERMUTATORS-1:0] batchesDone,
    output[`NUMBER_OF_PERMUTATORS-1:0] slowDownInputs,
    
    // Output side
    input grabResults,
    output resultsAvailable,
    output[`PCOEFF_COUNT_BITWIDTH+35-1:0] pcoeffSum,
    output[`PCOEFF_COUNT_BITWIDTH-1:0] pcoeffCount,
    
    // TODO ADD FIFOS TO THIS AS WELL
    output wor eccStatus
);

(* dont_merge *) reg computePipeRST; always @(posedge clk) computePipeRST <= rst;
(* dont_merge *) reg resultsFIFORST; always @(posedge clk) resultsFIFORST <= rst;

reg outputFIFORequestsSlowdown;
wire aggregatingPipelineSlowDownInput;

wire permutedBotValid;
wire[127:0] permutedBot;
wire batchFinished;
botPermuterWithMultiFIFO multiFIFOPermuter (
    .clk(clk),
    .clk2x(clk2x),
    .rst(rst),
    
    // Input side
    .bots(botsIn),
    .validBotsPermutes(validBotsPermutes),
    .batchesDone(batchesDone),
    .slowDownInputs(slowDownInputs),
    
    // Output side
    .permutedBot(permutedBot),
    .permutedBotValid(permutedBotValid),
    .batchFinished(batchFinished),
    .requestSlowDown(aggregatingPipelineSlowDownInput || outputFIFORequestsSlowdown)
);


reg[31:0] batchesDoneCount = 0; always @(posedge clk2x) if(rst) batchesDoneCount <= 0; else if(|batchesDone) batchesDoneCount <= batchesDoneCount + 1;
reg[31:0] batchesFinishedCount = 0; always @(posedge clk) if(rst) batchesFinishedCount <= 0; else if(batchFinished) batchesFinishedCount <= batchesFinishedCount + 1;


wire aggregateFinished;
wire[`PCOEFF_COUNT_BITWIDTH+35-1:0] pcoeffSumFromPipeline;
wire[`PCOEFF_COUNT_BITWIDTH-1:0] pcoeffCountFromPipeline;
aggregatingPipeline computePipe (
    .clk(clk),
    .clk2x(clk2x),
    .rst(computePipeRST),
    .activityMeasure(activityMeasure),
    .top(top),
    
    .isBotValid(permutedBotValid),
    .bot(permutedBot),
    .lastBotOfBatch(batchFinished),
    .slowDownInput(aggregatingPipelineSlowDownInput),
    
    .resultsValid(aggregateFinished),
    .pcoeffSum(pcoeffSumFromPipeline),
    .pcoeffCount(pcoeffCountFromPipeline),
    
    .eccStatus(eccStatus)
);

wire[8:0] outputFIFOUsedw;
// Some registers for extra slack on this connection
reg outputFIFORequestsSlowdownPreReg; always @(posedge clk) outputFIFORequestsSlowdownPreReg <= outputFIFOUsedw > 200; // TODO see if this is enough?
always @(posedge clk) outputFIFORequestsSlowdown <= outputFIFORequestsSlowdownPreReg;

// Some registers for extra slack on this connection
reg aggregateFinishedD; always @(posedge clk) aggregateFinishedD <= aggregateFinished;
reg[`PCOEFF_COUNT_BITWIDTH+35-1:0] pcoeffSumFromPipelineD; always @(posedge clk) pcoeffSumFromPipelineD <= pcoeffSumFromPipeline;
reg[`PCOEFF_COUNT_BITWIDTH-1:0] pcoeffCountFromPipelineD; always @(posedge clk) pcoeffCountFromPipelineD <= pcoeffCountFromPipeline;

wire resultsFIFOEmpty;
assign resultsAvailable = !resultsFIFOEmpty;
FastFIFO #(.WIDTH(`PCOEFF_COUNT_BITWIDTH+35 + `PCOEFF_COUNT_BITWIDTH), .DEPTH_LOG2(9), .IS_MLAB(0)) resultsFIFO (
    .clk(clk),
    .rst(resultsFIFORST),
    
    // input side
    .writeEnable(aggregateFinishedD),
    .dataIn({pcoeffSumFromPipelineD, pcoeffCountFromPipelineD}),
    .usedw(outputFIFOUsedw),
    
    // output side
    .readRequest(grabResults),
    .dataOut({pcoeffSum, pcoeffCount}),
    .dataOutValid(),
    .empty(resultsFIFOEmpty),
    .eccStatus(eccStatus)
);


endmodule
