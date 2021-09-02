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


module singletonElimination(
    input[127:0] graphIn,
    output[127:0] graphOut,
    output[5:0] singletonCount
);

wire[127:0] hasNeighboring;
wire[127:0] isSingleton = graphIn &~hasNeighboring;
assign graphOut = graphIn & hasNeighboring;
wire[7:0] popCountSingleton;
popcnt singletonCounter(isSingleton, popCountSingleton);
assign singletonCount = popCountSingleton[5:0]; // explicitly cut off top bits. #singletons <= 35

generate
    for(genvar outI = 0; outI < 128; outI = outI + 1) begin
        wire[6:0] inputWires;
        for(genvar var = 0; var < 7; var = var + 1) begin
            if((outI & (1 << var)) != 0)
                assign inputWires[var] = graphIn[outI - (1 << var)];
            else
                assign inputWires[var] = graphIn[outI + (1 << var)];
        end
        assign hasNeighboring[outI] = |inputWires;
    end
endgenerate

endmodule
