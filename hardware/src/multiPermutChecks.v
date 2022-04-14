`timescale 1ns / 1ps

/*
Intermediaries format:

wire[8:0] oneTwoVarOverlaps; // 3x3 array: maps flavor to location X__, _X_, __X, in flavors a,b,c   indexed [location*3+flavor]
*   a__, b__, c__
*   _a_, _b_, _c_
*   __a, __b, __c

wire[15:0] oneThreeVarOverlaps; // 4x4 array: maps flavor to location X___, _X__, __X_, ___X, in flavors a,b,c,d  indexed [location*4+flavor]
*   a___, b___, c___, d___
*   _a__, _b__, _c__, _d__
*   __a_, __b_, __c_, __d_
*   ___a, ___b, ___c, ___d

wire[17:0] twoTwoVarOverlaps; // 3x6 array: for the shapes XX__, X_X_ and X__X, in flavors ab, ac, ad, cd, bd, bc  indexed [location*6+flavor]
*   ab__, ac__, ad__, cd__, bd__, bc__
*   a_b_, a_c_, a_d_, c_d_, b_d_, b_c_
*   a__b, a__c, a__d, c__d, b__d, b__c

*/



/*********************************************
  Produce IsBelow Checks From Intermediaries
*********************************************/

module permutCheckProduceResults6(
    input sharedAll,
    input[8:0] oneTwoVars, // indexed [3*position+flavor], so aCb becomes 3*0+2=5
    output[5:0] results // {abc, acb, bac, bca, cab, cba}
);

`define COMPUTE_WIRE(v1, v2, v3) &{sharedAll, oneTwoVars[3*0+v1], oneTwoVars[3*1+v2], oneTwoVars[3*2+v3]}
wire abc = `COMPUTE_WIRE(0,1,2);
wire acb = `COMPUTE_WIRE(0,2,1);
wire bac = `COMPUTE_WIRE(1,0,2);
wire bca = `COMPUTE_WIRE(1,2,0);
wire cab = `COMPUTE_WIRE(2,0,1);
wire cba = `COMPUTE_WIRE(2,1,0);

assign results = {abc, acb, bac, bca, cab, cba};

endmodule

module permutCheckProduceResults6Pipelined (
    input clk,
    input sharedAll,
    input[8:0] oneTwoVars, // indexed [3*position+flavor], so aCb becomes 3*0+2=5
    output reg[5:0] results // {abc, acb, bac, bca, cab, cba}
);

