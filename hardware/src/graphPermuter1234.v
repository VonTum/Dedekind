`timescale 1ns / 1ps

module graphPermuter1234  #(parameter ALMOST_FULL_MARGIN = 64) (
    input clk,
    input rst,
    
    // Input side
    input[127:0] botIn,
    input[23:0] validBotPermutesIn,
    input lastBotOfBatchIn,
    output almostFull,
    
    // Output side
    input[127:0] top,
    input slowDown,
    output reg graphValid,
    output reg[127:0] graph,
    output reg batchDone,
    output eccStatus
);

botPermuter1234 botPermuter1234(
    .clk(clk),
    .rst(rst),
    
    // Input side
    .writeDataIn(|validBotPermutesIn || lastBotOfBatchIn),
    .botIn(botIn),
    .validBotPermutesIn(validBotPermutesIn),
    .lastBotOfBatchIn(lastBotOfBatchIn),
    .almostFull(almostFull),
    
    // Output side
    .slowDown(slowDown),
    .permutedBotValid(permutedBotValidWire),
    .permutedBot(permutedBotWire),
    .batchDone(batchDoneWire),
    .eccStatus(eccStatus)
);

always @(posedge clk) begin
    graph <= top & ~permutedBotWire;
    batchDone <= batchDoneWire;
    graphValid <= permutedBotValidWire;
end

endmodule

