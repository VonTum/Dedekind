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

always @(*) begin
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
end

always @(posedge clk) begin
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


module permute1234Naive(
    input clk,
    
    input[127:0] storedBot,
    input[1:0] selectedSet, // 0-3
    input[2:0] selectedPermutationInSet, // 0-5
    
    output[127:0] permutedBot
);

`include "inlineVarSwap_header.v"

wire[127:0] botABCD = storedBot;       // vs33 (no swap)
wire[127:0] botBACD; `VAR_SWAP_INLINE(3,4,botABCD, botBACD)// varSwap #(3,4) vs34 (botABCD, botBACD);
wire[127:0] botCBAD; `VAR_SWAP_INLINE(3,5,botABCD, botCBAD)// varSwap #(3,5) vs35 (botABCD, botCBAD);
wire[127:0] botDBCA; `VAR_SWAP_INLINE(3,6,botABCD, botDBCA)// varSwap #(3,6) vs36 (botABCD, botDBCA);

wire[127:0] botACBD; `VAR_SWAP_INLINE(4,5,botABCD, botACBD)// varSwap #(4,5) vs33_45 (botABCD, botACBD);
wire[127:0] botBCAD; `VAR_SWAP_INLINE(4,5,botBACD, botBCAD)// varSwap #(4,5) vs34_45 (botBACD, botBCAD);
wire[127:0] botCABD; `VAR_SWAP_INLINE(4,5,botCBAD, botCABD)// varSwap #(4,5) vs35_45 (botCBAD, botCABD);
wire[127:0] botDCBA; `VAR_SWAP_INLINE(4,5,botDBCA, botDCBA)// varSwap #(4,5) vs36_45 (botDBCA, botDCBA);

wire[127:0] botADCB; `VAR_SWAP_INLINE(4,6,botABCD, botADCB)// varSwap #(4,6) vs33_46 (botABCD, botADCB);
wire[127:0] botBDCA; `VAR_SWAP_INLINE(4,6,botBACD, botBDCA)// varSwap #(4,6) vs34_46 (botBACD, botBDCA);
wire[127:0] botCDAB; `VAR_SWAP_INLINE(4,6,botCBAD, botCDAB)// varSwap #(4,6) vs35_46 (botCBAD, botCDAB);
wire[127:0] botDACB; `VAR_SWAP_INLINE(4,6,botDBCA, botDACB)// varSwap #(4,6) vs36_46 (botDBCA, botDACB);


wire[127:0] botABDC; `VAR_SWAP_INLINE(5,6,botABCD, botABDC)
wire[127:0] botBADC; `VAR_SWAP_INLINE(5,6,botBACD, botBADC)
wire[127:0] botCBDA; `VAR_SWAP_INLINE(5,6,botCBAD, botCBDA)
wire[127:0] botDBAC; `VAR_SWAP_INLINE(5,6,botDBCA, botDBAC)

wire[127:0] botACDB; `VAR_SWAP_INLINE(5,6,botACBD, botACDB)
wire[127:0] botBCDA; `VAR_SWAP_INLINE(5,6,botBCAD, botBCDA)
wire[127:0] botCADB; `VAR_SWAP_INLINE(5,6,botCABD, botCADB)
wire[127:0] botDCAB; `VAR_SWAP_INLINE(5,6,botDCBA, botDCAB)

wire[127:0] botADBC; `VAR_SWAP_INLINE(5,6,botADCB, botADBC)
wire[127:0] botBDAC; `VAR_SWAP_INLINE(5,6,botBDCA, botBDAC)
wire[127:0] botCDBA; `VAR_SWAP_INLINE(5,6,botCDAB, botCDBA)
wire[127:0] botDABC; `VAR_SWAP_INLINE(5,6,botDACB, botDABC)


wire[127:0] allPermutations[3:0][5:0];
        /*abcd, abdc, acbd, acdb, adbc, adcb;
        bacd, badc, bcad, bcda, bdac, bdca;
        cbad, cbda, cabd, cadb, cdba, cdab; 
        dbca, dbac, dcba, dcab, dabc, dacb;*/

assign allPermutations[0][0] = botABCD;
assign allPermutations[0][1] = botABDC;
assign allPermutations[0][2] = botACBD;
assign allPermutations[0][3] = botACDB;
assign allPermutations[0][4] = botADBC;
assign allPermutations[0][5] = botADCB;

assign allPermutations[1][0] = botBACD;
assign allPermutations[1][1] = botBADC;
assign allPermutations[1][2] = botBCAD;
assign allPermutations[1][3] = botBCDA;
assign allPermutations[1][4] = botBDAC;
assign allPermutations[1][5] = botBDCA;

