`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 10/13/2021 04:45:58 PM
// Design Name: 
// Module Name: pipeline24Pack
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: 
// 
// Dependencies: 
// 
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
// 
//////////////////////////////////////////////////////////////////////////////////

// sums all 24 permutations of variables 3,4,5,6.
module pipeline24Pack(
    input clk,
    input rst,
    
    input[127:0] top,
    input[127:0] bot,
    input[11:0] botIndex,
    input isBotValid,
    output wor full,
    output wor almostFull,
    output[39:0] summedData, // log2(24*2^35)=39.5849625007 -> 40 bits
    output[4:0] pcoeffCount // log2(24)=4.5849625007 -> 5 bits
);

`include "inlineVarSwap.vh"

// generate the permuted bots
wire[127:0] botABCD = bot;       // vs33 (no swap)
wire[127:0] botBACD; `VAR_SWAP_INLINE(3,4,botABCD, botBACD)// varSwap #(3,4) vs34 (botABCD, botBACD);
wire[127:0] botCBAD; `VAR_SWAP_INLINE(3,5,botABCD, botCBAD)// varSwap #(3,5) vs35 (botABCD, botCBAD);
wire[127:0] botDBCA; `VAR_SWAP_INLINE(3,6,botABCD, botDBCA)// varSwap #(3,6) vs36 (botABCD, botDBCA);

wire[127:0] botACBD; `VAR_SWAP_INLINE(4,5,botABCD, botACBD)// varSwap #(4,5) vs33_45 (botABCD, botACBD);
wire[127:0] botBCAD; `VAR_SWAP_INLINE(4,5,botBACD, botBCAD)// varSwap #(4,5) vs34_45 (botBACD, botBCAD);
wire[127:0] botCABD; `VAR_SWAP_INLINE(4,5,botCBAD, botCABD)// varSwap #(4,5) vs35_45 (botCBAD, botCABD);
wire[127:0] botDCBA; `VAR_SWAP_INLINE(4,5,botDBCA, botDCBA)// varSwap #(4,5) vs36_45 (botDBCA, botDCBA);

wire[127:0] botADCB; `VAR_SWAP_INLINE(4,6,botABCD, botADCB)// varSwap #(4,6) vs33_46 (botABCD, botADCB);
wire[127:0] botBDCA; `VAR_SWAP_INLINE(4,6,botBACD, botBDCA)// varSwap #(4,6) vs34_46 (botBACD, botBDCA);
wire[127:0] botCDAB; `VAR_SWAP_INLINE(4,6,botCBAD, botCDAB)// varSwap #(4,6) vs35_46 (botCBAD, botCDAB);
wire[127:0] botDACB; `VAR_SWAP_INLINE(4,6,botDBCA, botDACB)// varSwap #(4,6) vs36_46 (botDBCA, botDACB);

wire vABCD, vABDC, vACBD, vACDB, vADBC, vADCB;
wire vBACD, vBADC, vBCAD, vBCDA, vBDAC, vBDCA;
wire vCBAD, vCBDA, vCABD, vCADB, vCDBA, vCDAB;
wire vDBCA, vDBAC, vDCBA, vDCAB, vDABC, vDACB;

permuteCheck24 permutChecker (
    .top(top),
    .bot(bot),
    .isBotValid(isBotValid),
    .validBotPermutations({
        vABCD, vABDC, vACBD, vACDB, vADBC, vADCB, 
        vBACD, vBADC, vBCAD, vBCDA, vBDAC, vBDCA, 
        vCBAD, vCBDA, vCABD, vCADB, vCDBA, vCDAB, 
        vDBCA, vDBAC, vDCBA, vDCAB, vDABC, vDACB
    })
);


wire[5:0] fullWires;
wire[5:0] almostFullWires;

// 6 pipelines. Each has two as unrelated as possible bot permutations
wire[37:0] sum1; wire[2:0] count1; fullPipeline4 p1(clk, rst, top, botABCD, botDCBA, botIndex, isBotValid, vABCD, vABDC, vDCBA, vDCAB, fullWires[0], almostFullWires[0], sum1, count1); // ABCD -> DCBA  or CDAB
wire[37:0] sum2; wire[2:0] count2; fullPipeline4 p2(clk, rst, top, botBACD, botCDAB, botIndex, isBotValid, vBACD, vBADC, vCDAB, vCDBA, fullWires[1], almostFullWires[1], sum2, count2); // BACD -> DCBA  or CDAB
wire[37:0] sum3; wire[2:0] count3; fullPipeline4 p3(clk, rst, top, botCBAD, botDACB, botIndex, isBotValid, vCBAD, vCBDA, vDACB, vDABC, fullWires[2], almostFullWires[2], sum3, count3); // CBAD -> ADCB* or DACB
wire[37:0] sum4; wire[2:0] count4; fullPipeline4 p4(clk, rst, top, botDBCA, botCABD, botIndex, isBotValid, vDBCA, vDBAC, vCABD, vCADB, fullWires[3], almostFullWires[3], sum4, count4); // DBCA -> ACBD* or CABD
wire[37:0] sum5; wire[2:0] count5; fullPipeline4 p5(clk, rst, top, botACBD, botBDCA, botIndex, isBotValid, vACBD, vACDB, vBDCA, vBDAC, fullWires[4], almostFullWires[4], sum5, count5); // ACBD -> DBCA* or BDCA
wire[37:0] sum6; wire[2:0] count6; fullPipeline4 p6(clk, rst, top, botADCB, botBCAD, botIndex, isBotValid, vADCB, vADBC, vBCAD, vBCDA, fullWires[5], almostFullWires[5], sum6, count6); // ADCB -> CBAD* or BCAD

// combine outputs
wire[38:0] sum123 = sum1 + sum2 + sum3;
wire[38:0] sum456 = sum4 + sum5 + sum6;
wire[3:0] count123 = count1 + count2 + count3;
wire[3:0] count456 = count4 + count5 + count6;
assign summedData = sum123 + sum456;
assign pcoeffCount = count123 + count456;

assign full = |fullWires;
assign almostFull = |almostFullWires;

endmodule
