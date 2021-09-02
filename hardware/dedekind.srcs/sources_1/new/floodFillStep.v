`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 06/28/2021 12:36:18 AM
// Design Name: 
// Module Name: floodFillStep
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


// graph 132250a8000000000000000000000000, explored 000000a8000000000000000000000000 returns 00220000000000000000000000000000?


module floodFillStep(
    input[127:0] graphIn,
    input[127:0] exploredIn,
    output[127:0] exploredOut,
    output noChange
);

wire[127:0] monotonizedUp;
monotonizeUp firstStep(exploredIn, monotonizedUp);

wire[127:0] midPoint;
assign midPoint = monotonizedUp & graphIn;

wire[127:0] monotonizedDown;
monotonizeDown secondStep(midPoint, monotonizedDown);

assign exploredOut = monotonizedDown & graphIn;
assign noChange = (midPoint == exploredOut);

endmodule
