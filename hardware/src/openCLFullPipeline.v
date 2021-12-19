`timescale 1ns / 1ps
`include "pipelineGlobals.vh"

module openCLFullPipeline (
    input clock,
    input resetn,
	input ivalid, 
	//input iready, // not hooked up, pipeline does not handle output stalls
	output ovalid,
	output oready,
    
    // we reuse bot to set the top, to save on inputs. 
    input startNewTop,
    input[63:0] botLower, // Represents all final 3 var swaps
    input[63:0] botUpper, // Represents all final 3 var swaps
    output[63:0] summedDataPcoeffCountOut   // first 16 bits pcoeffCountOut, last 48 bits summedDataOut
);

wire[127:0] bot = {botUpper, botLower};
wire[127:0] top;

wire rst = !resetn;
wire[4:0] fifoFullness;
wire[`ADDR_WIDTH-1:0] botIndex;
wire isBotValid;

pipelineManager pipelineMngr(
    .clk(clock),
    .rst(rst),
    
    .startNewTop(startNewTop),
    .topIn(bot), // Reuse bot for topIn
    .isBotInValid(ivalid),
    .readyForBotIn(oready),
    .resultValid(ovalid),
    
    .top(top),
    .botIndex(botIndex),
    .isBotValid(isBotValid),
    .fifoAlmostFull(fifoFullness >= 25)
);

wire[5:0] validBotPermutations;
permuteCheck6 permuteChecker (top, bot, isBotValid, validBotPermutations);


fullPipeline pipeline (
    .clk(clock),
    .rst(rst),
    .top(top),
    
    .bot(bot), // Represents all final 3 var swaps
    .botIndex(botIndex),
    .isBotValid(isBotValid),
    .validBotPermutations(validBotPermutations), // == {vABCin, vACBin, vBACin, vBCAin, vCABin, vCBAin}
    .fifoFullness(fifoFullness),
    .summedDataOut(summedDataPcoeffCountOut[37:0]),
    .pcoeffCountOut(summedDataPcoeffCountOut[50:48])
);

endmodule
