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
    output[`SUMMED_DATA_WIDTH-1:0] summedDataOut,
    output[`PCOEFF_COUNT_WIDTH-1:0] pcoeffCountOut
);


wire rstLocal; // Manual reset tree, can't use constraints to have it generate it for me. 
hyperpipe #(.CYCLES(2)) rstPipe(clock, rst, rstLocal);

wire[4:0] fifoFullness;
wire[`ADDR_WIDTH-1:0] botIndex;
wire isBotValid;

wire pipelineResultValid;
reg slowThePipeline;

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

wire[`SUMMED_DATA_WIDTH-1:0] summedData;
wire[`PCOEFF_COUNT_WIDTH-1:0] pcoeffCount;

pipeline24Pack pipeline (
    .clk(clock),
    .rst(rstLocal),
    .top(top),
    
    .bot(bot), // Represents all final 3 var swaps
    .botIndex(botIndex),
    .isBotValid(isBotValid),
    .validBotPermutations(validBotPermutations), // == {vABCin, vACBin, vBACin, vBCAin, vCABin, vCBAin}
    .maxFullness(fifoFullness),
    .summedData(summedData),
    .pcoeffCount(pcoeffCount)
);

// Output buffer
// Space for 8192 elements
`define FIFO_DEPTH_BITS 13
wire[`FIFO_DEPTH_BITS-1:0] outputFifoFullness;

always @(posedge clock) slowThePipeline <= outputFifoFullness >= 1000; // want to leave >7000 margin so the pipeline can empty out entirely. 

FastFIFO #(.WIDTH(`SUMMED_DATA_WIDTH + `PCOEFF_COUNT_WIDTH), .DEPTH_LOG2(`FIFO_DEPTH_BITS), .IS_MLAB(0), .READ_ADDR_STAGES(1)) resultsBuffer (
    .clk(clock),
    .rst(rstLocal),
	
    // input side
    .writeEnable(pipelineResultValid),
    .dataIn({summedData, pcoeffCount}),
    .usedw(outputFifoFullness),
    
    // output side
    .readRequest(iready),
    .dataOut({summedDataOut, pcoeffCountOut}),
    .dataOutValid(ovalid)
);

endmodule
