`timescale 1ns / 1ps
`include "pipelineGlobals_header.v"

// sums all 24 permutations of variables 3,4,5,6.
module pipeline24Pack(
    input clk,
    input rst,
    
    input[127:0] top,
    input[127:0] bot,
    input[`ADDR_WIDTH-1:0] botIndex,
    input isBotValid,
    input[23:0] validBotPermutations, // == {permutesABCD, permutesBACD, permutesCBAD, permutesDBCA}
    output reg[4:0] maxFullness,
    output[39:0] summedData, // log2(24*2^35)=39.5849625007 -> 40 bits
    output[4:0] pcoeffCount // log2(24)=4.5849625007 -> 5 bits
);

`include "inlineVarSwap_header.v"

// generate the permuted bots
wire[127:0] botABCD = bot;       // vs33 (no swap)
wire[127:0] botBACD; `VAR_SWAP_INLINE(3,4,botABCD, botBACD)// varSwap #(3,4) vs34 (botABCD, botBACD);
wire[127:0] botCBAD; `VAR_SWAP_INLINE(3,5,botABCD, botCBAD)// varSwap #(3,5) vs35 (botABCD, botCBAD);
wire[127:0] botDBCA; `VAR_SWAP_INLINE(3,6,botABCD, botDBCA)// varSwap #(3,6) vs36 (botABCD, botDBCA);

wire[5:0] permutesABCD;
wire[5:0] permutesBACD;
wire[5:0] permutesCBAD;
wire[5:0] permutesDBCA;

assign {permutesABCD, permutesBACD, permutesCBAD, permutesDBCA} = validBotPermutations;

wire[37:0] sums[3:0];
wire[2:0] counts[3:0];
wire[4:0] fullnesses[3:0];

fullPipeline p0(clk, rst, top, botABCD, botIndex, isBotValid, permutesABCD, fullnesses[0], sums[0], counts[0]);
fullPipeline p1(clk, rst, top, botBACD, botIndex, isBotValid, permutesBACD, fullnesses[1], sums[1], counts[1]);
fullPipeline p2(clk, rst, top, botCBAD, botIndex, isBotValid, permutesCBAD, fullnesses[2], sums[2], counts[2]);
fullPipeline p3(clk, rst, top, botDBCA, botIndex, isBotValid, permutesDBCA, fullnesses[3], sums[3], counts[3]);

// combine outputs
reg[38:0] sum01; always @(posedge clk) sum01 <= sums[0] + sums[1];
reg[38:0] sum23; always @(posedge clk) sum23 <= sums[2] + sums[3];
reg[39:0] fullSum; always @(posedge clk) fullSum <= sum01 + sum23;
reg[4:0] fullCount; always @(posedge clk) fullCount <= (counts[0] + counts[1]) + (counts[2] + counts[3]);
hyperpipe #(.CYCLES(`OUTPUT_READ_LATENCY_24PACK-2), .WIDTH(40)) sumPipe(clk, fullSum, summedData);
hyperpipe #(.CYCLES(`OUTPUT_READ_LATENCY_24PACK-1), .WIDTH(5)) countPipe(clk, fullCount, pcoeffCount);

reg[4:0] maxFullness01; always @(posedge clk) maxFullness01 <= fullnesses[0] > fullnesses[1] ? fullnesses[0] : fullnesses[1];
reg[4:0] maxFullness23; always @(posedge clk) maxFullness23 <= fullnesses[2] > fullnesses[3] ? fullnesses[2] : fullnesses[3];
always @(posedge clk) maxFullness <= maxFullness01 > maxFullness23 ? maxFullness01 : maxFullness23;

endmodule



//`timescale 1ns / 1ps
//`include "pipelineGlobals_header.v"

