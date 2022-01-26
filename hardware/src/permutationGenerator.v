`timescale 1ns / 1ps

`include "pipelineGlobals_header.v"

module permutationGenerator67 (
    input clk,
    input rst,
    
    input[127:0] inputBot,
    input writeInputBot,
    output hasSpaceForNextBot,
    
    output[127:0] outputBot,
    output outputBotValid,
    output reg botSeriesFinished
);

`include "inlineVarSwap_header.v"

reg[127:0] nextBot;
reg nextBotValid;
assign hasSpaceForNextBot = !nextBotValid;

reg[2:0] permutState7 = 0;
reg[2:0] permutState6 = 0;

reg[127:0] currentlyPermuting;
reg currentlyPermutingValid;

always @(posedge clk) begin
    if(rst) begin
        nextBotValid <= 0;
    end else begin
        if(writeInputBot) begin
            nextBotValid <= 1;
            nextBot <= inputBot;
        end else if(permutState7 >= 6 && permutState6 >= 5) begin
            nextBotValid <= 0;
        end
    end
end

always @(posedge clk) begin
    if(permutState7 >= 6) begin
        permutState7 <= 0;
        if(permutState6 >= 5) begin
            permutState6 <= 0;
            
            currentlyPermutingValid <= nextBotValid;
            currentlyPermuting <= nextBot;
            
            nextBotValid <= 0;
            
            botSeriesFinished <= currentlyPermutingValid;
        end else begin
            permutState6 <= permutState6 + 1;
            botSeriesFinished <= 0;
        end
    end else begin
        permutState7 <= permutState7 + 1;
        botSeriesFinished <= 0;
    end
end

wire[127:0] firstStagePermutWires[6:0];
genvar i;
generate
for(i = 0; i < 7; i = i + 1) begin
    `VAR_SWAP_INLINE_WITHIN_GENERATE(0, i, currentlyPermuting, firstStagePermutWires[i])
end
endgenerate

wire[127:0] selectedFirstStagePermut = firstStagePermutWires[permutState7];

wire[127:0] secondStagePermutWires[5:0];
generate
for(i = 0; i < 6; i = i + 1) begin
    `VAR_SWAP_INLINE_WITHIN_GENERATE(1, i+1, selectedFirstStagePermut, secondStagePermutWires[i])
end
endgenerate

assign outputBot = secondStagePermutWires[permutState6];
assign outputBotValid = currentlyPermutingValid;

endmodule



module permutationQueue(
    input clk,
    input rst,
    
    input[127:0] top,
    
    input botInValid,
    output botSeriesFinished,
    input[127:0] botIn,
    output[8:0] usedw,
    
    // output side
    input readRequest,
    output[127:0] permutedBotOut,
    output dataOutValid
);

wire[23:0] validBotPermutations;
permuteCheck24 permuteChecker(top, bot, botInValid, validBotPermutations);

wire hasAnyValidPermutation = |validBotPermutations;

reg[5:0] currentInputTally;
always @(posedge clk) begin
    if(rst || botSeriesFinished) begin
        //currentInputTally <= 0 + hasAnyValidPermutation;
        currentInputTally[5:1] <= 0;
        currentInputTally[0] <= hasAnyValidPermutation;
    end else if(hasAnyValidPermutation) begin
        currentInputTally <= currentInputTally + 1;
    end
end

/*wire[4:0] batchSizeUsedW;
FastFIFO #(.WIDTH(6), .DEPTH_LOG2(5)) batchSizeFIFO(
    .clk(clk),
    .rst(rst),
    
    // input side
    .writeEnable(botSeriesFinished),
    .dataIn(currentInputTally),
    .usedw(batchSizeUsedW),
    
    // output side
    input readRequest,
    output[WIDTH-1:0] dataOut,
    output dataOutValid
);

FastFIFO #(.WIDTH(128+24), .DEPTH_LOG2(9)) dataFIFO(
    .clk(clk),
    .rst(rst),
    
    // input side
    .writeEnable(hasAnyValidPermutation),
    .dataIn({botIn, validBotPermutations}),
    .usedw(usedw),
    
    // output side
    input readRequest,
    output[WIDTH-1:0] dataOut,
    output dataOutValid
);*/

endmodule

module aggregationManager(
    input clk,
    input rst,
    
    
    // batchSizeFIFO side
    output readyForNewBatch,
    
    input startNewBatch, // called after requestNewBatch loops back
    input[5:0] batchSizeIn,
    
    // pipeline input side
    output botAvailable,
    
    input slowDown,
    
    // Pipeline output side
    input resultAvailable,
    
);

wire grabStagedBatch;
wire stagedBatchAvailable;
wire[5:0] stagedBatchSize;
pipelineRegister #(6) stagedBatchSizeReg(clk, rst, startNewBatch, batchSizeIn, grabStagedBatch, stagedBatchAvailable, stagedBatchSize);

assign readyForNewBatch = !stagedBatchAvailable;


reg[5:0] curBatchIndex;

assign botAvailable = curBatchIndex != 0 && !slowDown;

always @(posedge clk) begin
    if(rst) begin
        curBatchIndex <= 0;
    end else begin
        if(botAvailable)  begin
            curBatchIndex <= curBatchIndex - 1;
        end
    end
end


FastFIFO #(.WIDTH(6), .DEPTH_LOG2(`ADDR_WIDTH)) inProcessBatchSizeFIFO( // Just make this fifo as big as the pipeline memory, that way we can be sure it'll never overflow
    .clk(clk),
    .rst(rst),
    
    // input side
    .writeEnable(startNewBatch),
    .dataIn(batchSizeIn),
    .usedw(),// Unused, FIFO is big enough to hold all batch sizes we could dream of
    
    // output side
    input readRequest,
    output[WIDTH-1:0] dataOut,
    output dataOutValid
);

endmodule
