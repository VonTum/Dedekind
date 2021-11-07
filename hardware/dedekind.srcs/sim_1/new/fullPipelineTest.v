`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 08/30/2021 04:56:34 PM
// Design Name: 
// Module Name: fullPipelineTest
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


module fullPipelineTest();

reg clk;
initial begin
    clk = 0;
    forever #1 clk = ~clk;
end

wire[14:0] botIndex;
wire[127:0] top, botA, botC;
wire isBotValid;
wire full, almostFull;
wire[39:0] summedDataOut;

wire validBotA, validBotB, validBotC, validBotD;

permuteCheck2 checkAB(top, botA, isBotValid, {validBotA, validBotB});
permuteCheck2 checkCD(top, botC, isBotValid, {validBotC, validBotD});

fullPipeline elementUnderTest (
    .clk(clk),
    .top(top),
    .botA(botA), // botB = varSwap(5,6)(A)
    .botC(botC), // botD = varSwap(5,6)(C)
    .botIndex(botIndex[11:0]),
    .isBotValid(isBotValid),
    .validBotA(validBotA),
    .validBotB(validBotB),
    .validBotC(validBotC),
    .validBotD(validBotD),
    .full(full),
    .almostFull(almostFull),
    .summedDataOut(summedDataOut)
);

indexProvider #(32000) indexProvider (
    .clk(clk),
    .index(botIndex),
    .dataAvailable(isBotValid),
    .requestData(!almostFull)
);

reg[128*3+64-1:0] testSet[31999:0];
initial $readmemb("pipelineTestSet7.mem", testSet);
assign {top, botA, botC} = testSet[botIndex][128*3+64-1:64];
wire[39:0] correctBotSum = testSet[botIndex-4096][39:0];

wire IS_CORRECT = summedDataOut == correctBotSum;

endmodule
