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
    parameter DEPTH_LOG2 = 5,
    parameter ALMOST_FULL_MARGIN = 2,
    parameter ALMOST_EMPTY_MARGIN = 2) (
    
    input wClk,
    input writeEnable,
    input[WIDTH-1:0] dataIn,
    output full,
    output almostFull,
    
    input rClk,
    input readEnable,
    output[WIDTH-1:0] dataOut,
    output empty,
    output almostEmpty
);

reg[WIDTH-1:0] data [(1 << DEPTH_LOG2) - 1:0]; // one extra element to differentiate between empty fifo and full

reg[DEPTH_LOG2-1:0] writeHead;
reg[DEPTH_LOG2-1:0] readHead;

assign empty = writeHead == readHead;
assign full = readHead - writeHead == 1; // uses unsigned overflow
assign almostFull = readHead - writeHead - 1 <= ALMOST_FULL_MARGIN; // uses unsigned overflow
assign almostEmpty = writeHead - readHead <= ALMOST_EMPTY_MARGIN; // uses unsigned overflow
assign dataOut = data[readHead];

initial begin
    writeHead = 0;
    readHead = 0;
end

always @ (posedge wClk) begin
    if(writeEnable) begin
        data[writeHead] = dataIn;
        writeHead = writeHead + 1; // uses unsigned overflow
    end
end
always @ (posedge rClk) begin
    if(readEnable) begin
        readHead = readHead + 1; // uses unsigned overflow
    end
end

endmodule
