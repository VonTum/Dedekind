`timescale 1ns / 1ps

`include "pipelineGlobals_header.v"

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

wire fifoEmpty;
assign dataOutValid = !fifoEmpty;

`ifdef USE_FIFO_IP
outputBufferFifo buffer (
    .clock(clk),
    .sclr(rst),
    .wrreq(dataInValid),
    .data(dataIn),
	 
    .rdreq(dataOutReady & !fifoEmpty),
    .q(dataOut),
    .empty(fifoEmpty),
	 
    .usedw(outputFifoFullness)
);
`else
FIFO #(.WIDTH(64), .DEPTH_LOG2(`FIFO_DEPTH_BITS)) buffer (
    .clk(clk),
    .rst(rst),
	
    // input side
    .writeEnable(dataInValid),
    .dataIn(dataIn),
    .full(__fullUnconnected),
    
    // output side
    .readEnable(dataOutReady & !fifoEmpty),
    .dataOut(dataOut),
    .empty(fifoEmpty),
	 
    .usedw(outputFifoFullness)
);
`endif

endmodule
