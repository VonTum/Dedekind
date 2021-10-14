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
    
    input[127:0] top,
    input[127:0] bot,
    input[11:0] botIndex,
    input isBotValid,
    output wor full,
    output wor almostFull,
    output[39:0] summedData, // log2(24*2^35)=39.5849625007 -> 40 bits
    output[4:0] pcoeffCount // log2(24)=4.5849625007 -> 5 bits
);

// generate the permuted bots
wire[127:0] botABCD = bot;       // vs33 (no swap)
wire[127:0] botBACD; varSwap #(3,4) vs34 (botABCD, botBACD);
wire[127:0] botCBAD; varSwap #(3,5) vs35 (botABCD, botCBAD);
wire[127:0] botDBCA; varSwap #(3,6) vs36 (botABCD, botDBCA);

wire[127:0] botACBD; varSwap #(4,5) vs33_45 (botABCD, botACBD);
wire[127:0] botBCAD; varSwap #(4,5) vs34_45 (botBACD, botBCAD);
wire[127:0] botCABD; varSwap #(4,5) vs35_45 (botCBAD, botCABD);
wire[127:0] botDCBA; varSwap #(4,5) vs36_45 (botDBCA, botDCBA);

wire[127:0] botADCB; varSwap #(4,6) vs33_46 (botABCD, botADCB);
wire[127:0] botBDCA; varSwap #(4,6) vs34_46 (botBACD, botBDCA);
wire[127:0] botCDAB; varSwap #(4,6) vs35_46 (botCBAD, botCDAB);
wire[127:0] botDACB; varSwap #(4,6) vs36_46 (botDBCA, botDACB);

// 6 pipelines. Each has two as unrelated as possible bot permutations
wire[37:0] sum1; wire[2:0] count1; fullPipeline p1(clk, top, botABCD, botDCBA, botIndex, isBotValid, full, almostFull, sum1, count1); // ABCD -> DCBA  or CDAB
wire[37:0] sum2; wire[2:0] count2; fullPipeline p2(clk, top, botBACD, botCDAB, botIndex, isBotValid, full, almostFull, sum2, count2); // BACD -> DCBA  or CDAB
wire[37:0] sum3; wire[2:0] count3; fullPipeline p3(clk, top, botCBAD, botDACB, botIndex, isBotValid, full, almostFull, sum3, count3); // CBAD -> ADCB* or DACB
wire[37:0] sum4; wire[2:0] count4; fullPipeline p4(clk, top, botDBCA, botCABD, botIndex, isBotValid, full, almostFull, sum4, count4); // DBCA -> ACBD* or CABD
wire[37:0] sum5; wire[2:0] count5; fullPipeline p5(clk, top, botACBD, botBDCA, botIndex, isBotValid, full, almostFull, sum5, count5); // ACBD -> DBCA* or BDCA
wire[37:0] sum6; wire[2:0] count6; fullPipeline p6(clk, top, botADCB, botBCAD, botIndex, isBotValid, full, almostFull, sum6, count6); // ADCB -> CBAD* or BCAD

// combine outputs
wire[38:0] sum123 = sum1 + sum2 + sum3;
wire[38:0] sum456 = sum4 + sum5 + sum6;
wire[3:0] count123 = count1 + count2 + count3;
wire[3:0] count456 = count4 + count5 + count6;
assign summedData = sum123 + sum456;
assign pcoeffCount = count123 + count456;

endmodule
