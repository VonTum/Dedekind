`timescale 1ns / 1ps

`include "leafElimination_header.v"

module computeModule #(parameter EXTRA_DATA_WIDTH = 14) (
    input clk,
    input rst,
    input[127:0] top,
    
    // input side
    input[127:0] botIn,
    input graphAvailable,
    input[EXTRA_DATA_WIDTH-1:0] extraDataIn,
    output requestGraph,
    
    // output side
    output done,
    output[5:0] resultCount,
    output[EXTRA_DATA_WIDTH-1:0] extraDataOut
);

reg[127:0] graphIn; always @(posedge clk) if(requestGraph) graphIn <= top & ~botIn;

// PIPELINE STEP 1
wire[127:0] leafEliminatedGraph;

leafElimination #(.DIRECTION(`DOWN)) le(clk, requestGraph, graphIn, leafEliminatedGraph);

wire[127:0] singletonEliminatedGraph;
wire[5:0] connectCountInDelayed;
wire graphAvailablePostDelay;
wire[EXTRA_DATA_WIDTH-1:0] extraDataPostDelay;
singletonElimination se(clk, requestGraph, leafEliminatedGraph, singletonEliminatedGraph, connectCountInDelayed);

localparam PIPE_STEPS = 1+1+2;

shiftRegister #(.CYCLES(PIPE_STEPS), .WIDTH(1), .RESET_VALUE(0))
    graphAvalailbePipe (clk, requestGraph, rst, graphAvailable, graphAvailablePostDelay);
shiftRegister #(.CYCLES(PIPE_STEPS), .WIDTH(EXTRA_DATA_WIDTH), .RESET_VALUE(1'bX))
    extraDataPipe      (clk, requestGraph, rst, extraDataIn, extraDataPostDelay);

wire coreRequestsGraph;
reg coreRequestsGraphD;
always @(posedge clk) coreRequestsGraphD <= coreRequestsGraph;
assign requestGraph = coreRequestsGraphD;

pipelinedCountConnectedCore #(.EXTRA_DATA_WIDTH(EXTRA_DATA_WIDTH), .DATA_IN_LATENCY(1), .CONNECT_COUNT_IN_LAG(5)) core(
    .clk(clk), 
    .rst(rst),
    .top(top),
    
    // input side
    .request(coreRequestsGraph), 
    .graphIn(singletonEliminatedGraph), 
    .start(graphAvailablePostDelay & coreRequestsGraphD), 
    .connectCountInDelayed(connectCountInDelayed), 
    .extraDataIn(extraDataPostDelay),
    
    // output side
    .done(done),
    .connectCount(resultCount),
    .extraDataOut(extraDataOut)
);
endmodule
