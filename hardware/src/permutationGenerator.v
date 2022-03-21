`timescale 1ns / 1ps

`include "pipelineGlobals_header.v"


module permute67 (
    input clk,
    
    input[2:0] permut6,
    input[2:0] permut7,

    input[127:0] mbfIn,
    output reg[127:0] mbfOut,
    
    input currentlyPermutingValidIn,
    output reg currentlyPermutingValid,
    
    input botSeriesFinishedIn,
    output reg botSeriesFinished
);

`include "inlineVarSwap_header.v"

wire[127:0] firstStagePermutWires[6:0];
`VAR_SWAP_INLINE(0, 0, mbfIn, firstStagePermutWires[0])
`VAR_SWAP_INLINE(0, 1, mbfIn, firstStagePermutWires[1])
`VAR_SWAP_INLINE(0, 2, mbfIn, firstStagePermutWires[2])
`VAR_SWAP_INLINE(0, 3, mbfIn, firstStagePermutWires[3])
`VAR_SWAP_INLINE(0, 4, mbfIn, firstStagePermutWires[4])
`VAR_SWAP_INLINE(0, 5, mbfIn, firstStagePermutWires[5])
`VAR_SWAP_INLINE(0, 6, mbfIn, firstStagePermutWires[6])

reg[127:0] selectedFirstStagePermut; always @(posedge clk) selectedFirstStagePermut <= firstStagePermutWires[permut7];
reg[2:0] permut6D; always @(posedge clk) permut6D <= permut6;
reg botSeriesFinishedInD; always @(posedge clk) botSeriesFinishedInD <= botSeriesFinishedIn;
reg currentlyPermutingValidInD; always @(posedge clk) currentlyPermutingValidInD <= currentlyPermutingValidIn;

wire[127:0] secondStagePermutWires[5:0];

`VAR_SWAP_INLINE(1, 1, selectedFirstStagePermut, secondStagePermutWires[0])
`VAR_SWAP_INLINE(1, 2, selectedFirstStagePermut, secondStagePermutWires[1])
`VAR_SWAP_INLINE(1, 3, selectedFirstStagePermut, secondStagePermutWires[2])
`VAR_SWAP_INLINE(1, 4, selectedFirstStagePermut, secondStagePermutWires[3])
`VAR_SWAP_INLINE(1, 5, selectedFirstStagePermut, secondStagePermutWires[4])
`VAR_SWAP_INLINE(1, 6, selectedFirstStagePermut, secondStagePermutWires[5])

always @(posedge clk) mbfOut <= secondStagePermutWires[permut6D];
always @(posedge clk) botSeriesFinished <= botSeriesFinishedInD;
always @(posedge clk) currentlyPermutingValid <= currentlyPermutingValidInD;

endmodule

module permutationIterator67 (
    input clk,
    
    output reg[2:0] permut6,
    output reg[2:0] permut7
);

initial permut6 = 0;
initial permut7 = 0;

wire zero6 = permut6 == 0;
wire zero7 = permut7 == 0;

// A very strange bug occurs for some orders of these permutation indices. Further investigation is required. 
always @(posedge clk) begin
    permut7 <= zero7 ? 6 : permut7 - 1;
    if(zero7) begin
        permut6 <= zero6 ? 5 : permut6 - 1;
    end
end

endmodule


module permutationGenerator67 (
    input clk,
    input rst,
    
    input[2:0] permut7,
    input[2:0] permut6,
    
    input[127:0] nextBot,
    input nextBotValid,
    output requestNextBot,
    
    output[127:0] outputBot,
    output outputBotValid,
    output botSeriesFinished,
    input slowDown
);


reg[127:0] currentlyPermuting;
reg currentlyPermutingValid;

wire permutationWillEnd = (permut6 == 0) && (permut7 == 0);
assign requestNextBot = permutationWillEnd && !slowDown;
wire endOfPermutation;

// 3 for fifo, 1 for extra reg in readRequest path
`define INPUT_FIFO_REQUEST_LATENCY 3+1
hyperpipe #(.CYCLES(`INPUT_FIFO_REQUEST_LATENCY)) requestLatency(clk, permutationWillEnd, endOfPermutation);

always @(posedge clk) begin
    if(rst) begin
        currentlyPermutingValid <= 0;
    end else begin
        if(endOfPermutation) begin
            currentlyPermuting <= nextBot;
            currentlyPermutingValid <= nextBotValid;
        end
    end
end

permute67 permut67(
    clk, permut6, permut7,
    currentlyPermuting, outputBot, 
    currentlyPermutingValid, outputBotValid, 
    currentlyPermutingValid && endOfPermutation, botSeriesFinished
);

endmodule

