`timescale 1ns / 1ps
/*

module aggregatingPipeline(
    input clk,
    input rst,
    
    input[127:0] top,
    
    // batchSizeFIFO side
    output readyForNewBatch,
    
    input startNewBatch, // called after requestNewBatch loops back
    input[5:0] batchSizeIn,
    
    output grabBot,
    input[127:0] botIn, // Arrives 3? Clock cycles after grabBot call
);

wire grabStagedBatch;
wire stagedBatchAvailable;
wire[5:0] stagedBatchSize;
pipelineRegister #(6) stagedBatchSizeReg(clk, rst, startNewBatch, batchSizeIn, grabStagedBatch, stagedBatchAvailable, stagedBatchSize);

assign readyForNewBatch = !stagedBatchAvailable;


reg[5:0] curBatchIndex;

assign grabBot = curBatchIndex != 0 && !slowDown;

always @(posedge clk) begin
    if(rst) begin
        curBatchIndex <= 0;
    end else begin
        if(grabBot)  begin
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

wire[4:0] maxFullness;
wire slowThePipeline = maxFullness > 20;

pipeline24Pack pipeline(
    .clk(clk),
    .rst(rst),
    
    .top(top),
    .bot(botIn),
    input[`ADDR_WIDTH-1:0] botIndex,
    input isBotValid,
    input[23:0] validBotPermutations, // == {permutesABCD, permutesBACD, permutesCBAD, permutesDBCA}
    .maxFullness(maxFullness),
    output[39:0] summedData, // log2(24*2^35)=39.5849625007 -> 40 bits
    output[4:0] pcoeffCount // log2(24)=4.5849625007 -> 5 bits
);

module pipelineManager (
    input clk,
    input rst,
    
	input isBotInValid, 
	output readyForBotIn,
	output resultValid,
    
    // To pipelines
    output[`ADDR_WIDTH-1:0] botIndex,
    output isBotValid,
    .pipelineReady()
);

endmodule
*/