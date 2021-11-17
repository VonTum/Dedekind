`timescale 1ns / 1ps

// permutes the last 3 variables
module botPermuter #(parameter EXTRA_DATA_WIDTH = 12) (
    input clk,
    
    // input side
    input[127:0] botIn,
    input botInIsValid,
    input[5:0] validBotPermutesIn, // == {vABCin, vACBin, vBACin, vBCAin, vCABin, vCBAin}
    input[EXTRA_DATA_WIDTH-1:0] extraDataIn,
    output requestBotFromInput,
    
    // output side
    input requestBot,
    output reg permutedBotValid,
    output reg[127:0] permutedBot,
    output reg[2:0] selectedPermutationOut,
    output reg[EXTRA_DATA_WIDTH-1:0] extraDataOut
);

reg[127:0] savedBot;
reg[EXTRA_DATA_WIDTH-1:0] savedExtraData;
reg[5:0] validBotPermutes = 6'b000000;

initial begin
    savedBot = 0;
    savedExtraData = 0;
    permutedBotValid = 0;
    permutedBot = 0;
    selectedPermutationOut = 7;
    extraDataOut = 0;
end

// This marks the permutation that should be removed next
wire[5:0] shouldNotRemove = {
    0, 
    validBotPermutes[5], 
    |validBotPermutes[5:4], 
    |validBotPermutes[5:3], 
    |validBotPermutes[5:2], 
    |validBotPermutes[5:1]
};

reg[2:0] selectedPermutation;
reg isDone;
always @(validBotPermutes) begin
    casez(validBotPermutes)
        6'b1?????: selectedPermutation <= 5;
        6'b01????: selectedPermutation <= 4;
        6'b001???: selectedPermutation <= 3;
        6'b0001??: selectedPermutation <= 2;
        6'b00001?: selectedPermutation <= 1;
        6'b000001: selectedPermutation <= 0;
        6'b000000: selectedPermutation <= 7;
    endcase
    casez(validBotPermutes)
        6'b100000: isDone <= 1;
        6'b010000: isDone <= 1;
        6'b001000: isDone <= 1;
        6'b000100: isDone <= 1;
        6'b000010: isDone <= 1;
        6'b000001: isDone <= 1;
        6'b000000: isDone <= 1;
        default: isDone <= 0;
    endcase
end

wire[127:0] permutedBotFromSelector;
botPermuteSelector3 combinatorialSelector (
    savedBot,
    validBotPermutes,
    permutedBotFromSelector
);

assign requestBotFromInput = requestBot & isDone;

always @(posedge clk) begin
    if(requestBotFromInput) begin
        savedBot <= botIn;
        savedExtraData <= extraDataIn;
    end
    if(requestBot) begin
        if(isDone) begin
            if(botInIsValid)
                validBotPermutes <= validBotPermutesIn;
            else
                validBotPermutes <= 6'b000000;
        end else begin
            validBotPermutes <= validBotPermutes & shouldNotRemove;
        end
        
        permutedBotValid <= validBotPermutes != 0;
        permutedBot <= permutedBotFromSelector;
        extraDataOut <= savedExtraData;
        selectedPermutationOut <= selectedPermutation;
    end
end

endmodule

module botPermuteSelector3 (
    input[127:0] bot,
    input[5:0] validBotPermutes, // == {vABCin, vACBin, vBACin, vBCAin, vCABin, vCBAin}
    output[127:0] permutedBot
);

wire[15:0] botParts[7:0];
genvar i;
generate
for(i = 0; i < 8; i = i + 1) assign botParts[i] = bot[i*16 +: 16];
endgenerate


wire[15:0] permutedBotParts[7:0];
generate
for(i = 0; i < 8; i = i + 1) assign permutedBot[i*16 +: 16] = permutedBotParts[i];
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