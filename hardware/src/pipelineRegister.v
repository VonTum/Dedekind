`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 08/28/2021 01:06:14 PM
// Design Name: 
// Module Name: pipelineRegister
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


module pipelineRegister #(parameter WIDTH = 8) (
    input clk,
    
    // input side
    output inputReadEnable,
    input inputDataAvailable,
    input[WIDTH-1:0] dataIn,
    
    // input side
    input readEnable,
    output reg dataAvailable,
    output reg[WIDTH-1:0] data
);

initial dataAvailable = 0;
initial data = 0;

wire wantsToGrabData = !dataAvailable | dataAvailable & readEnable;
assign inputReadEnable = wantsToGrabData & inputDataAvailable;

always @ (posedge clk) begin
    if(wantsToGrabData) begin
        dataAvailable <= inputDataAvailable;
    end
    if(inputReadEnable) begin
        data <= dataIn;
    end
end

endmodule
