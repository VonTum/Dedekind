`timescale 1ns / 1ps
`include "pipelineGlobals_header.v"

module openCLFullPipeline (
    input clock,
    input rst,
    input ivalid, 
    input iready,
    output ovalid,
    output oready,
    
    // Remains constant for the duration of a run
    input[127:0] top,
    
    // we reuse bot to set the top, to save on inputs. 
    input[127:0] bot, // Represents all final 3 var swaps
    output[63:0] summedDataPcoeffCountOut   // first 16 bits pcoeffCountOut, last 48 bits summedDataOut
);


wire rstLocal; // Manual reset tree, can't use constraints to have it generate it for me. 
hyperpipe #(.CYCLES(2)) rstPipe(clock, rst, rstLocal);

wire[4:0] fifoFullness;
wire[`ADDR_WIDTH-1:0] botIndex;
wire isBotValid;

wire pipelineResultValid;
wire slowThePipeline;

wire pipelineManagerIsReadyForBotIn;
assign oready = pipelineManagerIsReadyForBotIn && !slowThePipeline;

pipelineManager pipelineMngr(
    .clk(clock),
    .rst(rstLocal),
    
    .isBotInValid(ivalid && !slowThePipeline),
    .readyForBotIn(pipelineManagerIsReadyForBotIn),
    .resultValid(pipelineResultValid),
    
    .botIndex(botIndex),
    .isBotValid(isBotValid),
    .pipelineReady(fifoFullness <= 25)
);

wire[23:0] validBotPermutations;
permuteCheck24 permuteChecker (top, bot, isBotValid, validBotPermutations);


wire[63:0] pipelineResult;

// clock count
reg[18:0] clockReg; always @(posedge clock) if(rstLocal) clockReg <= 0; else clockReg <= clockReg + 1;
wire[18:0] clockRegLax;
hyperpipe #(.CYCLES(2), .WIDTH(19)) clockRegPipe(clock, clockReg, clockRegLax);
assign pipelineResult[63:45] = clockRegLax;


pipeline24Pack pipeline (
    .clk(clock),
    .rst(rstLocal),
    .top(top),
    
    .bot(bot), // Represents all final 3 var swaps
    .botIndex(botIndex),
    .isBotValid(isBotValid),
    .validBotPermutations(validBotPermutations), // == {vABCin, vACBin, vBACin, vBCAin, vCABin, vCBAin}
    .maxFullness(fifoFullness),
    .summedData(pipelineResult[39:0]),
    .pcoeffCount(pipelineResult[44:40])
);

outputBuffer resultsBuf (
    .clk(clock),
    .rst(rstLocal),
    
    .dataInValid(pipelineResultValid),
    .dataIn(pipelineResult),
    
    .slowInputting(slowThePipeline),
    .dataOutReady(iready),
    .dataOutValid(ovalid),
    .dataOut(summedDataPcoeffCountOut)
);

endmodule
