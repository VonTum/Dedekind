`timescale 1ns / 1ps

module testBotPermuter1234();

reg clk = 0;
initial forever #1 clk = !clk;

reg rst = 1;

// Input side
reg writeDataIn = 0;
reg[127:0] botIn = 0;
reg[23:0] validBotPermutesIn = 0;
reg lastBotOfBatchIn = 0;
wire almostFull;

// Output side
reg slowDown = 0;
wire permutedBotValid;
wire[127:0] permutedBot;
wire batchDone;
wire eccStatus;

initial begin
    #15 rst = 0;
    #5
    writeDataIn = 1;
    botIn = 128'h00000000_00000000_00000000_FFFFFFFF;
    validBotPermutesIn = 24'b110100_001011_001010_000010;
    lastBotOfBatchIn = 0;
    #2
    writeDataIn = 0;
    #2
    writeDataIn = 1;
    botIn = 128'h00000000_00000000_FFFFFFFF_FFFFFFFF;
    validBotPermutesIn = 24'b000000_010000_000000_100000;
    lastBotOfBatchIn = 1;
    #2
    botIn = 128'h00000000_FFFFFFFF_FFFFFFFF_FFFFFFFF;
    validBotPermutesIn = 24'b000000_000000_000000_000000;
    lastBotOfBatchIn = 1;
    #2
    botIn = 128'hFFFFFFFF_FFFFFFFF_FFFFFFFF_FFFFFFFF;
    validBotPermutesIn = 24'b000000_000000_000000_000000;
    lastBotOfBatchIn = 0;
    #2
    writeDataIn = 0;
end

botPermuter1234 #(.ALMOST_FULL_MARGIN(64)) botPermuter1234_test (
    clk,
    rst,
    
    // Input side
    writeDataIn,
    botIn,
    validBotPermutesIn,
    lastBotOfBatchIn,
    almostFull,
    
    // Output side
    slowDown,
    permutedBotValid,
    permutedBot,
    batchDone,
    eccStatus
);

endmodule
