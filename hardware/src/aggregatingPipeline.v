`timescale 1ns / 1ps

`include "leafElimination_header.v"
`include "pipelineGlobals_header.v"

module aggregatingPipeline (
    input clk,
    input clk2x,
    input rst,
    output[1:0] activityMeasure, // Instrumentation wire for profiling (0-2 activity level)
    input[1:0] topChannel,
    
    input isBotValid,
    input[127:0] bot,
    input lastBotOfBatch,
    output slowDownInput,
    
    output resultsValid,
    output reg[`PCOEFF_COUNT_BITWIDTH+35-1:0] pcoeffSum,
    output reg[`PCOEFF_COUNT_BITWIDTH-1:0] pcoeffCount,
    
    output eccStatus
);


wire eccFromPipeline;
wire connectCountFromPipelineValid;
wire[5:0] connectCountFromPipeline;

wire[127:0] top;
topReceiver receiver(
    clk,
    topChannel,
    top
);

wire[127:0] graph = top & ~bot;

wire[127:0] leafEliminatedGraph;
leafElimination #(.DIRECTION(`DOWN)) le(graph, leafEliminatedGraph);

reg[127:0] leafEliminatedGraphD; always @(posedge clk) leafEliminatedGraphD <= leafEliminatedGraph;
reg isBotValidD; always @(posedge clk) isBotValidD <= isBotValid;
reg lastBotOfBatchD; always @(posedge clk) lastBotOfBatchD <= lastBotOfBatch;

streamingCountConnectedCore #(.EXTRA_DATA_WIDTH(1)) core (
    .clk(clk),
    .clk2x(clk2x),
    .rst(rst),
    .activityMeasure(activityMeasure),
    
    // Input side
    .isBotValid(isBotValidD),
    .graphIn(leafEliminatedGraphD),
    .extraDataIn(lastBotOfBatchD),
    .slowDownInput(slowDownInput),
    
    // Output side
    .resultValid(connectCountFromPipelineValid),
    .connectCount(connectCountFromPipeline),
    .extraDataOut(resultsValid),
    .eccStatus(eccFromPipeline)
);

assign eccStatus = eccFromPipeline || (connectCountFromPipelineValid && (connectCountFromPipeline > 35)); // Connect Count should *never* be > 35
wire[35:0] pcoeff = 36'b000000000000000000000000000000000001 << connectCountFromPipeline;

always @(posedge clk) begin
    if(rst || resultsValid) begin
        pcoeffSum <= 0;
        pcoeffCount <= 0;
    end else begin
        if(connectCountFromPipelineValid) begin
            pcoeffSum <= pcoeffSum + pcoeff;
            pcoeffCount <= pcoeffCount + 1;
        end
    end
end

endmodule