module multiPermutationGenerator67 (
    input clk,
    input clk2x,
    input rst,
    
    // Input side, clocked at clk
    input[127:0] inputBot,
    input writeInputBot,
    output hasSpaceForNextBot,
    
    // Output side, clocked at clk2x
    output[128*`NUMBER_OF_PERMUTATORS-1:0] outputBots,
    output[`NUMBER_OF_PERMUTATORS-1:0] outputBotsValid,
    output[`NUMBER_OF_PERMUTATORS-1:0] botSeriesFinished,
    input[`NUMBER_OF_PERMUTATORS-1:0] slowDownPermutationProduction
);

wire[2:0] permut7;
wire[2:0] permut6;
permutationIterator67 iter67(clk2x, permut6, permut7);

wire[2:0] permut6Divider[`NUMBER_OF_PERMUTATORS-1:0];

generate
assign permut6Divider[0] = permut6;
/*if(`NUMBER_OF_PERMUTATORS >= 2) assign permut6Divider[1] = permut6 < 3 ? permut6 + 3 : permut6 - 3; // (permut6 + 3) % 6
if(`NUMBER_OF_PERMUTATORS >= 3) assign permut6Divider[2] = permut6 < 5 ? permut6 + 1 : 0; // (permut6 + 1) % 6
if(`NUMBER_OF_PERMUTATORS >= 4) assign permut6Divider[3] = permut6 < 2 ? permut6 + 4 : permut6 - 2; // (permut6 + 4) % 6
if(`NUMBER_OF_PERMUTATORS >= 5) assign permut6Divider[4] = permut6 < 1 ? 5 : permut6 - 1; // (permut6 + 5) % 6
if(`NUMBER_OF_PERMUTATORS >= 6) assign permut6Divider[5] = permut6 < 4 ? permut6 + 2 : permut6 - 4; // (permut6 + 2) % 6*/
if(`NUMBER_OF_PERMUTATORS == 3) begin
    assign permut6Divider[1] = permut6 < 2 ? permut6 + 4 : permut6 - 2; // (permut6 + 4) % 6
    assign permut6Divider[2] = permut6 < 4 ? permut6 + 2 : permut6 - 4; // (permut6 + 2) % 6
end
endgenerate


wire permutationGeneratorInputFIFOAlmostFull;
assign hasSpaceForNextBot = !permutationGeneratorInputFIFOAlmostFull;

wire[`NUMBER_OF_PERMUTATORS-1:0] newBotRequests;
reg requestNewBot; always @(posedge clk2x) requestNewBot <= |newBotRequests;
wire[127:0] botFromInputFIFO;
wire botFromInputFIFOValid;

wire rst2x;
synchronizer rstSync(clk, rst, clk2x, rst2x);

// 3 cycles read latency
FastDualClockFIFO_SAFE #(.WIDTH(128), .DEPTH_LOG2(5), .IS_MLAB(1)) permutationGeneratorInputFIFO(
    // input side
    .wrclk(clk),
    .wrrst(rst),
    .writeEnable(writeInputBot),
    .dataIn(inputBot),
    .almostFull(permutationGeneratorInputFIFOAlmostFull),
    
    // output side
    .rdclk(clk2x),
    .rdrst(rst2x),
    .readRequest(requestNewBot),
    .dataOut(botFromInputFIFO),
    .dataOutValid(botFromInputFIFOValid),
    .empty(), // unused
    .eccStatus() // unused
);

generate

genvar i;
for(i = 0; i < `NUMBER_OF_PERMUTATORS; i = i + 1) begin
    permutationGenerator67 subGenerator67 (
        .clk(clk2x),
        .rst(rst2x),
        
        .permut7(permut7),
        .permut6(permut6Divider[i]),
        
        .nextBot(botFromInputFIFO),
        .nextBotValid(botFromInputFIFOValid),
        .requestNextBot(newBotRequests[i]),
        
        .outputBot(outputBots[128*i +: 128]),
        .outputBotValid(outputBotsValid[i]),
        .botSeriesFinished(botSeriesFinished[i]),
        .slowDown(slowDownPermutationProduction[i])
    );
end

endgenerate

endmodule

module oldPermutationGenerator67 (
    input clk,
    input rst,
    
    input[127:0] inputBot,
    input writeInputBot,
    output hasSpaceForNextBot,
    
    output[127:0] outputBot,
    output outputBotValid,
    output botSeriesFinished
);

reg[127:0] nextBot;
reg nextBotValid;
assign hasSpaceForNextBot = !nextBotValid;

wire[2:0] permut6;
wire[2:0] permut7;
permutationIterator67 iter67 (clk, permut6, permut7);

wire endOfPermutation = permut6 == 0 && permut7 == 0;

reg[127:0] currentlyPermuting;
reg currentlyPermutingValid;

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

permute67 permut67 (
    clk, permut6, permut7, 
    currentlyPermuting, outputBot, 
    currentlyPermutingValid, outputBotValid, 
    currentlyPermutingValid && endOfPermutation, botSeriesFinished
);

endmodule
