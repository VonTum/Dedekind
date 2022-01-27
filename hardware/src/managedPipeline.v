`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 01/27/2022 12:13:29 PM
// Design Name: 
// Module Name: managedPipeline
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


module managedPipeline(
    input clk,
    input rst,
    
    input[127:0] top,
    
    // Input side
    input isBotValid,
    input[23:0] validBotPermutations,
    input[127:0] bot, 
    output slowThePipeline,
    
    // Output side
    output pipelineResultValid,
    output[`SUMMED_DATA_WIDTH-1:0] summedData,
    output[`PCOEFF_COUNT_WIDTH-1:0] pcoeffCount
);

wire[4:0] fifoFullness;
wire[`ADDR_WIDTH-1:0] botIndex;

wire pipelineManagerIsReadyForBotIn;
assign oready = pipelineManagerIsReadyForBotIn && !slowThePipeline;

pipelineManager pipelineMngr(
    .clk(clk),
    .rst(rst),
    
    .isBotInValid(ivalid && !slowThePipeline),
    .readyForBotIn(pipelineManagerIsReadyForBotIn),
    .resultValid(pipelineResultValid),
    
    .botIndex(botIndex),
    .isBotValid(isBotValid),
    .pipelineReady(fifoFullness <= 25)
);

pipeline24Pack pipeline (
    .clk(clk),
    .rst(rst),
    .top(top),
    
    .bot(bot), // Represents all final 3 var swaps
    .botIndex(botIndex),
    .isBotValid(isBotValid),
    .validBotPermutations(validBotPermutations), // == {vABCin, vACBin, vBACin, vBCAin, vCABin, vCBAin}
    .maxFullness(fifoFullness),
    .summedData(summedData),
    .pcoeffCount(pcoeffCount)
);
    
endmodule
