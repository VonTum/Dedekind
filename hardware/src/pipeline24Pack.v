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
