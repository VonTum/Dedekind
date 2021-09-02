`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 08/30/2021 05:00:14 PM
// Design Name: 
// Module Name: dataProvider
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


module dataProvider(
    input clk,
    input canProvideData,
    output dataAvailble,
    output reg dataValid,
    output[11:0] caseIndex,
    output[127:0] top,
    output[127:0] botA, // botB = varSwap(5,6,botA)
    output[127:0] botC  // botD = varSwap(5,6,botC)
);

reg[128*3+64-1:0] dataTable[4095:0];

initial $readmemb("testData.mem", dataTable);
initial dataValid = 0;

reg[8:0] pushLoop = 0;
//always @(posedge clk) pushLoop <= pushLoop + 1;

reg[11:0] curAddr = -1;
reg isDone = 0;

wire provideData = canProvideData & (pushLoop == 0);

assign dataAvailble = !isDone;
assign caseIndex = curAddr;
wire[63:0] botSum;
assign {top, botA, botC, botSum} = dataTable[curAddr];

always @(posedge clk) begin
    if(curAddr == -1) isDone <= 1;
    dataValid <= provideData;
    if(provideData) begin
        curAddr <= curAddr + 1;
    end
end

endmodule
