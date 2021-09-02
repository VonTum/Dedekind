`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 07/25/2021 02:51:57 AM
// Design Name: 
// Module Name: exactlyOne
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


module exactlyOne #(
    parameter INPUT_SIZE = 3
) (
    input[INPUT_SIZE-1:0] inputWires,
    output isExactlyOne
);

generate
    if(INPUT_SIZE == 0)
        assign isExactlyOne = 0;
    else if(INPUT_SIZE == 1)
        assign isExactlyOne = inputWires[0];
     else if(INPUT_SIZE >= 2) begin
        wire[INPUT_SIZE-1:1] atLeastOnePath;
        wire[INPUT_SIZE-1:1] atLeastTwoPath;
        
        assign atLeastOnePath[1] = inputWires[0] | inputWires[1];
        assign atLeastTwoPath[1] = inputWires[0] & inputWires[1];
        
        for(genvar i = 2; i < INPUT_SIZE; i = i + 1) begin
            assign atLeastOnePath[i] = atLeastOnePath[i-1] | inputWires[i];
            assign atLeastTwoPath[i] = atLeastTwoPath[i-1] | atLeastOnePath[i-1] & inputWires[i];
        end
        assign isExactlyOne = atLeastOnePath[INPUT_SIZE-1] & ~atLeastTwoPath[INPUT_SIZE-1];
    end
endgenerate

endmodule
