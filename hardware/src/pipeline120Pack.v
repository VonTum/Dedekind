`timescale 1ns / 1ps

// sums all 120 permutations of variables 2,3,4,5,6.
module pipeline120Pack(
    input clk,
    input rst,
    
    input[127:0] top,
    input[127:0] bot,
    input[11:0] botIndex,
    input isBotValid,
    output wor full,
    output wor almostFull,
    output[41:0] summedData, // log2(120*2^35)=41.9068905956 -> 42 bits
    output[6:0] pcoeffCount // log2(120)=6.90689059561 -> 7 bits
);

`include "inlineVarSwap_header.v"

// generate the permuted bots
wire[127:0] botABCDE = bot;
wire[127:0] botBACDE; `VAR_SWAP_INLINE(2,3,botABCDE, botBACDE)
wire[127:0] botCBADE; `VAR_SWAP_INLINE(2,4,botABCDE, botCBADE)
wire[127:0] botDBCAE; `VAR_SWAP_INLINE(2,5,botABCDE, botDBCAE)
wire[127:0] botEBCDA; `VAR_SWAP_INLINE(2,6,botABCDE, botEBCDA)

wire[4:0] fullWires;
wire[4:0] almostFullWires;

wire[39:0] sum1; wire[4:0] count1; pipeline24Pack p1(clk, rst, top, botABCDE, botIndex, isBotValid, fullWires[0], almostFullWires[0], sum1, count1);
wire[39:0] sum2; wire[4:0] count2; pipeline24Pack p2(clk, rst, top, botBACDE, botIndex, isBotValid, fullWires[1], almostFullWires[1], sum2, count2);
wire[39:0] sum3; wire[4:0] count3; pipeline24Pack p3(clk, rst, top, botCBADE, botIndex, isBotValid, fullWires[2], almostFullWires[2], sum3, count3);
wire[39:0] sum4; wire[4:0] count4; pipeline24Pack p4(clk, rst, top, botDBCAE, botIndex, isBotValid, fullWires[3], almostFullWires[3], sum4, count4);
wire[39:0] sum5; wire[4:0] count5; pipeline24Pack p5(clk, rst, top, botEBCDA, botIndex, isBotValid, fullWires[4], almostFullWires[4], sum5, count5);

// combine outputs
assign summedData = (sum1 + sum2) + sum3 + (sum4 + sum5);
assign pcoeffCount = (count1 + count2) + count3 + (count4 + count5);

assign full = |fullWires;
assign almostFull = |almostFullWires;

endmodule
