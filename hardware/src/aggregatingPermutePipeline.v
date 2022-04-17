`timescale 1ns / 1ps

`include "pipelineGlobals_header.v"

module aggregatingPipelineWithOutputFIFO #(parameter PCOEFF_COUNT_BITWIDTH = 10) (
    input clk,
    input clk2x,
    input rst,
    input[1:0] topChannel,
    output[1:0] activityMeasure, // Instrumentation wire for profiling (0-2 activity level)
    
    // Input side
    input isBotValid,
    input[127:0] bot,
    input lastBotOfBatch,
    output reg slowDownInput,
    
    // Output side
    input grabResults,
    output resultsAvailable,
    output[PCOEFF_COUNT_BITWIDTH+35-1:0] pcoeffSum,
    output[PCOEFF_COUNT_BITWIDTH-1:0] pcoeffCount,
    
    output wor eccStatus
);

(* dont_merge *) reg computePipeRST; always @(posedge clk) computePipeRST <= rst;
(* dont_merge *) reg resultsFIFORST; always @(posedge clk) resultsFIFORST <= rst;

// Extra fitting registers
(* dont_merge *) reg[1:0] topChannelD; always @(posedge clk) topChannelD <= topChannel;
(* dont_merge *) reg[1:0] topChannelDD; always @(posedge clk) topChannelDD <= topChannelD;

wire aggregateFinished;
wire[PCOEFF_COUNT_BITWIDTH+35-1:0] pcoeffSumFromPipeline;
wire[PCOEFF_COUNT_BITWIDTH-1:0] pcoeffCountFromPipeline;
wire aggregatingPipelineSlowDownInput;
aggregatingPipeline #(PCOEFF_COUNT_BITWIDTH) computePipe (
    .clk(clk),
    .clk2x(clk2x),
    .rst(computePipeRST),
    .topChannel(topChannelDD),
    .activityMeasure(activityMeasure),
    
    .isBotValid(isBotValid),
    .bot(bot),
    .lastBotOfBatch(lastBotOfBatch),
    .slowDownInput(aggregatingPipelineSlowDownInput),
    
    .resultsValid(aggregateFinished),
    .pcoeffSum(pcoeffSumFromPipeline),
    .pcoeffCount(pcoeffCountFromPipeline),
    
    .eccStatus(eccStatus)
);

reg outputFIFORequestsSlowdown;

// Some registers for extra slack on this connection
wire outputFIFOAlmostFull;
always @(posedge clk) outputFIFORequestsSlowdown <= outputFIFOAlmostFull;
always @(posedge clk) slowDownInput <= aggregatingPipelineSlowDownInput || outputFIFORequestsSlowdown;

// Some registers for extra slack on this connection
reg aggregateFinishedD; always @(posedge clk) aggregateFinishedD <= aggregateFinished;
reg[PCOEFF_COUNT_BITWIDTH+35-1:0] pcoeffSumFromPipelineD; always @(posedge clk) pcoeffSumFromPipelineD <= pcoeffSumFromPipeline;
reg[PCOEFF_COUNT_BITWIDTH-1:0] pcoeffCountFromPipelineD; always @(posedge clk) pcoeffCountFromPipelineD <= pcoeffCountFromPipeline;

wire resultsFIFOEmpty;
assign resultsAvailable = !resultsFIFOEmpty;
FIFO_M20K #(.WIDTH(PCOEFF_COUNT_BITWIDTH+35 + PCOEFF_COUNT_BITWIDTH), .DEPTH_LOG2(9), .ALMOST_FULL_MARGIN(300 /*TODO See if this is enough?*/)) resultsFIFO (
    .clk(clk),
    .rst(resultsFIFORST),
    
    // input side
    .writeEnable(aggregateFinishedD),
    .dataIn({pcoeffSumFromPipelineD, pcoeffCountFromPipelineD}),
    .almostFull(outputFIFOAlmostFull),
    
    // output side
    .readEnable(grabResults),
    .dataOut({pcoeffSum, pcoeffCount}),
    .empty(resultsFIFOEmpty),
    .eccStatus(eccStatus)
);

endmodule