assign allPermutations[2][0] = botCBAD;
assign allPermutations[2][1] = botCBDA;
assign allPermutations[2][2] = botCABD;
assign allPermutations[2][3] = botCADB;
assign allPermutations[2][4] = botCDBA;
assign allPermutations[2][5] = botCDAB;

assign allPermutations[3][0] = botDBCA;
assign allPermutations[3][1] = botDBAC;
assign allPermutations[3][2] = botDCBA;
assign allPermutations[3][3] = botDCAB;
assign allPermutations[3][4] = botDABC;
assign allPermutations[3][5] = botDACB;

reg[1:0] selectedSetD; always @(posedge clk) selectedSetD <= selectedSet;
reg[2:0] selectedPermutationInSetD; always @(posedge clk) selectedPermutationInSetD <= selectedPermutationInSet;
assign permutedBot = allPermutations[selectedSetD][selectedPermutationInSetD];

endmodule

module permute1234(
    input clk,
    
    input[127:0] storedBot,
    input[1:0] selectedSet, // 0-3
    input[2:0] selectedPermutationInSet, // 0-5
        /*abcd, abdc, acbd, acdb, adbc, adcb;
        bacd, badc, bcad, bcda, bdac, bdca;
        cbad, cbda, cabd, cadb, cdba, cdab; 
        dbca, dbac, dcba, dcab, dabc, dacb;*/
    
    output[127:0] permutedBot
);

function [7:0] mux4;
    input[1:0] sel;
    input[7:0] v0, v1, v2, v3;
    begin
        case(sel)
            0: mux4 = v0;
            1: mux4 = v1;
            2: mux4 = v2;
            3: mux4 = v3;
        endcase
    end
endfunction

function [7:0] mux6;
    input[2:0] sel;
    input[7:0] v0, v1, v2, v4, v5, v6;
    begin
        case(sel)
            0: mux6 = v0;
            1: mux6 = v1;
            2: mux6 = v2;
            4: mux6 = v4;
            5: mux6 = v5;
            6: mux6 = v6;
        endcase
    end
endfunction


wire[7:0] bot[15:0];

reg[1:0] varInPos_a;
reg[1:0] varInPos_b;
reg[1:0] varInPos_c;
reg[1:0] varInPos_d;

reg[2:0] varInPos_ab;
reg[2:0] varInPos_ac;
reg[2:0] varInPos_ad;

