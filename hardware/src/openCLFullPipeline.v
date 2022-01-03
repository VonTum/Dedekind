`timescale 1ns / 1ps
`include "pipelineGlobals_header.v"

module openCLFullPipeline (
    input clock,
    input clock2x, // apparently this specific name gives access to a 2x speed clock. Very useful!
    input resetn,
	input ivalid, 
	input iready,
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

wire rst;
resetNormalizer rstNormalizer(clock, resetn, rst);

wire[4:0] fifoFullness;
wire[`ADDR_WIDTH-1:0] botIndex;
wire isBotValid;

wire pipelineResultValid;
wire slowThePipeline;

wire pipelineManagerIsReadyForBotIn;
assign oready = pipelineManagerIsReadyForBotIn && !slowThePipeline;

pipelineManager pipelineMngr(
    .clk(clock),
    .rst(rst),
    
    .startNewTop(startNewTop),
    .topIn(bot), // Reuse bot for topIn
    .isBotInValid(ivalid && !slowThePipeline),
    .readyForBotIn(pipelineManagerIsReadyForBotIn),
    .resultValid(pipelineResultValid),
    
    .top(top),
    .botIndex(botIndex),
    .isBotValid(isBotValid),
    .pipelineReady(fifoFullness <= 25)
);

wire[5:0] validBotPermutations;
permuteCheck6 permuteChecker (top, bot, isBotValid, validBotPermutations);


wire[63:0] pipelineResult;

// clock2x test
reg[11:0] clockReg; always @(posedge clock) if(rst) clockReg <= 0; else clockReg <= clockReg + 1;
reg[10:0] clock2xReg; always @(posedge clock2x) if(rst) clock2xReg <= 0; else clock2xReg <= clock2xReg + 1;

assign pipelineResult[63:41] = {clockReg, clock2xReg};
fullPipeline pipeline (
    .clk(clock),
    .rst(rst),
    .top(top),
    
    .bot(bot), // Represents all final 3 var swaps
    .botIndex(botIndex),
    .isBotValid(isBotValid),
    .validBotPermutations(validBotPermutations), // == {vABCin, vACBin, vBACin, vBCAin, vCABin, vCBAin}
    .fifoFullness(fifoFullness),
    .summedDataOut(pipelineResult[37:0]),
    .pcoeffCountOut(pipelineResult[40:38])
);

outputBuffer resultsBuf (
    .clk(clock),
    .rst(rst),
    
    .dataInValid(pipelineResultValid),
    .dataIn(pipelineResult),
    
    .slowInputting(slowThePipeline),
    .dataOutReady(iready),
    .dataOutValid(ovalid),
    .dataOut(summedDataPcoeffCountOut)
);

endmodule
