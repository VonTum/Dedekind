`timescale 1ns / 1ps
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



module leafEliminationDown (
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

wire[127:0] isLeaf;

assign graphOut = graphIn & ~isLeaf; // removes nodes with exactly one neighbor

generate
    assign isLeaf[0] = 0;
    for(genvar outI = 1; outI < 128; outI = outI + 1) begin
        wire[popcntStatic(outI, 7)-1:0] inputWires;
        
        for(genvar wireI = 0; wireI < popcntStatic(outI, 7); wireI = wireI + 1) begin
            assign inputWires[wireI] = graphIn[outI & ~(1 << getNthBit(outI, wireI, 1))];
        end
        
        exactlyOne #(popcntStatic(outI, 7)) hasExactlyOne(inputWires, isLeaf[outI]);
    end
endgenerate



endmodule

/*module leafEliminationUp (
    input [127:0] graphIn,
    output [127:0] graphOut
);

wire[127:0] isLeaf;

assign graphOut = graphIn & ~isLeaf; // removes nodes with exactly one neighbor

generate
    for(genvar outI = 0; outI < 128; outI = outI + 1) begin
        wire[6-popcntStatic(outI, 7):0] inputWires;
        
        for(genvar wireI = 0; wireI < 7-popcntStatic(outI, 7); wireI = wireI + 1) begin
            assign inputWires[wireI] = graphIn[outI | (1 << getNthBit(outI, wireI, 0))];
        end
        
        exactlyOne #(7-popcntStatic(outI, 7)) hasExactlyOne(inputWires, isLeaf[outI]);
    end
endgenerate

endmodule*/
