`timescale 1ns / 1ps

`include "pipelineGlobals_header.v"

// sums all 120 permutations of variables 2,3,4,5,6.
module pipeline120Pack(
    input clk,
    input clk2x,
    input rst,
    output[5:0] activityMeasure, // Instrumentation wire for profiling (0-40 activity level)
    
    // Input side
    input[127:0] top,
    input[127:0] bot,
    input isBotValid,
    input batchDone,
    output wor slowDownInput,
    
    // Output side
    input grabResults,
    output wand resultsAvailable,
    output reg[`PCOEFF_COUNT_BITWIDTH+5+35-1:0] pcoeffSum,
    output reg[`PCOEFF_COUNT_BITWIDTH+5-1:0] pcoeffCount,
    output wor eccStatus
);

`include "inlineVarSwap_header.v"

// generate the permuted bots
wire[127:0] botABCDE = bot;
wire[127:0] botBACDE; `VAR_SWAP_INLINE(2,3,botABCDE, botBACDE)
wire[127:0] botCBADE; `VAR_SWAP_INLINE(2,4,botABCDE, botCBADE)
wire[127:0] botDBCAE; `VAR_SWAP_INLINE(2,5,botABCDE, botDBCAE)
wire[127:0] botEBCDA; `VAR_SWAP_INLINE(2,6,botABCDE, botEBCDA)

wire[`PCOEFF_COUNT_BITWIDTH+2+35-1:0] sums[4:0];
wire[`PCOEFF_COUNT_BITWIDTH+2-1:0] counts[4:0];

// Profiling instrumentation
wire[3:0] activitySubMeasures[4:0];
reg[4:0] activityMeasure01; always @(posedge clk) activityMeasure01 <= activitySubMeasures[0] + activitySubMeasures[1];
reg[4:0] activityMeasure23; always @(posedge clk) activityMeasure23 <= activitySubMeasures[2] + activitySubMeasures[3];
reg[3:0] activityMeasure4D; always @(posedge clk) activityMeasure4D <= activitySubMeasures[4];
reg[4:0] activityMeasure01D; always @(posedge clk) activityMeasure01D <= activityMeasure01;
reg[5:0] activityMeasure234; always @(posedge clk) activityMeasure234 <= activityMeasure23 + activityMeasure4D;
hyperpipe #(.CYCLES(3), .WIDTH(6)) activityPipe(clk, activityMeasure01D + activityMeasure234, activityMeasure);

pipeline24PackV2 p1_5(clk, clk2x, rst, activitySubMeasures[0], top, botABCDE, isBotValid, batchDone, slowDownInput, grabResults, resultsAvailable, sums[0], counts[0], eccStatus);
pipeline24PackV2 p2_5(clk, clk2x, rst, activitySubMeasures[1], top, botBACDE, isBotValid, batchDone, slowDownInput, grabResults, resultsAvailable, sums[1], counts[1], eccStatus);
pipeline24PackV2 p3_5(clk, clk2x, rst, activitySubMeasures[2], top, botCBADE, isBotValid, batchDone, slowDownInput, grabResults, resultsAvailable, sums[2], counts[2], eccStatus);
pipeline24PackV2 p4_5(clk, clk2x, rst, activitySubMeasures[3], top, botDBCAE, isBotValid, batchDone, slowDownInput, grabResults, resultsAvailable, sums[3], counts[3], eccStatus);
pipeline24PackV2 p5_5(clk, clk2x, rst, activitySubMeasures[4], top, botEBCDA, isBotValid, batchDone, slowDownInput, grabResults, resultsAvailable, sums[4], counts[4], eccStatus);

reg[`PCOEFF_COUNT_BITWIDTH+3+35-1:0] sum01; always @(posedge clk) sum01 <= sums[0] + sums[1];
reg[`PCOEFF_COUNT_BITWIDTH+3+35-1:0] sum23; always @(posedge clk) sum23 <= sums[2] + sums[3];
reg[`PCOEFF_COUNT_BITWIDTH+2+35-1:0] sum4D; always @(posedge clk) sum4D <= sums[4];
reg[`PCOEFF_COUNT_BITWIDTH+3+35-1:0] sum01D; always @(posedge clk) sum01D <= sum01;
reg[`PCOEFF_COUNT_BITWIDTH+4+35-1:0] sum234; always @(posedge clk) sum234 <= sum23 + sum4D;
always @(posedge clk) pcoeffSum <= sum01D + sum234;

reg[`PCOEFF_COUNT_BITWIDTH+3-1:0] count01; always @(posedge clk) count01 <= counts[0] + counts[1];
reg[`PCOEFF_COUNT_BITWIDTH+3-1:0] count23; always @(posedge clk) count23 <= counts[2] + counts[3];
reg[`PCOEFF_COUNT_BITWIDTH+2-1:0] count4D; always @(posedge clk) count4D <= counts[4];
reg[`PCOEFF_COUNT_BITWIDTH+3-1:0] count01D; always @(posedge clk) count01D <= count01;
reg[`PCOEFF_COUNT_BITWIDTH+4-1:0] count234; always @(posedge clk) count234 <= count23 + count4D;
always @(posedge clk) pcoeffCount <= count01D + count234;

endmodule