// sums all 24 permutations of variables 3,4,5,6.
module pipeline24PackV2WithFIFO (
    input clk,
    input clk2x,
    input rst,
    output[3:0] activityMeasure, // Instrumentation wire for profiling (0-8 activity level)
    
    // Input side
    input[127:0] top,
    input[127:0] bot,
    input isBotValid,
    input batchDone,
    output slowDownInput,
    
    // Output side
    input grabResults,
    output resultsAvailable,
    output[`PCOEFF_COUNT_BITWIDTH+2+35-1:0] pcoeffSum,
    output[`PCOEFF_COUNT_BITWIDTH+2-1:0] pcoeffCount,
    
    output wor eccStatus
);

wire[23:0] validBotPermutationsD;
permuteCheck24Pipelined permuteChecker(clk, top, bot, isBotValid, validBotPermutationsD);
reg[127:0] botD; always @(posedge clk) botD <= bot;
reg batchDoneD; always @(posedge clk) batchDoneD <= batchDone;

reg[128+24+1-1:0] dataToFIFO24; always @(posedge clk) dataToFIFO24 <= {botD, validBotPermutationsD, batchDoneD};
reg writeToFIFO24; always @(posedge clk) writeToFIFO24 <= |validBotPermutationsD || batchDoneD;


wire[127:0] botFromFIFO;
wire[23:0] validBotPermutationsFromFIFO;
wire batchDoneFromFIFO;

wire[8:0] inputFIFOUsedW;
hyperpipe #(.CYCLES(5)) slowDownInputPipe(clk, inputFIFOUsedW > 400, slowDownInput);
wire pipelinesRequestSlowDown;
wire fifo24DataOutValid;

(* dont_merge *) reg inputFifoRST; always @(posedge clk) inputFifoRST <= rst;
FastFIFO #(.WIDTH(128+24+1), .DEPTH_LOG2(9), .IS_MLAB(0)) fifo24 (
    .clk(clk),
    .rst(inputFifoRST),
    
    .writeEnable(writeToFIFO24),
    .dataIn(dataToFIFO24),
    .usedw(inputFIFOUsedW),
    
    .readRequest(!pipelinesRequestSlowDown),
    .dataOut({botFromFIFO, validBotPermutationsFromFIFO, batchDoneFromFIFO}),
    .dataOutValid(fifo24DataOutValid),
    .empty(),
    .eccStatus(eccStatus)
);

(* dont_merge *) reg pipelineRST; always @(posedge clk) pipelineRST <= rst;
pipeline24PackV2 pipeline (
    .clk(clk),
    .clk2x(clk2x),
    .rst(pipelineRST),
    .activityMeasure(activityMeasure),
    .top(top),
    
    .bot(botFromFIFO),
    .writeData(fifo24DataOutValid),
    .validBotPermutations(validBotPermutationsFromFIFO),
    .batchDone(batchDoneFromFIFO),
    .slowDownInput(pipelinesRequestSlowDown),
    
    // Output side
    .grabResults(grabResults),
    .resultsAvailable(resultsAvailable),
    .pcoeffSum(pcoeffSum),
    .pcoeffCount(pcoeffCount),
    
    .eccStatus(eccStatus)
);

endmodule

// sums all 24 permutations of variables 3,4,5,6.
module pipeline24PackV2 (
    input clk,
    input clk2x,
    input rst,
    output[3:0] activityMeasure, // Instrumentation wire for profiling (0-8 activity level)
    
    // Input side
    input[127:0] top,
    input[127:0] bot,
    input botValid,
    input batchDone,
    output slowDownInput,
    
    // Output side
    input grabResults,
    output reg resultsAvailable,
    output reg[`PCOEFF_COUNT_BITWIDTH+2+35-1:0] pcoeffSum,
    output reg[`PCOEFF_COUNT_BITWIDTH+2-1:0] pcoeffCount,
    
    output reg eccStatus
);

