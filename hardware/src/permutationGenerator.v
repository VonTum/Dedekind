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
    output botSeriesFinished
);

`include "inlineVarSwap_header.v"

reg[127:0] nextBot;
reg nextBotValid;
assign hasSpaceForNextBot = !nextBotValid;

reg[2:0] permutState7 = 0;
reg[2:0] permutState6 = 0;

reg[127:0] currentlyPermuting;
reg currentlyPermutingValid;

wire endOfPermutation = permutState6 >= 5 && permutState7 >= 6;

always @(posedge clk) begin
    if(permutState6 >= 5) begin
        permutState6 <= 0;
        if(permutState7 >= 6) begin
            permutState7 <= 0;
        end else permutState7 <= permutState7 + 1;
    end else permutState6 <= permutState6 + 1;
end

always @(posedge clk) begin
    if(rst) begin
        currentlyPermutingValid <= 0;
        nextBotValid <= 0;
    end else begin
        if(endOfPermutation) begin
            currentlyPermuting <= nextBot;
            currentlyPermutingValid <= nextBotValid;
        end
        if(writeInputBot) begin
            nextBot <= inputBot;
            nextBotValid <= 1;
        end else if(endOfPermutation) nextBotValid <= 0;
    end
end

wire[127:0] firstStagePermutWires[6:0];
`VAR_SWAP_INLINE(0, 0, currentlyPermuting, firstStagePermutWires[0])
`VAR_SWAP_INLINE(0, 1, currentlyPermuting, firstStagePermutWires[1])
`VAR_SWAP_INLINE(0, 2, currentlyPermuting, firstStagePermutWires[2])
`VAR_SWAP_INLINE(0, 3, currentlyPermuting, firstStagePermutWires[3])
`VAR_SWAP_INLINE(0, 4, currentlyPermuting, firstStagePermutWires[4])
`VAR_SWAP_INLINE(0, 5, currentlyPermuting, firstStagePermutWires[5])
`VAR_SWAP_INLINE(0, 6, currentlyPermuting, firstStagePermutWires[6])

reg[127:0] selectedFirstStagePermut; always @(posedge clk) selectedFirstStagePermut <= firstStagePermutWires[permutState7];
reg[2:0] permutState6D; always @(posedge clk) permutState6D <= permutState6;

wire[127:0] secondStagePermutWires[5:0];

`VAR_SWAP_INLINE(1, 1, selectedFirstStagePermut, secondStagePermutWires[0])
`VAR_SWAP_INLINE(1, 2, selectedFirstStagePermut, secondStagePermutWires[1])
`VAR_SWAP_INLINE(1, 3, selectedFirstStagePermut, secondStagePermutWires[2])
`VAR_SWAP_INLINE(1, 4, selectedFirstStagePermut, secondStagePermutWires[3])
`VAR_SWAP_INLINE(1, 5, selectedFirstStagePermut, secondStagePermutWires[4])
`VAR_SWAP_INLINE(1, 6, selectedFirstStagePermut, secondStagePermutWires[5])

reg[127:0] selectedSecondStagePermut; always @(posedge clk) selectedSecondStagePermut <= secondStagePermutWires[permutState6D];
assign outputBot = selectedSecondStagePermut;
hyperpipe #(.CYCLES(2)) outputBotValidPipe(clk, currentlyPermutingValid, outputBotValid);
hyperpipe #(.CYCLES(2)) botSeriesFinishedPipe(clk, endOfPermutation & currentlyPermutingValid, botSeriesFinished);

endmodule
