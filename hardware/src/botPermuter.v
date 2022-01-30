`timescale 1ns / 1ps

// permutes the last 3 variables
module botPermuter #(parameter EXTRA_DATA_WIDTH = 12) (
    input clk,
    input rst,
    
    // input side
    input startNewBurst,
    input[127:0] botIn,
    input[5:0] validBotPermutesIn, // == {vABCin, vACBin, vBACin, vBCAin, vCABin, vCBAin}
    input[EXTRA_DATA_WIDTH-1:0] extraDataIn,
    output reg done,
    
    // output side
    output reg permutedBotValid,
    output reg[127:0] permutedBot,
    output reg[2:0] selectedPermutationOut,
    output reg[EXTRA_DATA_WIDTH-1:0] extraDataOut
);


wire rstLocal; // Manual reset tree, can't use constraints to have it generate it for me. 
hyperpipe #(.CYCLES(2)) rstPipe(clk, rst, rstLocal);


reg[127:0] savedBot;
reg[EXTRA_DATA_WIDTH-1:0] savedExtraData;
reg[5:0] validBotPermutes;

// This marks the permutation that should be removed next
wire[5:0] shouldNotRemove = {
    1'b0, 
    validBotPermutes[5], 
    |validBotPermutes[5:4], 
    |validBotPermutes[5:3], 
    |validBotPermutes[5:2], 
    |validBotPermutes[5:1]
};

wire[127:0] permutedBotFromSelector;
botPermuteSelector6 combinatorialSelector (
    savedBot,
    validBotPermutes,
    permutedBotFromSelector
);

always @(posedge clk) begin
    casez(validBotPermutes)
        6'b100000: done <= 1;
        6'b010000: done <= 1;
        6'b001000: done <= 1;
        6'b000100: done <= 1;
        6'b000010: done <= 1;
        6'b000001: done <= 1;
        6'b000000: done <= 1;
        default: done <= 0;
    endcase
    casez(validBotPermutes)
        6'b1?????: selectedPermutationOut <= 5;
        6'b01????: selectedPermutationOut <= 4;
        6'b001???: selectedPermutationOut <= 3;
        6'b0001??: selectedPermutationOut <= 2;
        6'b00001?: selectedPermutationOut <= 1;
        6'b000001: selectedPermutationOut <= 0;
        6'b000000: selectedPermutationOut <= 7;
    endcase
    
    permutedBotValid <= validBotPermutes != 6'b000000;
    permutedBot <= permutedBotFromSelector;
    extraDataOut <= savedExtraData;
end

always @(posedge clk) begin
    if(rstLocal) begin
        savedBot <= 1'bX;
        savedExtraData <= 1'bX;
        validBotPermutes <= 6'b000000;
    end else begin
        if(startNewBurst) begin
            savedBot <= botIn;
            savedExtraData <= extraDataIn;
            validBotPermutes <= validBotPermutesIn;
        end else
            validBotPermutes <= validBotPermutes & shouldNotRemove;
    end
end

endmodule

module botPermuteSelector6 (
    input[127:0] bot,
    input[5:0] validBotPermutes, // == {vABCin, vACBin, vBACin, vBCAin, vCABin, vCBAin}
    output[127:0] permutedBot
);

wire[15:0] botParts[7:0];
genvar i;
generate
for(i = 0; i < 8; i = i + 1) begin assign botParts[i] = bot[i*16 +: 16]; end
endgenerate


wire[15:0] permutedBotParts[7:0];
generate
for(i = 0; i < 8; i = i + 1) begin assign permutedBot[i*16 +: 16] = permutedBotParts[i]; end
endgenerate

assign permutedBotParts[3'b000] = botParts[3'b000];
assign permutedBotParts[3'b111] = botParts[3'b111];

wire[15:0] oneVarBotParts[2:0];
assign oneVarBotParts[0] = botParts[3'b001];
assign oneVarBotParts[1] = botParts[3'b010];
assign oneVarBotParts[2] = botParts[3'b100];

wire[15:0] twoVarBotParts[2:0];
assign twoVarBotParts[0] = botParts[3'b110];
assign twoVarBotParts[1] = botParts[3'b101];
assign twoVarBotParts[2] = botParts[3'b011];

reg[1:0] selectedFirst;
reg[1:0] selectedSecond;
reg[1:0] selectedThird;

assign permutedBotParts[3'b001] = oneVarBotParts[selectedFirst];
assign permutedBotParts[3'b010] = oneVarBotParts[selectedSecond];
assign permutedBotParts[3'b100] = oneVarBotParts[selectedThird];

assign permutedBotParts[3'b110] = twoVarBotParts[selectedFirst];
assign permutedBotParts[3'b101] = twoVarBotParts[selectedSecond];
assign permutedBotParts[3'b011] = twoVarBotParts[selectedThird];

always @(validBotPermutes) begin
    casez(validBotPermutes)
        6'b1?????: begin selectedFirst <= 0; selectedSecond <= 1; selectedThird <= 2; end
        6'b01????: begin selectedFirst <= 0; selectedSecond <= 2; selectedThird <= 1; end
        6'b001???: begin selectedFirst <= 1; selectedSecond <= 0; selectedThird <= 2; end
        6'b0001??: begin selectedFirst <= 1; selectedSecond <= 2; selectedThird <= 0; end
        6'b00001?: begin selectedFirst <= 2; selectedSecond <= 0; selectedThird <= 1; end
        6'b000001: begin selectedFirst <= 2; selectedSecond <= 1; selectedThird <= 0; end
        6'b000000: begin selectedFirst <= 2'bxx; selectedSecond <= 2'bxx; selectedThird <= 2'bxx; end
    endcase
end

endmodule