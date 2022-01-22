`timescale 1ns / 1ps
`include "pipelineGlobals_header.v"




module isBotValidShifter #(parameter OUTPUT_LATENCY = `OUTPUT_READ_LATENCY) (
    input clk,
    input rst,
    
    input advanceShiftReg,
    input isBotValid,
    output outputBotValid
);

wire rstLocal; // Manual reset tree, can't use constraints to have it generate it for me. 
hyperpipe #(.CYCLES(2)) rstPipe(clk, rst, rstLocal);

localparam SHIFT_DEPTH = (1 << `ADDR_WIDTH) - `OUTPUT_INDEX_OFFSET;

reg[`ADDR_WIDTH-1:0] cyclesUntilResetFinished;

wire isResetFinished = cyclesUntilResetFinished == 0;

always @(posedge clk) begin
    if(rstLocal) begin
        cyclesUntilResetFinished <= SHIFT_DEPTH;
    end else begin
        if(advanceShiftReg) begin
            if(!isResetFinished) cyclesUntilResetFinished <= cyclesUntilResetFinished - 1;
        end
    end
end

wire isLastBotInShiftRegisterValid;
enabledShiftRegister #(.CYCLES(SHIFT_DEPTH), .WIDTH(1)) validIndicesRegister(
    clk,
    advanceShiftReg,
    1'b0,
    isBotValid,
    isLastBotInShiftRegisterValid
);

wire isLastBotInShiftRegisterValidIncludingReset = isLastBotInShiftRegisterValid & isResetFinished;

reg prevAdvanceShiftReg; always @(posedge clk) prevAdvanceShiftReg <= advanceShiftReg;

hyperpipe #(.CYCLES(OUTPUT_LATENCY), .WIDTH(1)) outputBotDelay(
    clk,
    isLastBotInShiftRegisterValidIncludingReset & prevAdvanceShiftReg, // the & prevAdvanceShiftReg makes sure outputs are 1-clock pulses
    outputBotValid
);

endmodule



module pipelineManager (
    input clk,
    input rst,
    
	input isBotInValid, 
	output readyForBotIn,
	output resultValid,
    
    // To pipelines
    output reg[`ADDR_WIDTH-1:0] botIndex,
    output isBotValid,
    input pipelineReady
);

wire rstLocal; // Manual reset tree, can't use constraints to have it generate it for me. 
hyperpipe #(.CYCLES(2)) rstPipe(clk, rst, rstLocal);

wire advancingShiftReg = pipelineReady & !rstLocal;
assign readyForBotIn = advancingShiftReg;
assign isBotValid = isBotInValid & readyForBotIn;

isBotValidShifter isBotValidHistory (
    .clk(clk),
    .rst(rst),
    
    .advanceShiftReg(advancingShiftReg),
    .isBotValid(isBotValid),
    .outputBotValid(resultValid)
);

always @(posedge clk) begin
    if(rstLocal) begin
        botIndex <= 0;
    end else if(pipelineReady) begin
        botIndex <= botIndex + 1;
    end
end

endmodule
