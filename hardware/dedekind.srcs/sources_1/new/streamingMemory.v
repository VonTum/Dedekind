`timescale 1ns / 1ps
`include "bramProperties.vh"

//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 08/17/2021 02:11:09 PM
// Design Name: 
// Module Name: streamingMemory
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

module streamingMemory_512x40b #(parameter EXTRA_DATA_WIDTH = 0) (
    input clk,
    
    // read section
    input[`ADDR_WIDTH-1:0] readAddr,
    output[`DATA_WIDTH-1:0] dout,
    output[`ADDR_WIDTH-1:0] readAddrOut,
    input[EXTRA_DATA_WIDTH-1:0] extraDataIn,
    output[EXTRA_DATA_WIDTH-1:0] extraDataOut,
    
    // write section
    input[`ADDR_WIDTH-1:0] writeAddr,
    input[`DATA_WIDTH-1:0] din,
    input writeEnable
);

reg[`ADDR_WIDTH-1:0] addrQueue[`READ_LATENCY-1:0];
reg[EXTRA_DATA_WIDTH-1:0] extraDataQueue[`READ_LATENCY-1:0];

generate
    for(genvar i = 0; i < `READ_LATENCY-1; i=i+1) begin
        always @(posedge clk) begin
            addrQueue[i] <= addrQueue[i+1];
            extraDataQueue[i] <= extraDataQueue[i+1];
        end
    end
endgenerate

always @(posedge clk) begin
    addrQueue[`READ_LATENCY-1] = readAddr;
    extraDataQueue[`READ_LATENCY-1] = extraDataIn;
end
assign readAddrOut = addrQueue[0];
assign extraDataOut = extraDataQueue[0];


block_mem_512x40b memory(
    .clka(clk), .clkb(clk),
    
    .addra(writeAddr), .addrb(readAddr),
    .dina(din), .doutb(dout),
    .wea(writeEnable)
);


endmodule
