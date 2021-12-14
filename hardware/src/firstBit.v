`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 06/28/2021 01:16:20 PM
// Design Name: 
// Module Name: firstBit
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: 
// 
// Dependencies: 
// 
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
// 
//////////////////////////////////////////////////////////////////////////////////


// should synthesize to a single logic element
module bitScanForward8(
    input[7:0] bitset,
    output[2:0] firstBitIdx,
    output isZero
);

assign firstBitIdx = 
    bitset[0] ? 0 : 
    bitset[1] ? 1 : 
    bitset[2] ? 2 : 
    bitset[3] ? 3 : 
    bitset[4] ? 4 : 
    bitset[5] ? 5 : 
    bitset[6] ? 6 : 7;

assign isZero = !(|bitset);

endmodule


module bitScanForward16(
    input[15:0] bitset,
    output[3:0] firstBitIdx,
    output isZero
);

wire[2:0] lowerIdx;
wire[2:0] upperIdx;
wire lowerZero;
wire upperZero;
bitScanForward8 lowerBits(bitset[7:0],lowerIdx,lowerZero);
bitScanForward8 upperBits(bitset[15:8],upperIdx,upperZero);

assign firstBitIdx = lowerZero ? {1'b1,upperIdx} : {1'b0,lowerIdx};
assign isZero = lowerZero & upperZero;

endmodule

module bitScanForward32(
    input[31:0] bitset,
    output[4:0] firstBitIdx,
    output isZero
);

wire[3:0] lowerIdx;
wire[3:0] upperIdx;
wire lowerZero;
wire upperZero;
bitScanForward16 lowerBits(bitset[15:0],lowerIdx,lowerZero);
bitScanForward16 upperBits(bitset[31:16],upperIdx,upperZero);

assign firstBitIdx = lowerZero ? {1'b1,upperIdx} : {1'b0,lowerIdx};
assign isZero = lowerZero & upperZero;

endmodule

module bitScanForward64(
    input[63:0] bitset,
    output[5:0] firstBitIdx,
    output isZero
);

wire[4:0] lowerIdx;
wire[4:0] upperIdx;
wire lowerZero;
wire upperZero;
bitScanForward32 lowerBits(bitset[31:0],lowerIdx,lowerZero);
bitScanForward32 upperBits(bitset[63:32],upperIdx,upperZero);

assign firstBitIdx = lowerZero ? {1'b1,upperIdx} : {1'b0,lowerIdx};
assign isZero = lowerZero & upperZero;

endmodule

module bitScanForward128(
    input[127:0] bitset,
    output[6:0] firstBitIdx,
    output isZero
);

wire[5:0] lowerIdx;
wire[5:0] upperIdx;
wire lowerZero;
wire upperZero;
bitScanForward64 lowerBits(bitset[63:0],lowerIdx,lowerZero);
bitScanForward64 upperBits(bitset[127:64],upperIdx,upperZero);

assign firstBitIdx = lowerZero ? {1'b1,upperIdx} : {1'b0,lowerIdx};
assign isZero = lowerZero & upperZero;

endmodule


module firstBit(
    input[127:0] bitsetIn,
    output[127:0] selectedBit,
    output isZero
);

wire[6:0] selector;
bitScanForward128 bitScanner(bitsetIn, selector, isZero);
assign selectedBit = 1 << selector;

endmodule
