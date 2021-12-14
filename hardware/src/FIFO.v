`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 08/03/2021 05:49:03 PM
// Design Name: 
// Module Name: FIFO
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

module FIFO #(
    parameter WIDTH = 16,
    parameter DEPTH_LOG2 = 5) (
    
    input clk,
	 
    // input side
    input writeEnable,
    input[WIDTH-1:0] dataIn,
    output full,
    
    // output side
    input readEnable,
    output[WIDTH-1:0] dataOut,
    output empty,
	 
	 output[DEPTH_LOG2-1:0] usedw
);

reg[WIDTH-1:0] data [(1 << DEPTH_LOG2) - 1:0]; // one extra element to differentiate between empty fifo and full

reg[DEPTH_LOG2-1:0] writeHead = 0;
reg[DEPTH_LOG2-1:0] readHead = 0;

assign usedw = writeHead - readHead;

assign empty = usedw == 0;
assign full = usedw == -1; // ((1 << DEPTH_LOG2) - 1); // uses unsigned overflow
assign dataOut = data[readHead];

always @ (posedge clk) begin
    if(writeEnable) begin
        data[writeHead] <= dataIn;
        writeHead <= writeHead + 1; // uses unsigned overflow
    end
    if(readEnable) begin
        readHead <= readHead + 1; // uses unsigned overflow
    end
end

endmodule
