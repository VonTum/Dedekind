`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 08/25/2021 01:29:36 AM
// Design Name: 
// Module Name: registerPipe
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


module registerPipe #(parameter WIDTH = 8, parameter DEPTH = 2)(
    input clk,
    input[WIDTH-1:0] dataIn,
    output[WIDTH-1:0] dataOut
);

generate
    if(DEPTH > 0) begin
        reg[WIDTH-1:0] pipeline[DEPTH-1:0];
        
        for(genvar i = 0; i < DEPTH-1; i=i+1) begin
            always @(posedge clk) begin
                pipeline[i] <= pipeline[i+1];
            end
        end
        always @(posedge clk) begin
            pipeline[DEPTH-1] <= dataIn;
        end
        assign dataOut = pipeline[0];
    end else assign dataOut = dataIn;
endgenerate

endmodule