always @(posedge clk) begin
    /* Generated LUTs */
    /*case({selectedSet, selectedPermutationInSet})
        5'o00: varInPos_a<=0; 5'o01: varInPos_a<=0; 5'o02: varInPos_a<=0; 5'o03: varInPos_a<=0; 5'o04: varInPos_a<=0; 5'o05: varInPos_a<=0; 
        5'o10: varInPos_a<=1; 5'o11: varInPos_a<=1; 5'o12: varInPos_a<=1; 5'o13: varInPos_a<=1; 5'o14: varInPos_a<=1; 5'o15: varInPos_a<=1; 
        5'o20: varInPos_a<=2; 5'o21: varInPos_a<=2; 5'o22: varInPos_a<=2; 5'o23: varInPos_a<=2; 5'o24: varInPos_a<=2; 5'o25: varInPos_a<=2; 
        5'o30: varInPos_a<=3; 5'o31: varInPos_a<=3; 5'o32: varInPos_a<=3; 5'o33: varInPos_a<=3; 5'o34: varInPos_a<=3; 5'o35: varInPos_a<=3; 
        default: varInPos_a<=2'bXX;
    endcase*/
    varInPos_a <= selectedSet;
    case({selectedSet, selectedPermutationInSet})
        5'o00: varInPos_b<=1; 5'o01: varInPos_b<=1; 5'o02: varInPos_b<=2; 5'o03: varInPos_b<=2; 5'o04: varInPos_b<=3; 5'o05: varInPos_b<=3; 
        5'o10: varInPos_b<=0; 5'o11: varInPos_b<=0; 5'o12: varInPos_b<=2; 5'o13: varInPos_b<=2; 5'o14: varInPos_b<=3; 5'o15: varInPos_b<=3; 
        5'o20: varInPos_b<=1; 5'o21: varInPos_b<=1; 5'o22: varInPos_b<=0; 5'o23: varInPos_b<=0; 5'o24: varInPos_b<=3; 5'o25: varInPos_b<=3; 
        5'o30: varInPos_b<=1; 5'o31: varInPos_b<=1; 5'o32: varInPos_b<=2; 5'o33: varInPos_b<=2; 5'o34: varInPos_b<=0; 5'o35: varInPos_b<=0; 
        default: varInPos_b<=2'bXX;
    endcase
    case({selectedSet, selectedPermutationInSet})
        5'o00: varInPos_c<=2; 5'o01: varInPos_c<=3; 5'o02: varInPos_c<=1; 5'o03: varInPos_c<=3; 5'o04: varInPos_c<=1; 5'o05: varInPos_c<=2; 
        5'o10: varInPos_c<=2; 5'o11: varInPos_c<=3; 5'o12: varInPos_c<=0; 5'o13: varInPos_c<=3; 5'o14: varInPos_c<=0; 5'o15: varInPos_c<=2; 
        5'o20: varInPos_c<=0; 5'o21: varInPos_c<=3; 5'o22: varInPos_c<=1; 5'o23: varInPos_c<=3; 5'o24: varInPos_c<=1; 5'o25: varInPos_c<=0; 
        5'o30: varInPos_c<=2; 5'o31: varInPos_c<=0; 5'o32: varInPos_c<=1; 5'o33: varInPos_c<=0; 5'o34: varInPos_c<=1; 5'o35: varInPos_c<=2; 
        default: varInPos_c<=2'bXX;
    endcase
    case({selectedSet, selectedPermutationInSet})
        5'o00: varInPos_d<=3; 5'o01: varInPos_d<=2; 5'o02: varInPos_d<=3; 5'o03: varInPos_d<=1; 5'o04: varInPos_d<=2; 5'o05: varInPos_d<=1; 
        5'o10: varInPos_d<=3; 5'o11: varInPos_d<=2; 5'o12: varInPos_d<=3; 5'o13: varInPos_d<=0; 5'o14: varInPos_d<=2; 5'o15: varInPos_d<=0; 
        5'o20: varInPos_d<=3; 5'o21: varInPos_d<=0; 5'o22: varInPos_d<=3; 5'o23: varInPos_d<=1; 5'o24: varInPos_d<=0; 5'o25: varInPos_d<=1; 
        5'o30: varInPos_d<=0; 5'o31: varInPos_d<=2; 5'o32: varInPos_d<=0; 5'o33: varInPos_d<=1; 5'o34: varInPos_d<=2; 5'o35: varInPos_d<=1; 
        default: varInPos_d<=2'bXX;
    endcase
    case({selectedSet, selectedPermutationInSet})
        5'o00: varInPos_ab<=0; 5'o01: varInPos_ab<=0; 5'o02: varInPos_ab<=1; 5'o03: varInPos_ab<=1; 5'o04: varInPos_ab<=2; 5'o05: varInPos_ab<=2; 
        5'o10: varInPos_ab<=0; 5'o11: varInPos_ab<=0; 5'o12: varInPos_ab<=4; 5'o13: varInPos_ab<=4; 5'o14: varInPos_ab<=5; 5'o15: varInPos_ab<=5; 
        5'o20: varInPos_ab<=4; 5'o21: varInPos_ab<=4; 5'o22: varInPos_ab<=1; 5'o23: varInPos_ab<=1; 5'o24: varInPos_ab<=6; 5'o25: varInPos_ab<=6; 
        5'o30: varInPos_ab<=5; 5'o31: varInPos_ab<=5; 5'o32: varInPos_ab<=6; 5'o33: varInPos_ab<=6; 5'o34: varInPos_ab<=2; 5'o35: varInPos_ab<=2; 
        default: varInPos_ab<=3'bXXX;
    endcase
    case({selectedSet, selectedPermutationInSet})
        5'o00: varInPos_ac<=1; 5'o01: varInPos_ac<=2; 5'o02: varInPos_ac<=0; 5'o03: varInPos_ac<=2; 5'o04: varInPos_ac<=0; 5'o05: varInPos_ac<=1; 
        5'o10: varInPos_ac<=4; 5'o11: varInPos_ac<=5; 5'o12: varInPos_ac<=0; 5'o13: varInPos_ac<=5; 5'o14: varInPos_ac<=0; 5'o15: varInPos_ac<=4; 
        5'o20: varInPos_ac<=1; 5'o21: varInPos_ac<=6; 5'o22: varInPos_ac<=4; 5'o23: varInPos_ac<=6; 5'o24: varInPos_ac<=4; 5'o25: varInPos_ac<=1; 
        5'o30: varInPos_ac<=6; 5'o31: varInPos_ac<=2; 5'o32: varInPos_ac<=5; 5'o33: varInPos_ac<=2; 5'o34: varInPos_ac<=5; 5'o35: varInPos_ac<=6; 
        default: varInPos_ac<=3'bXXX;
    endcase
    case({selectedSet, selectedPermutationInSet})
        5'o00: varInPos_ad<=2; 5'o01: varInPos_ad<=1; 5'o02: varInPos_ad<=2; 5'o03: varInPos_ad<=0; 5'o04: varInPos_ad<=1; 5'o05: varInPos_ad<=0; 
        5'o10: varInPos_ad<=5; 5'o11: varInPos_ad<=4; 5'o12: varInPos_ad<=5; 5'o13: varInPos_ad<=0; 5'o14: varInPos_ad<=4; 5'o15: varInPos_ad<=0; 
        5'o20: varInPos_ad<=6; 5'o21: varInPos_ad<=1; 5'o22: varInPos_ad<=6; 5'o23: varInPos_ad<=4; 5'o24: varInPos_ad<=1; 5'o25: varInPos_ad<=4; 
        5'o30: varInPos_ad<=2; 5'o31: varInPos_ad<=6; 5'o32: varInPos_ad<=2; 5'o33: varInPos_ad<=5; 5'o34: varInPos_ad<=6; 5'o35: varInPos_ad<=5; 
        default: varInPos_ad<=3'bXXX;
    endcase