`define COMPUTE_WIRE(v1, v2, v3) &{sharedAll, oneTwoVars[3*0+v1], oneTwoVars[3*1+v2], oneTwoVars[3*2+v3]}
wire abc = `COMPUTE_WIRE(0,1,2);
wire acb = `COMPUTE_WIRE(0,2,1);
wire bac = `COMPUTE_WIRE(1,0,2);
wire bca = `COMPUTE_WIRE(1,2,0);
wire cab = `COMPUTE_WIRE(2,0,1);
wire cba = `COMPUTE_WIRE(2,1,0);

always @(posedge clk) results <= {abc, acb, bac, bca, cab, cba};

endmodule

module permutCheckProduceResults24(
    input sharedAll,
    input[15:0] oneThreeVars24, // intermediaries
    input[17:0] twoTwoVars24,  // intermediaries
    output[23:0] results /* {
        abcd, abdc, acbd, acdb, adbc, adcb, 
        bacd, badc, bcad, bcda, bdac, bdca, 
        cbad, cbda, cabd, cadb, cdba, cdab, 
        dbca, dbac, dcba, dcab, dabc, dacb}
         == {permutesABCD, permutesBACD, permutesCBAD, permutesDBCA}*/
);

wire aShared;
wire[8:0] aOneTwo6;
wire bShared;
wire[8:0] bOneTwo6;
wire cShared;
wire[8:0] cOneTwo6;
wire dShared;
wire[8:0] dOneTwo6;

permutCvAllIntermediaries24To6 cvt(sharedAll, oneThreeVars24, twoTwoVars24, 
    aShared, aOneTwo6,
    bShared, bOneTwo6,
    cShared, cOneTwo6,
    dShared, dOneTwo6
);

wire[5:0] aResult;
permutCheckProduceResults6 aResultProd(aShared, aOneTwo6, aResult);

wire[5:0] bResult;
permutCheckProduceResults6 bResultProd(bShared, bOneTwo6, bResult);

wire[5:0] cResult;
permutCheckProduceResults6 cResultProd(cShared, cOneTwo6, cResult);

wire[5:0] dResult;
permutCheckProduceResults6 dResultProd(dShared, dOneTwo6, dResult);

assign results = {aResult, bResult, cResult, dResult};

endmodule

module permutCheckProduceResults24Pipelined(
    input clk,
    input sharedAll,
    input[15:0] oneThreeVars24, // intermediaries
    input[17:0] twoTwoVars24,  // intermediaries
    output[23:0] results /* {
        abcd, abdc, acbd, acdb, adbc, adcb, 
        bacd, badc, bcad, bcda, bdac, bdca, 
        cbad, cbda, cabd, cadb, cdba, cdab, 
        dbca, dbac, dcba, dcab, dabc, dacb}
         == {permutesABCD, permutesBACD, permutesCBAD, permutesDBCA}*/
);

wire aShared;
wire[8:0] aOneTwo6;
wire bShared;
wire[8:0] bOneTwo6;
wire cShared;
wire[8:0] cOneTwo6;
wire dShared;
wire[8:0] dOneTwo6;

permutCvAllIntermediaries24To6 cvt(sharedAll, oneThreeVars24, twoTwoVars24, 
    aShared, aOneTwo6,
    bShared, bOneTwo6,
    cShared, cOneTwo6,
    dShared, dOneTwo6
);

reg aSharedD; always @(posedge clk) aSharedD <= aShared;
reg[8:0] aOneTwo6D; always @(posedge clk) aOneTwo6D <= aOneTwo6;
wire[5:0] aResult;
permutCheckProduceResults6Pipelined aResultProd(clk, aSharedD, aOneTwo6D, aResult);

reg bSharedD; always @(posedge clk) bSharedD <= bShared;
reg[8:0] bOneTwo6D; always @(posedge clk) bOneTwo6D <= bOneTwo6;
wire[5:0] bResult;
permutCheckProduceResults6Pipelined bResultProd(clk, bSharedD, bOneTwo6D, bResult);

reg cSharedD; always @(posedge clk) cSharedD <= cShared;
reg[8:0] cOneTwo6D; always @(posedge clk) cOneTwo6D <= cOneTwo6;
wire[5:0] cResult;
permutCheckProduceResults6Pipelined cResultProd(clk, cSharedD, cOneTwo6D, cResult);

reg dSharedD; always @(posedge clk) dSharedD <= dShared;
reg[8:0] dOneTwo6D; always @(posedge clk) dOneTwo6D <= dOneTwo6;
wire[5:0] dResult;
permutCheckProduceResults6Pipelined dResultProd(clk, dSharedD, dOneTwo6D, dResult);

assign results = {aResult, bResult, cResult, dResult};

endmodule





/****************
  Convert Down
****************/

module permutCvtIntermediaries24to6 #(
    parameter REMOVED_VAR = 0,
    parameter TWOTWO_IDX_A = 0,
    parameter TWOTWO_IDX_B = 1,
    parameter TWOTWO_IDX_C = 2
) (
    input sharedAll,
    input[15:0] oneThreeVarOverlaps24, // 4x4 array: maps flavor to location X___, _X__, __X_, ___X, in flavors a,b,c,d
    input[17:0] twoTwoVarOverlaps24, // 3x6 array: for the shapes XX__, X_X_ and X__X, in flavors ab, ac, ad, cd, bd, bc
    output sharedOut,
    output[8:0] oneTwoVarOverlaps6
);

function automatic integer swapRemovedVarWithZero;
    input integer v;
    begin
        if(v == REMOVED_VAR)
            swapRemovedVarWithZero = 0;
        else
            swapRemovedVarWithZero = v;
    end
endfunction

assign sharedOut = sharedAll & oneThreeVarOverlaps24[4*0+REMOVED_VAR];

generate
for(genvar i = 0; i < 3; i=i+1) begin
    // list all items having REMOVED_VAR at the front. 
    assign oneTwoVarOverlaps6[i*3+0] = oneThreeVarOverlaps24[4*(i+1)+swapRemovedVarWithZero(1)] & twoTwoVarOverlaps24[6*i+TWOTWO_IDX_A];
    assign oneTwoVarOverlaps6[i*3+1] = oneThreeVarOverlaps24[4*(i+1)+swapRemovedVarWithZero(2)] & twoTwoVarOverlaps24[6*i+TWOTWO_IDX_B];
    assign oneTwoVarOverlaps6[i*3+2] = oneThreeVarOverlaps24[4*(i+1)+swapRemovedVarWithZero(3)] & twoTwoVarOverlaps24[6*i+TWOTWO_IDX_C];
end
endgenerate

endmodule


module permutCvAllIntermediaries24To6(
    input sharedAll,
    input[15:0] oneThreeVars24, // intermediaries
    input[17:0] twoTwoVars24,  // intermediaries
    
    output aShared,
    output[8:0] aOneTwo6,
    output bShared,
    output[8:0] bOneTwo6,
    output cShared,
    output[8:0] cOneTwo6,
    output dShared,
    output[8:0] dOneTwo6
);

permutCvtIntermediaries24to6 #(0,0,1,2) aRemoved(sharedAll, oneThreeVars24, twoTwoVars24, aShared, aOneTwo6);
permutCvtIntermediaries24to6 #(1,0,5,4) bRemoved(sharedAll, oneThreeVars24, twoTwoVars24, bShared, bOneTwo6);
permutCvtIntermediaries24to6 #(2,5,1,3) cRemoved(sharedAll, oneThreeVars24, twoTwoVars24, cShared, cOneTwo6);
permutCvtIntermediaries24to6 #(3,4,3,2) dRemoved(sharedAll, oneThreeVars24, twoTwoVars24, dShared, dOneTwo6);

endmodule




/*****************
  Intermediaries
*****************/

module permuteProduceIntermediaries6 (
    input[127:0] top,
    input[127:0] bot,
    output sharedAll,
    output[8:0] oneTwoVarOverlaps
);

wire[15:0] topParts[7:0];
wire[15:0] botParts[7:0];

generate
for(genvar i = 0; i < 8; i = i + 1) begin
    assign topParts[i] = top[i*16+:16];
    assign botParts[i] = bot[i*16+:16];
end
endgenerate

assign sharedAll = &({topParts[0], topParts[7]} | ~{botParts[0], botParts[7]});

wire[15:0] oneVarTops[2:0]; // indexed a, b, c
assign oneVarTops[0] = topParts[3'b001];
assign oneVarTops[1] = topParts[3'b010];
assign oneVarTops[2] = topParts[3'b100];
wire[15:0] oneVarBots[2:0];
assign oneVarBots[0] = botParts[3'b001];
assign oneVarBots[1] = botParts[3'b010];
assign oneVarBots[2] = botParts[3'b100];

wire[15:0] twoVarTops[2:0]; // indexed bc, ac, ab
assign twoVarTops[0] = topParts[3'b110];
assign twoVarTops[1] = topParts[3'b101];
assign twoVarTops[2] = topParts[3'b011];
wire[15:0] twoVarBots[2:0];
assign twoVarBots[0] = botParts[3'b110];
assign twoVarBots[1] = botParts[3'b101];
assign twoVarBots[2] = botParts[3'b011];

// maps var j to location i
generate
for(genvar i = 0; i < 3; i=i+1) begin
    for(genvar j = 0; j < 3; j=j+1) begin
        assign oneTwoVarOverlaps[i * 3 + j] = &({oneVarTops[i], twoVarTops[i]} | ~{oneVarBots[j], twoVarBots[j]});
    end
end
endgenerate

endmodule



module permuteProduceIntermediaries24 (
    input[127:0] top,
    input[127:0] bot,
    output sharedAll,
    output[15:0] oneThreeVarOverlaps,
    output[17:0] twoTwoVarOverlaps
);

wire[7:0] topParts[15:0];
wire[7:0] botParts[15:0];

generate
for(genvar i = 0; i < 16; i = i + 1) begin
    assign topParts[i] = top[i*8+:8];
    assign botParts[i] = bot[i*8+:8];
end
endgenerate

assign sharedAll = &({topParts[0], topParts[15]} | ~{botParts[0],botParts[15]});

wire[7:0] oneVarTops[3:0]; // indexed a, b, c, d
assign oneVarTops[0] = topParts[4'b0001];assign oneVarTops[1] = topParts[4'b0010];
assign oneVarTops[2] = topParts[4'b0100];assign oneVarTops[3] = topParts[4'b1000];
wire[7:0] oneVarBots[3:0];
assign oneVarBots[0] = botParts[4'b0001];assign oneVarBots[1] = botParts[4'b0010];
assign oneVarBots[2] = botParts[4'b0100];assign oneVarBots[3] = botParts[4'b1000];

wire[7:0] twoVarTops[5:0]; // indexed ab, ac, ad, cd, bd, bc
assign twoVarTops[0] = topParts[4'b0011];assign twoVarTops[1] = topParts[4'b0101];assign twoVarTops[2] = topParts[4'b1001];
assign twoVarTops[3] = topParts[4'b1100];assign twoVarTops[4] = topParts[4'b1010];assign twoVarTops[5] = topParts[4'b0110];
wire[7:0] twoVarBots[5:0];
assign twoVarBots[0] = botParts[4'b0011];assign twoVarBots[1] = botParts[4'b0101];assign twoVarBots[2] = botParts[4'b1001];
assign twoVarBots[3] = botParts[4'b1100];assign twoVarBots[4] = botParts[4'b1010];assign twoVarBots[5] = botParts[4'b0110];

wire[7:0] threeVarTops[3:0]; // indexed bcd, acd, abd, abc
assign threeVarTops[0] = topParts[4'b1110];assign threeVarTops[1] = topParts[4'b1101];
assign threeVarTops[2] = topParts[4'b1011];assign threeVarTops[3] = topParts[4'b0111];
wire[7:0] threeVarBots[3:0];
assign threeVarBots[0] = botParts[4'b1110];assign threeVarBots[1] = botParts[4'b1101];
assign threeVarBots[2] = botParts[4'b1011];assign threeVarBots[3] = botParts[4'b0111];


//wire[15:0] oneThreeVarOverlaps; // 4x4 array: maps flavor to location X___, _X__, __X_, ___X, in flavors a,b,c,d
//wire[17:0] twoTwoVarOverlaps; // 3x6 array: for the shapes XX__, X_X_ and X__X, in flavors ab, ac, ad, cd, bd, bc
/*
    ab__, ac__, ad__, cd__, bd__, bc__
    a_b_, a_c_, a_d_, c_d_, b_d_, b_c_
    a__b, a__c, a__d, c__d, b__d, b__c
*/

// maps var j to location i
generate
for(genvar i = 0; i < 4; i=i+1) begin
    for(genvar j = 0; j < 4; j=j+1) begin
        assign oneThreeVarOverlaps[i * 4 + j] = &((oneVarTops[i] | ~oneVarBots[j]) & (threeVarTops[i] | ~threeVarBots[j]));
    end
end
for(genvar i = 0; i < 3; i=i+1) begin
    for(genvar j = 0; j < 6; j=j+1) begin
        assign twoTwoVarOverlaps[i * 6 + j] = &((twoVarTops[i] | ~twoVarBots[j]) & (twoVarTops[i+3] | ~twoVarBots[(j+3) % 6])); // joins ab witb cd, ad with bc etc. 
    end
end
endgenerate

endmodule



module permuteProduceIntermediaries24Pipelined (
    input clk,
    input[127:0] top,
    input[127:0] bot,
    input botValid,
    output reg sharedAll,
    output reg[15:0] oneThreeVarOverlaps,
    output reg[17:0] twoTwoVarOverlaps
);

wire[7:0] topParts[15:0];
wire[7:0] botParts[15:0];

generate
for(genvar i = 0; i < 16; i = i + 1) begin
    assign topParts[i] = top[i*8+:8];
    assign botParts[i] = bot[i*8+:8];
end
endgenerate

always @(posedge clk) sharedAll <= (&({topParts[0], topParts[15]} | ~{botParts[0],botParts[15]})) && botValid;

wire[7:0] oneVarTops[3:0]; // indexed a, b, c, d
assign oneVarTops[0] = topParts[4'b0001];assign oneVarTops[1] = topParts[4'b0010];
assign oneVarTops[2] = topParts[4'b0100];assign oneVarTops[3] = topParts[4'b1000];
wire[7:0] oneVarBots[3:0];
assign oneVarBots[0] = botParts[4'b0001];assign oneVarBots[1] = botParts[4'b0010];
assign oneVarBots[2] = botParts[4'b0100];assign oneVarBots[3] = botParts[4'b1000];

wire[7:0] twoVarTops[5:0]; // indexed ab, ac, ad, cd, bd, bc
assign twoVarTops[0] = topParts[4'b0011];assign twoVarTops[1] = topParts[4'b0101];assign twoVarTops[2] = topParts[4'b1001];
assign twoVarTops[3] = topParts[4'b1100];assign twoVarTops[4] = topParts[4'b1010];assign twoVarTops[5] = topParts[4'b0110];
wire[7:0] twoVarBots[5:0];
assign twoVarBots[0] = botParts[4'b0011];assign twoVarBots[1] = botParts[4'b0101];assign twoVarBots[2] = botParts[4'b1001];
assign twoVarBots[3] = botParts[4'b1100];assign twoVarBots[4] = botParts[4'b1010];assign twoVarBots[5] = botParts[4'b0110];

wire[7:0] threeVarTops[3:0]; // indexed bcd, acd, abd, abc
assign threeVarTops[0] = topParts[4'b1110];assign threeVarTops[1] = topParts[4'b1101];
assign threeVarTops[2] = topParts[4'b1011];assign threeVarTops[3] = topParts[4'b0111];
wire[7:0] threeVarBots[3:0];
assign threeVarBots[0] = botParts[4'b1110];assign threeVarBots[1] = botParts[4'b1101];
assign threeVarBots[2] = botParts[4'b1011];assign threeVarBots[3] = botParts[4'b0111];


//wire[15:0] oneThreeVarOverlaps; // 4x4 array: maps flavor to location X___, _X__, __X_, ___X, in flavors a,b,c,d
//wire[17:0] twoTwoVarOverlaps; // 3x6 array: for the shapes XX__, X_X_ and X__X, in flavors ab, ac, ad, cd, bd, bc
/*
    ab__, ac__, ad__, cd__, bd__, bc__
    a_b_, a_c_, a_d_, c_d_, b_d_, b_c_
    a__b, a__c, a__d, c__d, b__d, b__c
*/

// maps var j to location i
generate
for(genvar i = 0; i < 4; i=i+1) begin
    for(genvar j = 0; j < 4; j=j+1) begin
        always @(posedge clk) oneThreeVarOverlaps[i * 4 + j] <= &((oneVarTops[i] | ~oneVarBots[j]) & (threeVarTops[i] | ~threeVarBots[j]));
    end
end
for(genvar i = 0; i < 3; i=i+1) begin
    for(genvar j = 0; j < 6; j=j+1) begin
        always @(posedge clk) twoTwoVarOverlaps[i * 6 + j] <= &((twoVarTops[i] | ~twoVarBots[j]) & (twoVarTops[i+3] | ~twoVarBots[(j+3) % 6])); // joins ab witb cd, ad with bc etc. 
    end
end
endgenerate

endmodule


/***************
  ALL Combined
***************/


// swaps variable 5 and 6
module permuteCheck2 (
    input[127:0] top,
    input[127:0] bot,
    input isBotValid,
    output[1:0] validBotPermutations
);

// checks if bot has a bit that top doesn't, in the fields containing neither x or y, or both x and y
wire shared = &({top[31:0], top[127:96]} | ~{bot[31:0], bot[127:96]}); 
wire unswappedBits = &({top[63:32], top[95:64]} | ~{bot[63:32], bot[95:64]});
wire swappedBits = &({top[95:64], top[63:32]} | ~{bot[63:32], bot[95:64]});

wire sharedAll = shared & isBotValid;
wire isBelowUnswapped = sharedAll & unswappedBits;
wire isBelowSwapped = sharedAll & swappedBits;

assign validBotPermutations = {isBelowUnswapped, isBelowSwapped};

endmodule

module permuteCheck6 (
    input[127:0] top,
    input[127:0] bot,
    input isBotValid,
    output[5:0] validBotPermutations
);

wire sharedAll;
wire[8:0] oneTwoVarOverlaps;

permuteProduceIntermediaries6 intermediaryProducer(top, bot, sharedAll, oneTwoVarOverlaps);
permutCheckProduceResults6 resultsProducer(sharedAll & isBotValid, oneTwoVarOverlaps, validBotPermutations);

endmodule


module permuteCheck24 (
    input[127:0] top,
    input[127:0] bot,
    input isBotValid,
    output[23:0] validBotPermutations /* {
        abcd, abdc, acbd, acdb, adbc, adcb, 
        bacd, badc, bcad, bcda, bdac, bdca, 
        cbad, cbda, cabd, cadb, cdba, cdab, 
        dbca, dbac, dcba, dcab, dabc, dacb}
         == {permutesABCD, permutesBACD, permutesCBAD, permutesDBCA}*/
);

wire sharedAll;
wire[15:0] oneThreeVarOverlaps;
wire[17:0] twoTwoVarOverlaps;

permuteProduceIntermediaries24 intermediaryProducer(top, bot, sharedAll, oneThreeVarOverlaps, twoTwoVarOverlaps);
permutCheckProduceResults24 resultsProducer(sharedAll & isBotValid, oneThreeVarOverlaps, twoTwoVarOverlaps, validBotPermutations);

endmodule

// 3 clock cycles latency
module permuteCheck24Pipelined (
    input clk,
    input[127:0] top,
    input[127:0] bot,
    input isBotValid,
    output[23:0] validBotPermutations
);

wire sharedAllD;
wire[15:0] oneThreeVarOverlapsD;
wire[17:0] twoTwoVarOverlapsD;
permuteProduceIntermediaries24Pipelined intermediaryProducer(clk, top, bot, isBotValid, sharedAllD, oneThreeVarOverlapsD, twoTwoVarOverlapsD);

permutCheckProduceResults24Pipelined resultsProducer(clk, sharedAllD, oneThreeVarOverlapsD, twoTwoVarOverlapsD, validBotPermutations);

endmodule

// 3 clock cycles latency
module permuteCheck720Pipelined (
    input clk,
    input[127:0] top,
    input[127:0] bot,
    input isBotValid,
    output[30*24-1:0] validBotPermutations // 30 sets of 24 valid bot permutations
);

`include "inlineVarSwap_header.v"