module aggregatingPermutePipeline #(parameter PCOEFF_COUNT_BITWIDTH = 10) (
    input clk,
    input clk2x,
    input rst,
    input[1:0] topChannel,
    output[1:0] activityMeasure, // Instrumentation wire for profiling (0-2 activity level)
    
    // Input side
    input[128*`NUMBER_OF_PERMUTATORS-1:0] botsIn,
    input[6*`NUMBER_OF_PERMUTATORS-1:0] validBotsPermutes,
    input[`NUMBER_OF_PERMUTATORS-1:0] batchesDone,
    output[`NUMBER_OF_PERMUTATORS-1:0] slowDownInputs,
    
    // Output side
    input grabResults,
    output resultsAvailable,
    output[PCOEFF_COUNT_BITWIDTH+35-1:0] pcoeffSum,
    output[PCOEFF_COUNT_BITWIDTH-1:0] pcoeffCount,
    
    output eccStatus
);

wire requestSlowDown;

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
    .requestSlowDown(requestSlowDown)
);


reg[31:0] batchesDoneCount = 0; always @(posedge clk2x) if(rst) batchesDoneCount <= 0; else if(|batchesDone) batchesDoneCount <= batchesDoneCount + 1;
reg[31:0] batchesFinishedCount = 0; always @(posedge clk) if(rst) batchesFinishedCount <= 0; else if(batchFinished) batchesFinishedCount <= batchesFinishedCount + 1;


aggregatingPipelineWithOutputFIFO #(PCOEFF_COUNT_BITWIDTH) aggregatingPipelineWithFIFO(
    clk,
    clk2x,
    rst,
    topChannel,
    activityMeasure, // Instrumentation wire for profiling (0-2 activity level)
    
    // Input side
    permutedBotValid,
    permutedBot,
    batchFinished,
    requestSlowDown,
    
    // Output side
    grabResults,
    resultsAvailable,
    pcoeffSum,
    pcoeffCount,
    
    eccStatus
);

endmodule



module aggregatingPermutePipeline24 #(parameter PCOEFF_COUNT_BITWIDTH = 10) (
    input clk,
    input clk2x,
    input rst,
    input longRST,
    input[1:0] topChannel,
    output[1:0] activityMeasure, // Instrumentation wire for profiling (0-2 activity level)
    
    // Input side
    input[127:0] botIn,
    input[23:0] validBotPermutes,
    input batchDone,
    output almostFull,
    
    // Output side
    input slowDown,
    output resultValid,
    output[PCOEFF_COUNT_BITWIDTH+35-1:0] pcoeffSum,
    output[PCOEFF_COUNT_BITWIDTH-1:0] pcoeffCount,
    
    output reg eccStatus
);

(* dont_merge *) reg botPermuterRST; always @(posedge clk) botPermuterRST <= rst;
(* dont_merge *) reg pipelineRST; always @(posedge clk) pipelineRST <= rst;

wor eccStatusWire;
always @(posedge clk) eccStatus <= eccStatusWire;

wire requestSlowDown;

wire permutedBotValid;
wire[127:0] permutedBot;
wire batchFinished;
botPermuter1234 #(.ALMOST_FULL_MARGIN(64)) botPermuter1234 (
    .clk(clk),
    .rst(botPermuterRST),
    
    // Input side
    .writeDataIn(|validBotPermutes || batchDone),
    .botIn(botIn),
    .validBotPermutesIn(validBotPermutes),
    .lastBotOfBatchIn(batchDone),
    .almostFull(almostFull),
    
    // Output side
    .permutedBot(permutedBot),
    .permutedBotValid(permutedBotValid),
    .batchDone(batchFinished),
    .slowDown(requestSlowDown),
    
    .eccStatus(eccStatusWire)
);

(* dont_merge *) reg[1:0] topChannelD; always @(posedge clk) topChannelD <= topChannel;
(* dont_merge *) reg[1:0] topChannelDD; always @(posedge clk) topChannelDD <= topChannelD;

aggregatingPipeline #(PCOEFF_COUNT_BITWIDTH) computePipe (
    .clk(clk),
    .clk2x(clk2x),
    .rst(pipelineRST),
    .longRST(longRST),
    .topChannel(topChannelDD),
    .activityMeasure(activityMeasure),
    
    .isBotValid(permutedBotValid),
    .bot(permutedBot),
    .lastBotOfBatch(batchFinished),
    .slowDownInput(requestSlowDown),
    
    .resultsValid(resultValid),
    .pcoeffSum(pcoeffSum),
    .pcoeffCount(pcoeffCount),
    
    .eccStatus(eccStatusWire)
);

endmodule

