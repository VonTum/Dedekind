`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 06/28/2021 12:26:01 AM
// Design Name: 
// Module Name: monotonizeUp
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


module monotonizeUp(
    input [127:0] vIn,
    output [127:0] vOut
);

wire[127:0] addA;
wire[127:0] addB;
wire[127:0] addC;
wire[127:0] addD;
wire[127:0] addE;
wire[127:0] addF;

genvar i;
generate
    // add A
    for(i = 0; i < 128; i = i + 1) begin
        assign addA[i] = (i % 2 >= 1) ? vIn[i] | vIn[i-1] : vIn[i];
    end
    // add B
    for(i = 0; i < 128; i = i + 1) begin
        assign addB[i] = (i % 4 >= 2) ? addA[i] | addA[i-2] : addA[i];
    end
    // add C
    for(i = 0; i < 128; i = i + 1) begin
        assign addC[i] = (i % 8 >= 4) ? addB[i] | addB[i-4] : addB[i];
    end
    // add D
    for(i = 0; i < 128; i = i + 1) begin
        assign addD[i] = (i % 16 >= 8) ? addC[i] | addC[i-8] : addC[i];
    end
    // add E
    for(i = 0; i < 128; i = i + 1) begin
        assign addE[i] = (i % 32 >= 16) ? addD[i] | addD[i-16] : addD[i];
    end
    // add F
    for(i = 0; i < 128; i = i + 1) begin
        assign addF[i] = (i % 64 >= 32) ? addE[i] | addE[i-32] : addE[i];
    end
    // add G
    for(i = 0; i < 128; i = i + 1) begin
        assign vOut[i] = (i >= 64) ? addF[i] | addF[i-64] : addF[i];
    end
endgenerate


endmodule
