`timescale 1ns / 1ps
`include "pipelineGlobals_header.v"




module isBotValidShifter #(parameter OUTPUT_LATENCY = `OUTPUT_READ_LATENCY) (
    input clk,
    input rst,
    
    input advanceShiftReg,
    input isBotValid,
    output outputBotValid,
    output isEmpty
);

localparam SHIFT_DEPTH = (1 << `ADDR_WIDTH) - `OUTPUT_INDEX_OFFSET;

reg[`ADDR_WIDTH-1:0] cyclesUntilEmpty;

// A few cycles of delay to make fitting easier
hyperpipe #(.CYCLES(2), .WIDTH(1)) isEmptyDelay (
    clk,
    cyclesUntilEmpty == 0,
    isEmpty
);

always @(posedge clk) begin
    if(rst) begin
        cyclesUntilEmpty <= 0;
    end else begin
        if(advanceShiftReg) begin
            if(isBotValid) begin
                cyclesUntilEmpty <= SHIFT_DEPTH;
            end else begin
                if(!isEmpty) cyclesUntilEmpty <= cyclesUntilEmpty - 1;
            end
        end
    end
end

wire isLastBotInShiftRegisterValid;
enabledShiftRegister #(.CYCLES(SHIFT_DEPTH), .WIDTH(1), .RESET_VALUE(1'b0)) validIndicesRegister(
    clk,
    advanceShiftReg,
    rst,
    isBotValid,
    isLastBotInShiftRegisterValid
);

reg prevAdvanceShiftReg; always @(posedge clk) prevAdvanceShiftReg <= advanceShiftReg;

hyperpipe #(.CYCLES(OUTPUT_LATENCY), .WIDTH(1)) outputBotDelay(
    clk,
    isLastBotInShiftRegisterValid & prevAdvanceShiftReg, // the & prevAdvanceShiftReg makes sure outputs are 1-clock pulses
    outputBotValid
);

endmodule



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
    output[127:0] top,
    output reg[`ADDR_WIDTH-1:0] botIndex,
    output isBotValid,
    input pipelineReady
);

reg[127:0] topReg;
// No false paths sadly, so this hyperpipe should at least help reduce fitting strain. Can be up to `OUTPUT_INDEX_OFFSET = 1024 cycles
hyperpipe #(.CYCLES(30), .WIDTH(128)) topPipe(clk, topReg, top);

reg newTopWaiting;
reg[127:0] newTopInWaiting;

reg isInitializing;

wire advancingShiftReg = pipelineReady & !rst & !isInitializing;
assign readyForBotIn = advancingShiftReg & !newTopWaiting;
assign isBotValid = isBotInValid & readyForBotIn;

wire allBotsCleared;
isBotValidShifter isBotValidHistory (
    .clk(clk),
    .rst(rst),
    
    .advanceShiftReg(advancingShiftReg),
    .isBotValid(isBotValid),
    .outputBotValid(resultValid),
    .isEmpty(allBotsCleared)
);

wire[`ADDR_WIDTH-1:0] INITIALIZATION_START = -1;

always @(posedge clk) begin
    if(rst) begin
        isInitializing <= 1'b0;
        newTopWaiting <= 1'b0;
        
        newTopInWaiting <= 128'bX;
        topReg <= 128'bX;
        botIndex <= `ADDR_WIDTH'bX;
        
    // The new top gets loaded in, 
    end else if(newTopWaiting & allBotsCleared) begin
        newTopWaiting <= 1'b0;
        isInitializing <= 1'b1;
        topReg <= newTopInWaiting;
        botIndex <= -`OUTPUT_INDEX_OFFSET; // start initializing at -1024, because the first module of the collectionModule will not have been initialized
    
    // A new top arrives, is stored temporarely while the previous top is finished up
    end else if(pipelineReady & !rst) begin
        if(botIndex == INITIALIZATION_START) begin
            isInitializing <= 1'b0;
        end
        
        if(isBotInValid) begin
            if(startNewTop & !newTopWaiting) begin
                newTopInWaiting <= topIn;
                newTopWaiting <= 1'b1;
            end
        end
        
        botIndex <= botIndex + 1;
    end
end

endmodule
