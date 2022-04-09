`timescale 1ns / 1ps



module testPermuter1234();

reg clk = 0;
initial forever #1 clk = !clk;

reg[127:0] graphToPermute = 128'b00000001000000110000000101010111000000010001011100010111010111110000000100010011000101110101111100010101011101110011011101111111;

reg[4:0] permutation = 0;
always @(posedge clk) begin
    if(permutation < 23) permutation <= permutation + 1;
    else permutation <= 0;
end

wire[1:0] selectedSet = permutation[1:0];
wire[2:0] selectedPermutationInSet = permutation[4:2];
wire[127:0] permutedBot;

permute1234 permuter (
    clk,
    graphToPermute,
    selectedSet, // 0-3
    selectedPermutationInSet, // 0-5
    permutedBot
);

wire[127:0] permutedBot_CORRECT;

permute1234Naive permuterCheck (
    clk,
    graphToPermute,
    selectedSet, // 0-3
    selectedPermutationInSet, // 0-5
    permutedBot_CORRECT
);

wire IS_CORRECT = (permutedBot == permutedBot_CORRECT);

endmodule
