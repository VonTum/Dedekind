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


module permuteCheck6Test();

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

wire[5:0] validBotPermutationsUnderTest;

permuteCheck6 elementUnderTest(top, bot, validBotPermutationsUnderTest);


`include "inlineVarSwap.vh"

// generate the permuted bots
wire[127:0] botABC = bot;
wire[127:0] botBAC; `VAR_SWAP_INLINE(4,5,botABC, botBAC)
wire[127:0] botCBA; `VAR_SWAP_INLINE(4,6,botABC, botCBA)

checkIsBelowWithSwap checkABC_ACB(top, botABC, isABC, isACB);
checkIsBelowWithSwap checkBAC_BCA(top, botBAC, isBAC, isBCA);
checkIsBelowWithSwap checkCBA_CAB(top, botCBA, isCBA, isCAB);

wire[5:0] correctValidBotPermutations = {isABC, isACB, isBAC, isBCA, isCAB, isCBA};

wire[5:0] isCorrect;
generate
    for(genvar i = 0; i < 6; i=i+1) assign isCorrect[i] = validBotPermutationsUnderTest[i] == correctValidBotPermutations[i];
endgenerate

endmodule
