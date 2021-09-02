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

wire[11:0] botIndex;
wire[127:0] top, botA, botC;
wire isBotValid;
wire full, almostFull;
wire[39:0] summedDataOut;

fullPipeline elementUnderTest (
    .busClk(clk),
    .coreClk(clk),
    
    .top(top),
    .botA(botA), // botB = varSwap(5,6)(A)
    .botC(botC), // botD = varSwap(5,6)(C)
    .botIndex(botIndex),
    .isBotValid(isBotValid),
    .full(full),
    .almostFull(almostFull),
    .summedDataOut(summedDataOut)
);

dataProvider dataProvider (
    .clk(clk),
    .canProvideData(!almostFull),
    .dataAvailble(),
    .dataValid(isBotValid),
    .caseIndex(botIndex),
    .top(top),
    .botA(botA), // botB = varSwap(5,6,botA)
    .botC(botC)  // botD = varSwap(5,6,botC)
);

endmodule
