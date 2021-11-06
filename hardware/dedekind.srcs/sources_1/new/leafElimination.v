`timescale 1ns / 1ps
`include "leafElimination.vh"
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 07/24/2021 05:22:49 PM
// Design Name: 
// Module Name: leafElimination
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


module leafEliminationElement #(parameter COUNT = 3) (
    input mainWire,
    input [COUNT - 1:0] oneWires,
    input [6 - COUNT:0] orWires,
    output nonLeafOut
);

wire hasExactlyOneWire;
exactlyOne #(COUNT) hasExactlyOne(oneWires, hasExactlyOneWire);
wire isLeaf = hasExactlyOneWire & !(|orWires);
assign nonLeafOut = mainWire & !isLeaf;

endmodule


module leafElimination #(parameter DIRECTION = `DOWN) (
    input [127:0] graphIn,
    output [127:0] graphOut
);

function integer popcntStatic;
    input integer val;
    input integer upTo;
    
    integer sum;
    integer i;
    begin
        sum = 0;
        for(i = 0; i < upTo; i = i + 1)
            if(val & (1 << i))
                sum = sum + 1;
        popcntStatic = sum;
    end
endfunction

function integer getNthBit;
    input integer val;
    input integer n;
    input bit;
    
    integer correctBitsPassed;
    integer i;
    begin
        correctBitsPassed = 0;
        for(i = 0; i < 32; i = i + 1) begin
            if(val[i] == bit) begin
                if(correctBitsPassed == n) begin
                    getNthBit = i;
                end
                correctBitsPassed = correctBitsPassed + 1;
            end
        end
    end

endfunction

generate
    assign graphOut[0] = graphIn[0];
    assign graphOut[127] = graphIn[127];
    for(genvar outI = 1; outI < 127; outI = outI + 1) begin
        wire[popcntStatic(outI, 7)-1:0] wiresBelow;
        wire[6-popcntStatic(outI, 7):0] wiresAbove;
        
        for(genvar wireI = 0; wireI < popcntStatic(outI, 7); wireI = wireI + 1) begin
            assign wiresBelow[wireI] = graphIn[outI & ~(1 << getNthBit(outI, wireI, 1))];
        end
        
        for(genvar wireI = 0; wireI < 7-popcntStatic(outI, 7); wireI = wireI + 1) begin
            assign wiresAbove[wireI] = graphIn[outI | (1 << getNthBit(outI, wireI, 0))];
        end
        
        if(DIRECTION == `UP) begin
            leafEliminationElement #(popcntStatic(outI, 7)) elem(
                .mainWire(graphIn[outI]), 
                .oneWires(wiresBelow), 
                .orWires(wiresAbove), 
                .nonLeafOut(graphOut[outI])
            );
        end else begin
            leafEliminationElement #(7-popcntStatic(outI, 7)) elem(
                .mainWire(graphIn[outI]), 
                .oneWires(wiresAbove), 
                .orWires(wiresBelow), 
                .nonLeafOut(graphOut[outI])
            );
        end
    end
endgenerate

endmodule