`include "inlineVarSwap_header.v"

// Input side

// generate the permuted bots
reg[127:0] botD; always @(posedge clk) botD <= bot;
reg batchDoneD; always @(posedge clk) batchDoneD <= batchDone;

wire[127:0] botABCD = botD;       // vs33 (no swap)
wire[127:0] botBACD; `VAR_SWAP_INLINE(3,4,botABCD, botBACD)// varSwap #(3,4) vs34 (botABCD, botBACD);
wire[127:0] botCBAD; `VAR_SWAP_INLINE(3,5,botABCD, botCBAD)// varSwap #(3,5) vs35 (botABCD, botCBAD);
wire[127:0] botDBCA; `VAR_SWAP_INLINE(3,6,botABCD, botDBCA)// varSwap #(3,6) vs36 (botABCD, botDBCA);

wire[5:0] permutesABCD; // All delayed by 1 clock cycle
wire[5:0] permutesBACD;
wire[5:0] permutesCBAD;
wire[5:0] permutesDBCA;
permuteCheck24Pipelined permuteChecker(clk, top, bot, botValid, {permutesABCD, permutesBACD, permutesCBAD, permutesDBCA});


// Output side
wire[`PCOEFF_COUNT_BITWIDTH+35-1:0] sums[3:0];
wire[`PCOEFF_COUNT_BITWIDTH-1:0] counts[3:0];

wand resultsAvailableWAND;
always @(posedge clk) resultsAvailable <= resultsAvailableWAND;
(* dont_merge *) reg grabResultsD; always @(posedge clk) grabResultsD <= grabResults;

wor slowDownInputWOR;
hyperpipe #(.CYCLES(5)) slowDownInputPipe(clk, slowDownInputWOR, slowDownInput);
wor eccStatusWOR; always @(posedge clk) eccStatus <= eccStatusWOR;

// Profiling wires
wire[1:0] activityMeasures[3:0];
reg[2:0] activityMeasure01; always @(posedge clk) activityMeasure01 <= activityMeasures[0] + activityMeasures[1];
reg[2:0] activityMeasure23; always @(posedge clk) activityMeasure23 <= activityMeasures[2] + activityMeasures[3];
reg[3:0] activityMeasureSum; always @(posedge clk) activityMeasureSum <= activityMeasure01 + activityMeasure23;
hyperpipe #(.CYCLES(3), .WIDTH(4)) activityPipe(clk, activityMeasureSum, activityMeasure);

aggregatingPermutePipeline p1_4(clk, clk2x, rst, activityMeasures[0], top, botABCD, permutesABCD, batchDoneD, slowDownInputWOR, grabResultsD, resultsAvailableWAND, sums[0], counts[0], eccStatusWOR);
aggregatingPermutePipeline p2_4(clk, clk2x, rst, activityMeasures[1], top, botBACD, permutesBACD, batchDoneD, slowDownInputWOR, grabResultsD, resultsAvailableWAND, sums[1], counts[1], eccStatusWOR);
aggregatingPermutePipeline p3_4(clk, clk2x, rst, activityMeasures[2], top, botCBAD, permutesCBAD, batchDoneD, slowDownInputWOR, grabResultsD, resultsAvailableWAND, sums[2], counts[2], eccStatusWOR);
aggregatingPermutePipeline p4_4(clk, clk2x, rst, activityMeasures[3], top, botDBCA, permutesDBCA, batchDoneD, slowDownInputWOR, grabResultsD, resultsAvailableWAND, sums[3], counts[3], eccStatusWOR);

// combine outputs
reg[`PCOEFF_COUNT_BITWIDTH+35+1-1:0] sum01; always @(posedge clk) sum01 <= sums[0] + sums[1];
reg[`PCOEFF_COUNT_BITWIDTH+35+1-1:0] sum23; always @(posedge clk) sum23 <= sums[2] + sums[3];
always @(posedge clk) pcoeffSum <= sum01 + sum23;
reg[`PCOEFF_COUNT_BITWIDTH+1-1:0] count01; always @(posedge clk) count01 <= counts[0] + counts[1];
reg[`PCOEFF_COUNT_BITWIDTH+1-1:0] count23; always @(posedge clk) count23 <= counts[2] + counts[3];
always @(posedge clk) pcoeffCount <= count01 + count23;

endmodule



