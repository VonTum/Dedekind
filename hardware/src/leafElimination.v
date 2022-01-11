`timescale 1ns / 1ps
`include "leafElimination_header.v"

module leafEliminationElement #(parameter COUNT = 3) (
    input mainWire,
    input [COUNT - 1:0] oneWires,
    input [6 - COUNT:0] orWires,
    output nonLeafOut
);

/*wire hasExactlyOneWireD;
wire hasNoOrWiresD;
reg mainWireD; always @(posedge clk) mainWireD <= mainWire;

generate
    if(COUNT >= 4) begin
        
    end else begin
        
    end
endgenerate*/

wire hasExactlyOneWire;
exactlyOne #(COUNT) hasExactlyOne(oneWires, hasExactlyOneWire);
assign hasNoOrWires = !(|orWires);

assign nonLeafOut = mainWire & !(hasExactlyOneWire & hasNoOrWires);

endmodule


module leafElimination #(parameter DIRECTION = `DOWN) (
    input clk,
    input [127:0] graphIn,
    output reg [127:0] graphOut
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
    input isBitOn;
    
    integer correctBitsPassed;
    integer i;
    begin
        correctBitsPassed = 0;
        for(i = 0; i < 32; i = i + 1) begin
            if(val[i] == isBitOn) begin
                if(correctBitsPassed == n) begin
                    getNthBit = i;
                end
                correctBitsPassed = correctBitsPassed + 1;
            end
        end
    end

endfunction

reg[127:0] graphInD; always @(posedge clk) graphInD <= graphIn;

wire[127:0] graphPreOut;
assign graphPreOut[0] = graphInD[0];
assign graphPreOut[127] = graphInD[127];
reg[127:0] graphPreOutD; always @(posedge clk) graphPreOutD <= graphPreOut;
always @(posedge clk) graphOut <= graphPreOutD;
generate
    for(genvar outI = 1; outI < 127; outI = outI + 1) begin
        wire[popcntStatic(outI, 7)-1:0] wiresBelow;
        wire[6-popcntStatic(outI, 7):0] wiresAbove;
        
        for(genvar wireI = 0; wireI < popcntStatic(outI, 7); wireI = wireI + 1) begin
            assign wiresBelow[wireI] = graphInD[outI & ~(1 << getNthBit(outI, wireI, 1))];
        end
        
        for(genvar wireI = 0; wireI < 7-popcntStatic(outI, 7); wireI = wireI + 1) begin
            assign wiresAbove[wireI] = graphInD[outI | (1 << getNthBit(outI, wireI, 0))];
        end
        
        if(DIRECTION == `UP) begin
            leafEliminationElement #(popcntStatic(outI, 7)) elem(
                .mainWire(graphInD[outI]), 
                .oneWires(wiresBelow), 
                .orWires(wiresAbove), 
                .nonLeafOut(graphPreOut[outI])
            );
        end else begin
            leafEliminationElement #(7-popcntStatic(outI, 7)) elem(
                .mainWire(graphInD[outI]), 
                .oneWires(wiresAbove), 
                .orWires(wiresBelow), 
                .nonLeafOut(graphPreOut[outI])
            );
        end
    end
endgenerate

endmodule
