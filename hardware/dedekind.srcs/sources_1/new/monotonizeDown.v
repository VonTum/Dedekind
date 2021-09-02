`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 06/27/2021 11:10:49 PM
// Design Name: 
// Module Name: monotonizeDown
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



module monotonizeDown(
    input[127:0] vIn,
    output[127:0] vOut
);

wire[127:0] removeA;
wire[127:0] removeB;
wire[127:0] removeC;
wire[127:0] removeD;
wire[127:0] removeE;
wire[127:0] removeF;

genvar i;
generate
    // remove A
    for(i = 0; i < 128; i = i + 1) begin
        assign removeA[i] = (i % 2 < 1) ? vIn[i] | vIn[i+1] : vIn[i];
    end
    // remove B
    for(i = 0; i < 128; i = i + 1) begin
        assign removeB[i] = (i % 4 < 2) ? removeA[i] | removeA[i+2] : removeA[i];
    end
    // remove C
    for(i = 0; i < 128; i = i + 1) begin
        assign removeC[i] = (i % 8 < 4) ? removeB[i] | removeB[i+4] : removeB[i];
    end
    // remove D
    for(i = 0; i < 128; i = i + 1) begin
        assign removeD[i] = (i % 16 < 8) ? removeC[i] | removeC[i+8] : removeC[i];
    end
    // remove E
    for(i = 0; i < 128; i = i + 1) begin
        assign removeE[i] = (i % 32 < 16) ? removeD[i] | removeD[i+16] : removeD[i];
    end
    // remove F
    for(i = 0; i < 128; i = i + 1) begin
        assign removeF[i] = (i % 64 < 32) ? removeE[i] | removeE[i+32] : removeE[i];
    end
    // remove G
    for(i = 0; i < 128; i = i + 1) begin
        assign vOut[i] = (i < 64) ? removeF[i] | removeF[i+64] : removeF[i];
    end
endgenerate

endmodule

