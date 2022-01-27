`timescale 1ns / 1ps

`include "leafElimination_header.v"

`define PIPE_STEPS (1+1+3+2)
`define STARTING_CONNECT_COUNT_LAG 5

module preprocessingModule #(parameter EXTRA_DATA_WIDTH = 8) (
    input clk,
    input[127:0] top,
    input[127:0] botIn,
    
    input start,
    input[EXTRA_DATA_WIDTH-1:0] extraDataIn,
    
    output[127:0] graphOut, // Has a latency of `PIPE_STEPS
    output[5:0] startingConnectCount_DELAYED, // Has a latency of `PIPE_STEPS + `STARTING_CONNECT_COUNT_LAG
    
    output startPostDelay,
    output[EXTRA_DATA_WIDTH-1:0] extraDataPostDelay
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
singletonElimination se(clk, leafEliminatedGraph, graphOut, startingConnectCount_DELAYED);


hyperpipe #(.CYCLES(`PIPE_STEPS), .WIDTH(1)) startPipe(clk,
    start, startPostDelay
);

shiftRegister #(.CYCLES(`PIPE_STEPS), .WIDTH(EXTRA_DATA_WIDTH)) extraDataPipe (clk, 
    extraDataIn, 
    extraDataPostDelay
);

endmodule

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

wire[127:0] topLocal; // Manual top distribution tree. I can't use SDC constraints so this will have to do
hyperpipe #(.CYCLES(2), .WIDTH(128)) topPipe(clk, top, topLocal);

wire[127:0] preprocessedGraph;
wire[5:0] startingConnectCount_DELAYED;
wire startPostDelay;
wire[EXTRA_DATA_WIDTH-1:0] extraDataPostDelay;
preprocessingModule #(EXTRA_DATA_WIDTH) preprocessor (
    clk,
    topLocal,
    botIn,
    
    start,
    extraDataIn,
    
    preprocessedGraph,
    startingConnectCount_DELAYED,
    
    startPostDelay,
    extraDataPostDelay
);

pipelinedCountConnectedCore #(.EXTRA_DATA_WIDTH(EXTRA_DATA_WIDTH), .DATA_IN_LATENCY(`PIPE_STEPS + REQUEST_LATENCY), .STARTING_CONNECT_COUNT_LAG(`STARTING_CONNECT_COUNT_LAG)) core(
    .clk(clk), 
    .rst(rst),
    
    // input side
    .request(requestGraph), 
    .graphIn(preprocessedGraph), 
    .start(startPostDelay), 
    .startingConnectCountIn_DELAYED(startingConnectCount_DELAYED), 
    .extraDataIn(extraDataPostDelay),
    
    // output side
    .done(done),
    .connectCount(resultCount),
    .extraDataOut(extraDataOut)
);
endmodule
