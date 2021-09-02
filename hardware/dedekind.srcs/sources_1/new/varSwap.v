`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 07/23/2021 01:13:23 PM
// Design Name: 
// Module Name: varSwap
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


module varSwap #(
    parameter FIRST_VAR = 5,
    parameter SECOND_VAR = 6
) (
    input [127:0] dataIn,
    output [127:0] dataOut
);

function automatic hasVar;
    input integer index;
    input integer var;
    
    begin
        hasVar = (index & (1 << var)) != 0;
    end
endfunction

function automatic integer getInIndex;
    input integer outIndex;
    begin
        if(hasVar(outIndex, FIRST_VAR) & !hasVar(outIndex, SECOND_VAR))
            getInIndex = outIndex - (1 << FIRST_VAR) + (1 << SECOND_VAR);
        else if(!hasVar(outIndex, FIRST_VAR) & hasVar(outIndex, SECOND_VAR)) 
            getInIndex = outIndex + (1 << FIRST_VAR) - (1 << SECOND_VAR);
        else
            getInIndex = outIndex;
    end
endfunction

generate
    for(genvar outI = 0; outI < 128; outI = outI + 1) begin
        //$display("outI:%d, inI:%d", outI, getInIndex(outI));
        //$display("test");
        assign dataOut[outI] = dataIn[getInIndex(outI)];
    end
endgenerate

endmodule
