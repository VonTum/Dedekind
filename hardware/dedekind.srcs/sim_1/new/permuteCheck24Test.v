`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 10/20/2021 05:05:03 PM
// Design Name: 
// Module Name: permuteCheck24Test
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


module permuteCheck24Test();

reg clk;
initial begin
    clk = 0;
    forever #1 clk = ~clk;
end


wire[127:0] top, bot;

dataProvider #("permuteCheck24TestSet7.mem", 128*2, 16384) dataProvider (
    .clk(clk),
    .requestData(1),
    .data({top, bot})
);

wire vABCD, vABDC, vACBD, vACDB, vADBC, vADCB;
wire vBACD, vBADC, vBCAD, vBCDA, vBDAC, vBDCA;
wire vCBAD, vCBDA, vCABD, vCADB, vCDBA, vCDAB;
wire vDBCA, vDBAC, vDCBA, vDCAB, vDABC, vDACB;
wire[23:0] validBotPermutationsUnderTest;

permuteCheck24 elementUnderTest(top, bot, 1, validBotPermutationsUnderTest);
assign {vABCD, vABDC, vACBD, vACDB, vADBC, vADCB, vBACD, vBADC, vBCAD, vBCDA, vBDAC, vBDCA, vCBAD, vCBDA, vCABD, vCADB, vCDBA, vCDAB, vDBCA, vDBAC, vDCBA, vDCAB, vDABC, vDACB} = validBotPermutationsUnderTest;

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

permuteCheck2 checkABCD_ABDC(top, botABCD, 1, {isABCD, isABDC});
permuteCheck2 checkBACD_BADC(top, botBACD, 1, {isBACD, isBADC});
permuteCheck2 checkCBAD_CBDA(top, botCBAD, 1, {isCBAD, isCBDA});
permuteCheck2 checkDBCA_DBAC(top, botDBCA, 1, {isDBCA, isDBAC});

permuteCheck2 checkACBD_ACDB(top, botACBD, 1, {isACBD, isACDB});
permuteCheck2 checkBCAD_BCDA(top, botBCAD, 1, {isBCAD, isBCDA});
permuteCheck2 checkCABD_CADB(top, botCABD, 1, {isCABD, isCADB});
permuteCheck2 checkDCBA_DCAB(top, botDCBA, 1, {isDCBA, isDCAB});

permuteCheck2 checkADCB_ADBC(top, botADCB, 1, {isADCB, isADBC});
permuteCheck2 checkBDCA_BDAC(top, botBDCA, 1, {isBDCA, isBDAC});
permuteCheck2 checkCDAB_CDBA(top, botCDAB, 1, {isCDAB, isCDBA});
permuteCheck2 checkDACB_DABC(top, botDACB, 1, {isDACB, isDABC});


wire[5:0] groupA = {isABCD, isABDC, isACBD, isACDB, isADBC, isADCB};
wire[5:0] groupB = {isBACD, isBADC, isBCAD, isBCDA, isBDAC, isBDCA};
wire[5:0] groupC = {isCBAD, isCBDA, isCABD, isCADB, isCDBA, isCDAB};
wire[5:0] groupD = {isDBCA, isDBAC, isDCBA, isDCAB, isDABC, isDACB};

wire[23:0] correctIsBelows = {groupA, groupB, groupC, groupD};

wire IS_FULLY_CORRECT = validBotPermutationsUnderTest == correctIsBelows;

wire[127:0] permutedBotsUsed[0:3];
assign permutedBotsUsed[0] = botABCD;
assign permutedBotsUsed[1] = botBACD;
assign permutedBotsUsed[2] = botCBAD;
assign permutedBotsUsed[3] = botDBCA;
wire[5:0] correctPermut6Groups[0:3];
/*permuteCheck6 p6A(top, botABCD, correctPermut6Groups[0]);
permuteCheck6 p6B(top, botBACD, correctPermut6Groups[1]);
permuteCheck6 p6C(top, botCBAD, correctPermut6Groups[2]);
permuteCheck6 p6D(top, botDBCA, correctPermut6Groups[3]);*/

wire correctSetSharedAll[0:3];
wire[8:0] correctSetOneTwoVarOverlaps[0:3];

generate
for(genvar i = 0; i < 4; i=i+1) begin
    permuteProduceIntermediaries6 intermediaryProducer(top, permutedBotsUsed[i], correctSetSharedAll[i], correctSetOneTwoVarOverlaps[i]);
    permutCheckProduceResults6 resultsProducer(correctSetSharedAll[i], correctSetOneTwoVarOverlaps[i], correctPermut6Groups[i]);
end
endgenerate

endmodule
