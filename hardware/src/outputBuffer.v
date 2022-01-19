`timescale 1ns / 1ps

// This module can buffer the data 
module outputBuffer(
    input clk,
    input rst,
    
    input dataInValid,
    input[63:0] dataIn,
    
    output reg slowInputting,
    input dataOutReady,
    output dataOutValid,
    output[63:0] dataOut
);
// Space for 8192 elements
`define FIFO_DEPTH_BITS 13
wire[`FIFO_DEPTH_BITS-1:0] outputFifoFullness;

always @(posedge clk) slowInputting <= outputFifoFullness >= 1000; // want to leave >7000 margin so the pipeline can empty out entirely. 

FastFIFO #(.WIDTH(64), .DEPTH_LOG2(`FIFO_DEPTH_BITS), .IS_MLAB(0), .READ_ADDR_STAGES(1)) buffer (
    .clk(clk),
    .rst(rst),
	
    // input side
    .writeEnable(dataInValid),
    .dataIn(dataIn),
    .usedw(outputFifoFullness),
    
    // output side
    .readRequest(dataOutReady),
    .dataOut(dataOut),
    .dataOutValid(dataOutValid)
);

endmodule