end

wire[7:0] permutedBotParts[15:0];

assign permutedBotParts[4'b0000] = bot[4'b0000];
assign permutedBotParts[4'b1111] = bot[4'b1111];

assign permutedBotParts[4'b0001] = mux4(varInPos_a, bot[4'b0001], bot[4'b0010], bot[4'b0100], bot[4'b1000]);
assign permutedBotParts[4'b0010] = mux4(varInPos_b, bot[4'b0001], bot[4'b0010], bot[4'b0100], bot[4'b1000]);
assign permutedBotParts[4'b0100] = mux4(varInPos_c, bot[4'b0001], bot[4'b0010], bot[4'b0100], bot[4'b1000]);
assign permutedBotParts[4'b1000] = mux4(varInPos_d, bot[4'b0001], bot[4'b0010], bot[4'b0100], bot[4'b1000]);

assign permutedBotParts[4'b1110] = mux4(varInPos_a, bot[4'b1110], bot[4'b1101], bot[4'b1011], bot[4'b0111]);
assign permutedBotParts[4'b1101] = mux4(varInPos_b, bot[4'b1110], bot[4'b1101], bot[4'b1011], bot[4'b0111]);
assign permutedBotParts[4'b1011] = mux4(varInPos_c, bot[4'b1110], bot[4'b1101], bot[4'b1011], bot[4'b0111]);
assign permutedBotParts[4'b0111] = mux4(varInPos_d, bot[4'b1110], bot[4'b1101], bot[4'b1011], bot[4'b0111]);

assign permutedBotParts[4'b0011] = mux6(varInPos_ab, bot[4'b0011], bot[4'b0101], bot[4'b1001], bot[4'b0110], bot[4'b1010], bot[4'b1100]);
assign permutedBotParts[4'b0101] = mux6(varInPos_ac, bot[4'b0011], bot[4'b0101], bot[4'b1001], bot[4'b0110], bot[4'b1010], bot[4'b1100]);
assign permutedBotParts[4'b1001] = mux6(varInPos_ad, bot[4'b0011], bot[4'b0101], bot[4'b1001], bot[4'b0110], bot[4'b1010], bot[4'b1100]);
assign permutedBotParts[4'b0110] = mux6(varInPos_ad, bot[4'b1100], bot[4'b1010], bot[4'b0110], bot[4'b1001], bot[4'b0101], bot[4'b0011]);
assign permutedBotParts[4'b1010] = mux6(varInPos_ac, bot[4'b1100], bot[4'b1010], bot[4'b0110], bot[4'b1001], bot[4'b0101], bot[4'b0011]);
assign permutedBotParts[4'b1100] = mux6(varInPos_ab, bot[4'b1100], bot[4'b1010], bot[4'b0110], bot[4'b1001], bot[4'b0101], bot[4'b0011]);

genvar i;
generate
for(i = 0; i < 16; i = i + 1) begin
    assign bot[i] = storedBot[8*i +: 8];
    assign permutedBot[8*i +: 8] = permutedBotParts[i];
end
endgenerate


endmodule

module botPermuter1234(
    input clk,
    
    // Input side
    input writeDataIn,
    input[127:0] botIn,
    input[23:0] validBotPermutesIn,
    input lastBotOfBatch,
    output grabDataIn,
    
    // Output side
    input slowDown,
    output reg permutedBotValid,
    output[127:0] permutedBot,
    output reg batchDone
);

reg[127:0] storedBot;
reg[23:0] storedValidPermutes;
reg storedLastBotOfBatch;

always @(posedge clk) begin
    if(writeDataIn) begin
        storedBot <= botIn;
        storedValidPermutes <= validBotPermutesIn;
        storedLastBotOfBatch <= lastBotOfBatch;
    end else begin
        
    end
end

endmodule
