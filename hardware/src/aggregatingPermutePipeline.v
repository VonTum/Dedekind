`timescale 1ns / 1ps

`include "pipelineGlobals_header.v"

module aggregatingPermutePipeline (
    input clk,
    input clk2x,
    input rst,
    input[127:0] top,
    
    // Input side
    input[127:0] bot,
    input writeData,
    input[5:0] validBotPermutes,
    input batchDone,
    output reg slowDownInput,
    
    // Output side
    input grabResults,
    output resultsAvailable,
    output[`PCOEFF_COUNT_BITWIDTH+35-1:0] pcoeffSum,
    output[`PCOEFF_COUNT_BITWIDTH-1:0] pcoeffCount,
    
    // TODO ADD FIFOS TO THIS AS WELL
    output wor eccStatus
);

(* dont_merge *) reg inputFIFORST; always @(posedge clk) inputFIFORST <= rst;
(* dont_merge *) reg permuter6RST; always @(posedge clk) permuter6RST <= rst;
(* dont_merge *) reg computePipeRST; always @(posedge clk) computePipeRST <= rst;
(* dont_merge *) reg resultsFIFORST; always @(posedge clk) resultsFIFORST <= rst;

wire[4:0] inputFifoUsedw;
always @(posedge clk) slowDownInput <= inputFifoUsedw > 24;

wire[8:0] outputFIFOUsedw;
wire aggregatingPipelineSlowDownInput;
wire inputBotQueueEmpty;
wire botPermuterRequestsNewBot;
wire grabNew6Pack = botPermuterRequestsNewBot && !inputBotQueueEmpty && !aggregatingPipelineSlowDownInput && !(outputFIFOUsedw > 200); // TODO see if this is enough?
wire[127:0] botToPermute;
wire[5:0] botToPermuteValidPermutations;
wire batchDonePostFIFO;



FIFO #(.WIDTH(128+6+1), .DEPTH_LOG2(5)) inputFIFO (
    .clk(clk),
    .rst(inputFIFORST),
    
    // input side
    .writeEnable(writeData && (|validBotPermutes || batchDone)),
    .dataIn({bot, validBotPermutes, batchDone}),
    .full(),
    .usedw(inputFifoUsedw),
    
    // output side
    .readEnable(grabNew6Pack),
    .dataOut({botToPermute, botToPermuteValidPermutations, batchDonePostFIFO}),
    .empty(inputBotQueueEmpty)
);

wire permutedBotValid;
wire[127:0] permutedBot;

// permutes the last 3 variables
botPermuter #(.EXTRA_DATA_WIDTH(0)) permuter6 (
    .clk(clk),
    .rst(permuter6RST),
    
    // input side
    .startNewBurst(grabNew6Pack),
    .botIn(botToPermute),
    .validBotPermutesIn(botToPermuteValidPermutations), // == {vABCin, vACBin, vBACin, vBCAin, vCABin, vCBAin}
    .extraDataIn(),
    .done(botPermuterRequestsNewBot),
    
    // output side
    .permutedBotValid(permutedBotValid),
    .permutedBot(permutedBot),
    .selectedPermutationOut(),
    .extraDataOut()
);


wire botAvailableForAggregatingPipeline = grabNew6Pack && batchDonePostFIFO;

// Two cycles extra delay because that is the latency of the permuter
wire botAvailableForAggregatingPipelineD;
hyperpipe #(.CYCLES(2)) permuterDelay(clk, botAvailableForAggregatingPipeline, botAvailableForAggregatingPipelineD);

wire aggregateFinished;
wire[`PCOEFF_COUNT_BITWIDTH+35-1:0] pcoeffSumFromPipeline;
wire[`PCOEFF_COUNT_BITWIDTH-1:0] pcoeffCountFromPipeline;
aggregatingPipeline computePipe (
    .clk(clk),
    .clk2x(clk2x),
    .rst(computePipeRST),
    .top(top),
    
    .isBotValid(permutedBotValid),
    .bot(permutedBot),
    .lastBotOfBatch(botAvailableForAggregatingPipelineD),
    .slowDownInput(aggregatingPipelineSlowDownInput),
    
    .resultsValid(aggregateFinished),
    .pcoeffSum(pcoeffSumFromPipeline),
    .pcoeffCount(pcoeffCountFromPipeline),
    
    .eccStatus(eccStatus)
);

wire resultsFIFOEmpty;
assign resultsAvailable = !resultsFIFOEmpty;
FIFO #(.WIDTH(`PCOEFF_COUNT_BITWIDTH+35 + `PCOEFF_COUNT_BITWIDTH), .DEPTH_LOG2(9)) resultsFIFO (
    .clk(clk),
    .rst(resultsFIFORST),
    
    // input side
    .writeEnable(aggregateFinished),
    .dataIn({pcoeffSumFromPipeline, pcoeffCountFromPipeline}),
    .full(),
    .usedw(outputFIFOUsedw),
    
    // output side
    .readEnable(grabResults),
    .dataOut({pcoeffSum, pcoeffCount}),
    .empty(resultsFIFOEmpty)
);


endmodule
