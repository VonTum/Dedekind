`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 07/23/2021 05:01:41 PM
// Design Name: 
// Module Name: singletonElimination
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

module hasNeighbor(
    input[127:0] graphIn,
    output[127:0] hasNeighboring
);

generate
    genvar outI;
    genvar var;
    for(outI = 0; outI < 128; outI = outI + 1) begin
        wire[6:0] inputWires;
        for(var = 0; var < 7; var = var + 1) begin
            assign inputWires[var] = graphIn[((outI & (1 << var)) != 0) ? outI - (1 << var) : outI + (1 << var)];
        end
        assign hasNeighboring[outI] = |inputWires;
    end
endgenerate

endmodule

module singletonElimination(
    input[127:0] graphIn,
    output[127:0] nonSingletons,
    output[5:0] singletonCount
); //0005 0120  5102 4020    0244 2402  5260 0000
   // cde  de    ce    e      cd    d     c   
wire[127:0] hasNeighboring;
hasNeighbor neighborChecker(graphIn, hasNeighboring);

wire[127:0] singletons = graphIn & ~hasNeighboring;
assign nonSingletons = graphIn & hasNeighboring;
wire[7:0] popCountSingleton;
popcnt singletonCounter(singletons, popCountSingleton);
assign singletonCount = popCountSingleton[5:0]; // explicitly cut off top bits. #singletons <= 35

endmodule
