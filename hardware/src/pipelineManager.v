`timescale 1ns / 1ps
`include "pipelineGlobals.vhd"

module pipelineManager (
    input clk,
    input rst,
    
    // From controlling circuit
    input startNewTop,
    input[127:0] topIn,
    
	input isBotInValid, 
	output readyForBotIn,
	output resultValid,
    
    
    // To pipelines
    output reg[127:0] top,
    output reg[`ADDR_WIDTH-1:0] botIndex,
    output isBotValid,
    input fifoAlmostFull
);

reg[`ADDR_WIDTH-1:0] lastValidBotIndex;
reg allBotsCleared;
wire[`ADDR_WIDTH-1:0] botIndexSinceLast = botIndex - lastValidBotIndex;

reg newTopWaiting;
reg[127:0] newTopInWaiting;

reg isInitializing;


wire canAcceptNewBot = !fifoAlmostFull;
wire advancingShiftReg = canAcceptNewBot & !isInitializing;
assign readyForBotIn = advancingShiftReg & !newTopWaiting;
assign isBotValid = isBotInValid & readyForBotIn;

wire outputBotAvailable;
shiftRegister #(.CYCLES((1 << `ADDR_WIDTH) - `OUTPUT_INDEX_OFFSET), .WIDTH(1), .RESET_VALUE(1'b0)) validIndicesRegister(
    clk,
    advancingShiftReg,
    rst,
    isBotValid,
    outputBotAvailable
);

hyperpipe #(.CYCLES(`OUTPUT_READ_LATENCY), .WIDTH(1)) outputBotDelay(clk, outputBotAvailable, resultValid);

wire[`ADDR_WIDTH-1:0] INITIALIZATION_START = -1;

always @(posedge clk) begin
    if(rst) begin
        isInitializing <= 1'b0;
        allBotsCleared <= 1'b1;
        newTopWaiting <= 1'b0;
        
        newTopInWaiting <= 128'bX;
        top <= 128'bX;
        botIndex <= `ADDR_WIDTH'bX;
        
    // The new top gets loaded in, 
    end else if(newTopWaiting & allBotsCleared) begin
        newTopWaiting <= 1'b0;
        isInitializing <= 1'b1;
        top <= newTopInWaiting;
        botIndex <= -`OUTPUT_INDEX_OFFSET; // start initializing at -1024, because the first module of the collectionModule will not have been initialized
    
    // A new top arrives, is stored temporarely while the previous top is finished up
    end else if(canAcceptNewBot) begin
        if(botIndex == INITIALIZATION_START) begin
            isInitializing <= 1'b0;
        end
        
        if(isBotInValid) begin
            if(startNewTop & !newTopWaiting) begin
                newTopInWaiting <= topIn;
                newTopWaiting <= 1'b1;
            end
        end
        
        if(readyForBotIn & !startNewTop) begin
            lastValidBotIndex <= botIndex;
            allBotsCleared <= 1'b0;
        end else begin
            if(botIndexSinceLast >= (1 << `ADDR_WIDTH) - `OUTPUT_INDEX_OFFSET) begin
                allBotsCleared <= 1'b1;
            end
        end
        
        botIndex <= botIndex + 1;
    end
end

endmodule