generate

wire[127:0] botPermutations[29:0];
assign botPermutations[0] = bot;
`VAR_SWAP_INLINE_WITHIN_GENERATE(1, 2, botPermutations[0], botPermutations[5])
`VAR_SWAP_INLINE_WITHIN_GENERATE(1, 3, botPermutations[0], botPermutations[10])
`VAR_SWAP_INLINE_WITHIN_GENERATE(1, 4, botPermutations[0], botPermutations[15])
`VAR_SWAP_INLINE_WITHIN_GENERATE(1, 5, botPermutations[0], botPermutations[20])
`VAR_SWAP_INLINE_WITHIN_GENERATE(1, 6, botPermutations[0], botPermutations[25])

for(genvar i = 0; i < 6; i = i + 1) begin
    `VAR_SWAP_INLINE_WITHIN_GENERATE(2, 3, botPermutations[5*i], botPermutations[5*i+1])
    `VAR_SWAP_INLINE_WITHIN_GENERATE(2, 4, botPermutations[5*i], botPermutations[5*i+2])
    `VAR_SWAP_INLINE_WITHIN_GENERATE(2, 5, botPermutations[5*i], botPermutations[5*i+3])
    `VAR_SWAP_INLINE_WITHIN_GENERATE(2, 6, botPermutations[5*i], botPermutations[5*i+4])
end

for(genvar permutationI = 0; permutationI < 30; permutationI = permutationI + 1) begin
    
    
    wire[7:0] topParts[15:0];
    wire[7:0] botParts[15:0];
    
    for(genvar i = 0; i < 16; i = i + 1) begin
        assign topParts[i] = top[i*8+:8];
        assign botParts[i] = botPermutations[permutationI][i*8+:8];
    end
    
    reg sharedAll; always @(posedge clk) sharedAll <= (&({topParts[0], topParts[15]} | ~{botParts[0],botParts[15]})) && isBotValid;
    
    wire[7:0] oneVarTops[3:0]; // indexed a, b, c, d
    assign oneVarTops[0] = topParts[4'b0001];assign oneVarTops[1] = topParts[4'b0010];
    assign oneVarTops[2] = topParts[4'b0100];assign oneVarTops[3] = topParts[4'b1000];
    wire[7:0] oneVarBots[3:0];
    assign oneVarBots[0] = botParts[4'b0001];assign oneVarBots[1] = botParts[4'b0010];
    assign oneVarBots[2] = botParts[4'b0100];assign oneVarBots[3] = botParts[4'b1000];
    
    wire[7:0] twoVarTops[5:0]; // indexed ab, ac, ad, cd, bd, bc
    assign twoVarTops[0] = topParts[4'b0011];assign twoVarTops[1] = topParts[4'b0101];assign twoVarTops[2] = topParts[4'b1001];
    assign twoVarTops[3] = topParts[4'b1100];assign twoVarTops[4] = topParts[4'b1010];assign twoVarTops[5] = topParts[4'b0110];
    wire[7:0] twoVarBots[5:0];
    assign twoVarBots[0] = botParts[4'b0011];assign twoVarBots[1] = botParts[4'b0101];assign twoVarBots[2] = botParts[4'b1001];
    assign twoVarBots[3] = botParts[4'b1100];assign twoVarBots[4] = botParts[4'b1010];assign twoVarBots[5] = botParts[4'b0110];
    
    wire[7:0] threeVarTops[3:0]; // indexed bcd, acd, abd, abc
    assign threeVarTops[0] = topParts[4'b1110];assign threeVarTops[1] = topParts[4'b1101];
    assign threeVarTops[2] = topParts[4'b1011];assign threeVarTops[3] = topParts[4'b0111];
    wire[7:0] threeVarBots[3:0];
    assign threeVarBots[0] = botParts[4'b1110];assign threeVarBots[1] = botParts[4'b1101];
    assign threeVarBots[2] = botParts[4'b1011];assign threeVarBots[3] = botParts[4'b0111];
    
    
    //wire[15:0] oneThreeVarOverlaps; // 4x4 array: maps flavor to location X___, _X__, __X_, ___X, in flavors a,b,c,d
    //wire[17:0] twoTwoVarOverlaps; // 3x6 array: for the shapes XX__, X_X_ and X__X, in flavors ab, ac, ad, cd, bd, bc
    /*
        ab__, ac__, ad__, cd__, bd__, bc__
        a_b_, a_c_, a_d_, c_d_, b_d_, b_c_
        a__b, a__c, a__d, c__d, b__d, b__c
    */
    
    reg[15:0] oneThreeVarOverlaps;
    // maps var j to location i
    for(genvar i = 0; i < 4; i=i+1) begin
        for(genvar j = 0; j < 4; j=j+1) begin
            always @(posedge clk) oneThreeVarOverlaps[i * 4 + j] <= &((oneVarTops[i] | ~oneVarBots[j]) & (threeVarTops[i] | ~threeVarBots[j]));
        end
    end
    reg[17:0] twoTwoVarOverlaps;
    for(genvar i = 0; i < 3; i=i+1) begin
        for(genvar j = 0; j < 6; j=j+1) begin
            always @(posedge clk) twoTwoVarOverlaps[i * 6 + j] <= &((twoVarTops[i] | ~twoVarBots[j]) & (twoVarTops[i+3] | ~twoVarBots[(j+3) % 6])); // joins ab witb cd, ad with bc etc. 
        end
    end
    
    permutCheckProduceResults24Pipelined resultsProducer(clk, sharedAll, oneThreeVarOverlaps, twoTwoVarOverlaps, validBotPermutations[24*permutationI +: 24]);
end

endgenerate

endmodule
