`timescale 1ns / 1ps

`include "leafElimination_header.v"

module computeModule #(parameter EXTRA_DATA_WIDTH = 14, parameter REQUEST_LATENCY = 3) (
    input clk,
    input rst,
    input[127:0] top,
    
    // input side
    input[127:0] botIn,
    input start,
    input[EXTRA_DATA_WIDTH-1:0] extraDataIn,
    output requestGraph,
    
    // output side
    output done,
    output[5:0] resultCount,
    output[EXTRA_DATA_WIDTH-1:0] extraDataOut
);

// 1 PIPE STEP
reg[127:0] botInD;
always @(posedge clk) botInD <= botIn;

// 1 PIPE STEP
reg[127:0] graphIn; always @(posedge clk) graphIn <= top & ~botInD;

// 2 PIPE STEP
wire[127:0] leafEliminatedGraph;
leafElimination #(.DIRECTION(`DOWN)) le(clk, graphIn, leafEliminatedGraph);

// 2 PIPE STEPS
wire[127:0] singletonEliminatedGraph;
wire[5:0] startingConnectCountIn_DELAYED;
wire startPostDelay;
wire[EXTRA_DATA_WIDTH-1:0] extraDataPostDelay;
singletonElimination se(clk, leafEliminatedGraph, singletonEliminatedGraph, startingConnectCountIn_DELAYED);

localparam PIPE_STEPS = 1+1+2+2;

shiftRegister #(.CYCLES(PIPE_STEPS), .WIDTH(1+EXTRA_DATA_WIDTH)) graphAvalailbePipe (clk, 
    {start, extraDataIn}, 
    {startPostDelay, extraDataPostDelay}
);


pipelinedCountConnectedCore #(.EXTRA_DATA_WIDTH(EXTRA_DATA_WIDTH), .DATA_IN_LATENCY(PIPE_STEPS + REQUEST_LATENCY), .STARTING_CONNECT_COUNT_LAG(5)) core(
    .clk(clk), 
    .rst(rst),
    
    // input side
    .request(requestGraph), 
    .graphIn(singletonEliminatedGraph), 
    .start(startPostDelay), 
    .startingConnectCountIn_DELAYED(startingConnectCountIn_DELAYED), 
    .extraDataIn(extraDataPostDelay),
    
    // output side
    .done(done),
    .connectCount(resultCount),
    .extraDataOut(extraDataOut)
);
endmodule
