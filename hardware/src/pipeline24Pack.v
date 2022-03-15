`timescale 1ns / 1ps
`include "pipelineGlobals_header.v"

// sums all 24 permutations of variables 3,4,5,6.
module pipeline24PackV2 (
    input clk,
    input clk2x,
    input rst,
    output[3:0] activityMeasure, // Instrumentation wire for profiling (0-8 activity level)
    
    // Input side
    input[127:0] top,
    
    input[128*`NUMBER_OF_PERMUTATORS-1:0] bots,
    input[`NUMBER_OF_PERMUTATORS-1:0] botsValid,
    input[`NUMBER_OF_PERMUTATORS-1:0] batchesDone,
    output[`NUMBER_OF_PERMUTATORS-1:0] slowDownInput,
    
    // Output side
    input grabResults,
    output reg resultsAvailable,
    output reg[`PCOEFF_COUNT_BITWIDTH+2+35-1:0] pcoeffSum,
    output reg[`PCOEFF_COUNT_BITWIDTH+2-1:0] pcoeffCount,
    
    output reg eccStatus
);

`include "inlineVarSwap_header.v"

// Input side
// Add extra stage to top to improve routing, latency is irrelevant anyway
(* dont_merge *) reg[127:0] topD; always @(posedge clk) topD <= top;

// generate the permuted bots
`define PERMUTE_CHECK_LATENCY 3
wire[128*`NUMBER_OF_PERMUTATORS-1:0] botsD;
wire[`NUMBER_OF_PERMUTATORS-1:0] batchesDoneD;
hyperpipe #(.CYCLES(`PERMUTE_CHECK_LATENCY), .WIDTH(128*`NUMBER_OF_PERMUTATORS)) botsPipe(clk2x, bots, botsD);
hyperpipe #(.CYCLES(`PERMUTE_CHECK_LATENCY), .WIDTH(`NUMBER_OF_PERMUTATORS)) batchesPipe(clk2x, batchesDone, batchesDoneD);

wire[128*`NUMBER_OF_PERMUTATORS-1:0] botsABCD = botsD;       // vs33 (no swap)
wire[128*`NUMBER_OF_PERMUTATORS-1:0] botsBACD;
wire[128*`NUMBER_OF_PERMUTATORS-1:0] botsCBAD;
wire[128*`NUMBER_OF_PERMUTATORS-1:0] botsDBCA;

genvar i;
generate
for(i = 0; i < `NUMBER_OF_PERMUTATORS; i = i + 1) begin
    wire[127:0] curInputBotToPermute = botsABCD[128*i +: 128];
    `VAR_SWAP_INLINE_EXTRA_WIRES_WITHIN_GENERATE(3, 4, curInputBotToPermute, botsBACD[128*i +: 128], outWireBACD)// varSwap #(3,4) vs34 (botsABCD, botsBACD);
    `VAR_SWAP_INLINE_EXTRA_WIRES_WITHIN_GENERATE(3, 5, curInputBotToPermute, botsCBAD[128*i +: 128], outWireCBAD)// varSwap #(3,5) vs35 (botsABCD, botsCBAD);
    `VAR_SWAP_INLINE_EXTRA_WIRES_WITHIN_GENERATE(3, 6, curInputBotToPermute, botsDBCA[128*i +: 128], outWireDBCA)// varSwap #(3,6) vs36 (botsABCD, botsDBCA);
end
endgenerate

wire[6*`NUMBER_OF_PERMUTATORS-1:0] permutesABCD; // All delayed by 1 clock cycle
wire[6*`NUMBER_OF_PERMUTATORS-1:0] permutesBACD;
wire[6*`NUMBER_OF_PERMUTATORS-1:0] permutesCBAD;
wire[6*`NUMBER_OF_PERMUTATORS-1:0] permutesDBCA;

generate
for(i = 0; i < `NUMBER_OF_PERMUTATORS; i = i + 1) begin
    permuteCheck24Pipelined permuteChecker(clk2x, topD, bots[i*128 +: 128], botsValid[i], {permutesABCD[i*6 +: 6], permutesBACD[i*6 +: 6], permutesCBAD[i*6 +: 6], permutesDBCA[i*6 +: 6]});
end
endgenerate

// Output side
wire[`PCOEFF_COUNT_BITWIDTH+35-1:0] sums[3:0];
wire[`PCOEFF_COUNT_BITWIDTH-1:0] counts[3:0];

wand resultsAvailableWAND;
always @(posedge clk) resultsAvailable <= resultsAvailableWAND;
(* dont_merge *) reg grabResultsD; always @(posedge clk) grabResultsD <= grabResults;

wor[`NUMBER_OF_PERMUTATORS-1:0] slowDownInputWOR;
hyperpipe #(.CYCLES(5), .WIDTH(`NUMBER_OF_PERMUTATORS)) slowDownInputPipe(clk2x, slowDownInputWOR, slowDownInput);
wor eccStatusWOR; always @(posedge clk) eccStatus <= eccStatusWOR;

// Profiling wires
wire[1:0] activityMeasures[3:0];
reg[2:0] activityMeasure01; always @(posedge clk) activityMeasure01 <= activityMeasures[0] + activityMeasures[1];
reg[2:0] activityMeasure23; always @(posedge clk) activityMeasure23 <= activityMeasures[2] + activityMeasures[3];
reg[3:0] activityMeasureSum; always @(posedge clk) activityMeasureSum <= activityMeasure01 + activityMeasure23;
hyperpipe #(.CYCLES(3), .WIDTH(4)) activityPipe(clk, activityMeasureSum, activityMeasure);

aggregatingPermutePipeline p1_4(clk, clk2x, rst, activityMeasures[0], topD, botsABCD, permutesABCD, batchesDoneD, slowDownInputWOR, grabResultsD, resultsAvailableWAND, sums[0], counts[0], eccStatusWOR);
aggregatingPermutePipeline p2_4(clk, clk2x, rst, activityMeasures[1], topD, botsBACD, permutesBACD, batchesDoneD, slowDownInputWOR, grabResultsD, resultsAvailableWAND, sums[1], counts[1], eccStatusWOR);
aggregatingPermutePipeline p3_4(clk, clk2x, rst, activityMeasures[2], topD, botsCBAD, permutesCBAD, batchesDoneD, slowDownInputWOR, grabResultsD, resultsAvailableWAND, sums[2], counts[2], eccStatusWOR);
aggregatingPermutePipeline p4_4(clk, clk2x, rst, activityMeasures[3], topD, botsDBCA, permutesDBCA, batchesDoneD, slowDownInputWOR, grabResultsD, resultsAvailableWAND, sums[3], counts[3], eccStatusWOR);

// combine outputs
reg[`PCOEFF_COUNT_BITWIDTH+35+1-1:0] sum01; always @(posedge clk) sum01 <= sums[0] + sums[1];
reg[`PCOEFF_COUNT_BITWIDTH+35+1-1:0] sum23; always @(posedge clk) sum23 <= sums[2] + sums[3];
always @(posedge clk) pcoeffSum <= sum01 + sum23;
reg[`PCOEFF_COUNT_BITWIDTH+1-1:0] count01; always @(posedge clk) count01 <= counts[0] + counts[1];
reg[`PCOEFF_COUNT_BITWIDTH+1-1:0] count23; always @(posedge clk) count23 <= counts[2] + counts[3];
always @(posedge clk) pcoeffCount <= count01 + count23;

endmodule



