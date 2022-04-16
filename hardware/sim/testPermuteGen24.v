`timescale 1ns / 1ps

module testPermuteGen24();

reg clk = 0;
initial forever #1 clk = !clk;

reg rst = 1;

reg writeDataIn = 0;

reg overrideValidBotPermutes;
initial begin
    #30
    rst = 0;
    #10
    overrideValidBotPermutes = 1;
    writeDataIn = 1;
    #2
    overrideValidBotPermutes = 0;
    #2
    writeDataIn = 0;
    #100
    $finish();
end

reg[127:0] top = 128'b00000000000001110000010111111111000000110111111101010111111111110001000100010111111111111111111101011111011111111111111111111111;
reg[127:0] bot = 128'b00000000000000010000000000001111000000000000010100000001001111110000000000000111000000000111111100000001011101110011111111111111;

//reg[127:0] top = 128'b00000000000000000000000100010101000000000011111100000111111111110001000100010001000101110001111101111111111111110111111111111111;
//reg[127:0] bot = 128'b00000000000000000000000000010001000000010001000100000011011101110000000000000000000101010101010100010101000111110101111111111111;

wire[23:0] validBotPermutes;
permuteCheck24 permuteCheck24(top, bot, 1, validBotPermutes);

wire almostFull;

wire permutedBotValid;
wire[127:0] permutedBot;
wire batchDone;
wire eccStatus;
    
botPermuter1234 permuter (clk, rst,
    // Input side
    writeDataIn,
    bot,
    overrideValidBotPermutes ? 24'hFFFFFF : validBotPermutes,
    1,
    almostFull,
    
    // Output side
    0,
    permutedBotValid,
    permutedBot,
    batchDone,
    eccStatus
);

wire isValidBotPermutation = (permutedBot & ~top) == 0;
wire IS_CORRECT = permutedBotValid ? isValidBotPermutation : 1;

endmodule
