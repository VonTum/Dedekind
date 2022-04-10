`timescale 1ns / 1ps



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
    input[7:0] v0, v1, v2, v3, v4, v5;
    begin
        case(sel)
            0: mux6 = v0;
            1: mux6 = v1;
            2: mux6 = v2;
            3: mux6 = v3;
            4: mux6 = v4;
            5: mux6 = v5;
            default: mux6 = 8'bXXXXXXXX;
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
        5'o10: varInPos_ab<=0; 5'o11: varInPos_ab<=0; 5'o12: varInPos_ab<=3; 5'o13: varInPos_ab<=3; 5'o14: varInPos_ab<=4; 5'o15: varInPos_ab<=4; 
        5'o20: varInPos_ab<=3; 5'o21: varInPos_ab<=3; 5'o22: varInPos_ab<=1; 5'o23: varInPos_ab<=1; 5'o24: varInPos_ab<=5; 5'o25: varInPos_ab<=5; 
        5'o30: varInPos_ab<=4; 5'o31: varInPos_ab<=4; 5'o32: varInPos_ab<=5; 5'o33: varInPos_ab<=5; 5'o34: varInPos_ab<=2; 5'o35: varInPos_ab<=2; 
        default: varInPos_ab<=3'bXXX;
    endcase
    case({selectedSet, selectedPermutationInSet})
        5'o00: varInPos_ac<=1; 5'o01: varInPos_ac<=2; 5'o02: varInPos_ac<=0; 5'o03: varInPos_ac<=2; 5'o04: varInPos_ac<=0; 5'o05: varInPos_ac<=1; 
        5'o10: varInPos_ac<=3; 5'o11: varInPos_ac<=4; 5'o12: varInPos_ac<=0; 5'o13: varInPos_ac<=4; 5'o14: varInPos_ac<=0; 5'o15: varInPos_ac<=3; 
        5'o20: varInPos_ac<=1; 5'o21: varInPos_ac<=5; 5'o22: varInPos_ac<=3; 5'o23: varInPos_ac<=5; 5'o24: varInPos_ac<=3; 5'o25: varInPos_ac<=1; 
        5'o30: varInPos_ac<=5; 5'o31: varInPos_ac<=2; 5'o32: varInPos_ac<=4; 5'o33: varInPos_ac<=2; 5'o34: varInPos_ac<=4; 5'o35: varInPos_ac<=5; 
        default: varInPos_ac<=3'bXXX;
    endcase
    case({selectedSet, selectedPermutationInSet})
        5'o00: varInPos_ad<=2; 5'o01: varInPos_ad<=1; 5'o02: varInPos_ad<=2; 5'o03: varInPos_ad<=0; 5'o04: varInPos_ad<=1; 5'o05: varInPos_ad<=0; 
        5'o10: varInPos_ad<=4; 5'o11: varInPos_ad<=3; 5'o12: varInPos_ad<=4; 5'o13: varInPos_ad<=0; 5'o14: varInPos_ad<=3; 5'o15: varInPos_ad<=0; 
        5'o20: varInPos_ad<=5; 5'o21: varInPos_ad<=1; 5'o22: varInPos_ad<=5; 5'o23: varInPos_ad<=3; 5'o24: varInPos_ad<=1; 5'o25: varInPos_ad<=3; 
        5'o30: varInPos_ad<=2; 5'o31: varInPos_ad<=5; 5'o32: varInPos_ad<=2; 5'o33: varInPos_ad<=4; 5'o34: varInPos_ad<=5; 5'o35: varInPos_ad<=4; 
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

module botPermute1234SelectionGenerator(
    input clk,
    
    // Input side
    input[23:0] validBotPermutesIn,
    input lastBotOfBatch,
    output acceptData,
    
    // Output side
    input slowDown,
    output reg permutedBotValid,
    output reg[1:0] selectedSet,
    output reg[2:0] selectedPermutationInSet,
    output reg batchDone
);

reg[3:0] sections[5:0];

integer ii;
initial begin
    for(ii = 0; ii < 6; ii = ii + 1) begin
        sections[ii] = 4'b0000;
    end
end

wire[5:0] sectionIsZero;
/*wire*/reg[5:0] sectionHasExactlyOneBit;

wire[1:0] tripleSectionIsZero;
/*wire*/reg[1:0] tripleSectionHasExactlyOneBit;

genvar i;
generate
for(i = 0; i < 6; i=i+1) begin
    assign sectionIsZero[i] = sections[i] == 4'b0000;
    always @(*) begin
        case(sections[i])
            4'b0001: sectionHasExactlyOneBit[i] = 1;
            4'b0010: sectionHasExactlyOneBit[i] = 1;
            4'b0100: sectionHasExactlyOneBit[i] = 1;
            4'b1000: sectionHasExactlyOneBit[i] = 1;
            default: sectionHasExactlyOneBit[i] = 0;
        endcase
    end
end

for(i = 0; i < 2; i=i+1) begin
    always @(*) begin
        case({sectionIsZero[i*3+:3], sectionHasExactlyOneBit[i*3+:3]})
            6'b110001: tripleSectionHasExactlyOneBit[i] = 1;
            6'b101010: tripleSectionHasExactlyOneBit[i] = 1;
            6'b011100: tripleSectionHasExactlyOneBit[i] = 1;
            default: tripleSectionHasExactlyOneBit[i] = 0;
        endcase
    end
    assign tripleSectionIsZero[i] = &sectionIsZero[i*3+:3];
end
endgenerate

wire[3:0] triplesCombined = {tripleSectionIsZero, tripleSectionHasExactlyOneBit};
assign acceptData = (triplesCombined == 4'b1001 || triplesCombined == 4'b0110 || tripleSectionIsZero == 2'b11) && !slowDown;


/*wire*/reg[1:0] firstBitInSection[5:0];
wire[5:0] isClearingSection;
generate
for(i = 0; i < 6; i=i+1) begin
    always @(*) begin
        casez(sections[i])
            4'b???1: firstBitInSection[i] = 0;
            4'b??10: firstBitInSection[i] = 1;
            4'b?100: firstBitInSection[i] = 2;
            4'b1000: firstBitInSection[i] = 3;
            4'b0000: firstBitInSection[i] = 2'bXX;
        endcase
    end
end
assign isClearingSection[0] = 1;
for(i = 1; i < 6; i=i+1) begin
    assign isClearingSection[i] = &sectionIsZero[i-1:0]; // All previous sections are 0
end
endgenerate

/*wire*/reg[2:0] selectedPermutationInSetPre;
always @(*) begin
    casez(sectionIsZero[4:0])
        5'b????0: selectedPermutationInSetPre = 0;
        5'b???01: selectedPermutationInSetPre = 1;
        5'b??011: selectedPermutationInSetPre = 2;
        5'b?0111: selectedPermutationInSetPre = 3;
        5'b01111: selectedPermutationInSetPre = 4;
        5'b11111: selectedPermutationInSetPre = 5;
    endcase
end

always @(posedge clk) begin
    if(!slowDown) begin
        for(ii = 0; ii < 6; ii=ii+1) begin
            if(acceptData) begin
                sections[ii] <= {validBotPermutesIn[ii],validBotPermutesIn[6+ii],validBotPermutesIn[12+ii],validBotPermutesIn[18+ii]};
            end else begin
                if(isClearingSection[ii]) begin
                    sections[ii][firstBitInSection[ii]] <= 0;
                end
            end
        end
    end
end

reg storedLastBotOfBatch;
always @(posedge clk) begin
    if(acceptData) begin
        storedLastBotOfBatch <= lastBotOfBatch;
    end
    
    selectedPermutationInSet <= selectedPermutationInSetPre;
    selectedSet <= firstBitInSection[selectedPermutationInSetPre];
    permutedBotValid <= tripleSectionIsZero != 2'b11 && !slowDown;
    batchDone <= acceptData ? storedLastBotOfBatch : 0;
end

endmodule

module botPermuterLowLatencyFIFO #(parameter ALMOST_FULL_MARGIN = 64, parameter BOT_DELAY = 2, parameter DEPTH_LOG2 = 9) (
    input clk,
    input rst,
    
    input write,
    input[127:0] botIn,
    input[23:0] validBotPermutesIn,
    input lastBotOfBatchIn,
    output reg almostFull,
    
    input accept,
    output[127:0] storedBot,
    output[23:0] nextValidBotPermutes,
    output nextLastBotOfBatch,
    output reg eccStatus
);

// Initial values everywhere otherwise this breaks botPermute1234SelectionGenerator in simulation
reg[DEPTH_LOG2-1:0] writeAddr = 1;
reg[DEPTH_LOG2-1:0] readAddr = 0;
wire[DEPTH_LOG2-1:0] nextValidPermutesReadAddr = readAddr + 1;

wire noNewDataAvailable = nextValidPermutesReadAddr == writeAddr;
reg curAddrIsNotNew = 1;
reg storedAddrIsNotNew = 1;

always @(posedge clk) begin
    if(rst) begin
        writeAddr <= 1;
        readAddr <= 0;
    end else begin
        if(write) begin
            writeAddr <= writeAddr + 1;
        end
        if(accept && !noNewDataAvailable) begin
            readAddr <= nextValidPermutesReadAddr;
        end
    end
    
    if(accept) begin
        curAddrIsNotNew <= noNewDataAvailable;
        storedAddrIsNotNew <= curAddrIsNotNew;
    end
    
    almostFull <= (readAddr - writeAddr) < ALMOST_FULL_MARGIN;
end

wire botM20KECC;
wire validPermutesM20KECC;
always @(posedge clk) eccStatus <= botM20KECC || validPermutesM20KECC;

LOW_LATENCY_M20K #(.WIDTH(24+1), .DEPTH_LOG2(DEPTH_LOG2), .USE_SCLEAR(1)) validPermutesM20K (
    // Write Side
    .wrclk(clk),
    .writeEnable(write),
    .writeAddr(writeAddr),
    .dataIn({validBotPermutesIn, lastBotOfBatchIn}),
    
    // Read Side
    .rdclk(clk),
    .readClockEnable(accept),
    .readAddr(readAddr),
    .dataOut({nextValidBotPermutes, nextLastBotOfBatch}),
    .eccStatus(validPermutesM20KECC),
    
    // Optional read output reg sclear
    .rstOutReg(storedAddrIsNotNew)
);


wire acceptD;
hyperpipe #(.CYCLES(BOT_DELAY)) acceptPipe(clk, accept, acceptD);
wire[DEPTH_LOG2-1:0] readAddrD;
hyperpipe #(.CYCLES(BOT_DELAY), .WIDTH(DEPTH_LOG2)) readAddrPipe(clk, readAddr, readAddrD);

reg[DEPTH_LOG2-1:0] botReadAddr;

always @(posedge clk) begin
    if(acceptD) begin
        botReadAddr <= readAddrD;
    end
end

LOW_LATENCY_M20K #(.WIDTH(128), .DEPTH_LOG2(DEPTH_LOG2), .USE_SCLEAR(0)) botM20K (
    // Write Side
    .wrclk(clk),
    .writeEnable(write),
    .writeAddr(writeAddr),
    .dataIn(botIn),
    
    // Read Side
    .rdclk(clk),
    .readClockEnable(acceptD),
    .readAddr(botReadAddr),
    .dataOut(storedBot),
    .eccStatus(botM20KECC),
    
    // Optional read output reg sclear
    .rstOutReg(1'b0)
);

endmodule

module botPermuter1234 #(parameter ALMOST_FULL_MARGIN = 64) (
    input clk,
    input rst,
    
    // Input side
    input writeDataIn,
    input[127:0] botIn,
    input[23:0] validBotPermutesIn,
    input lastBotOfBatchIn,
    output almostFull,
    
    // Output side
    input slowDown,
    output reg permutedBotValid,
    output[127:0] permutedBot,
    output reg batchDone,
    output eccStatus
);

wire[127:0] storedBotFromFIFO;

wire acceptToFIFO;
wire[23:0] nextValidBotPermutesFromFIFO;
wire nextLastBotOfBatchFromFIFO;

botPermuterLowLatencyFIFO #(.ALMOST_FULL_MARGIN(ALMOST_FULL_MARGIN), .BOT_DELAY(2)) fifo (
    .clk(clk),
    .rst(rst),
    
    .write(writeDataIn),
    .botIn(botIn),
    .validBotPermutesIn(validBotPermutesIn),
    .lastBotOfBatchIn(lastBotOfBatchIn),
    .almostFull(almostFull),
    
    .accept(acceptToFIFO),
    .storedBot(storedBotFromFIFO),
    .nextValidBotPermutes(nextValidBotPermutesFromFIFO),
    .nextLastBotOfBatch(nextLastBotOfBatchFromFIFO),
    .eccStatus(eccStatus)
);

wire permutedSelectionValid;
wire[1:0] selectedSet;
wire[2:0] selectedPermutationInSet;
wire selectionBatchDone;
botPermute1234SelectionGenerator selectionGenerator (
    .clk(clk),
    
    // Input side
    .validBotPermutesIn(nextValidBotPermutesFromFIFO),
    .lastBotOfBatch(nextLastBotOfBatchFromFIFO),
    .acceptData(acceptToFIFO),
    
    // Output side
    .slowDown(slowDown),
    .permutedBotValid(permutedSelectionValid),
    .selectedSet(selectedSet),
    .selectedPermutationInSet(selectedPermutationInSet),
    .batchDone(selectionBatchDone)
);

permute1234 permuter (
    .clk(clk),
    
    .storedBot(storedBotFromFIFO),
    .selectedSet(selectedSet), // 0-3
    .selectedPermutationInSet(selectedPermutationInSet), // 0-5
    .permutedBot(permutedBot)
);

always @(posedge clk) begin
    permutedBotValid <= permutedSelectionValid;
    batchDone <= selectionBatchDone;
end

endmodule
