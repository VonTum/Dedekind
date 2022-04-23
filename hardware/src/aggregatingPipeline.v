`timescale 1ns / 1ps

`include "leafElimination_header.v"

module aggregatingPipeline #(parameter PCOEFF_COUNT_BITWIDTH = 0) (
    input clk,
    input clk2x,
    input rst,
    input longRST,
    input[127:0] sharedTop,
    input[1:0] topChannel,
    output isActive2x, // Instrumentation wire for profiling
    
    input isBotValid,
    input[127:0] bot,
    input lastBotOfBatch,
    output slowDownInput,
    
    output resultsValid,
    output reg[PCOEFF_COUNT_BITWIDTH+35-1:0] pcoeffSum,
    output reg[PCOEFF_COUNT_BITWIDTH-1:0] pcoeffCount,
    
    output eccStatus
);


wire eccFromPipeline;
wire connectCountFromPipelineValid;
wire[5:0] connectCountFromPipeline;

reg[127:0] graphD; always @(posedge clk) graphD <= sharedTop & ~bot;

reg isBotValidD; always @(posedge clk) isBotValidD <= isBotValid;
reg lastBotOfBatchD; always @(posedge clk) lastBotOfBatchD <= lastBotOfBatch;

wire[127:0] leafEliminatedGraphD;
leafElimination #(.DIRECTION(`DOWN)) le(graphD, leafEliminatedGraphD);

reg[127:0] leafEliminatedGraphDD; always @(posedge clk) leafEliminatedGraphDD <= leafEliminatedGraphD;
reg isBotValidDD; always @(posedge clk) isBotValidDD <= isBotValidD;
reg lastBotOfBatchDD; always @(posedge clk) lastBotOfBatchDD <= lastBotOfBatchD;

streamingCountConnectedCore #(.EXTRA_DATA_WIDTH(1)) core (
    .clk(clk),
    .clk2x(clk2x),
    .rst(rst),
    .topChannel(topChannel),
    .isActive2x(isActive2x),
    
    // Input side
    .isBotValid(isBotValidDD),
    .graphIn(leafEliminatedGraphDD),
    .extraDataIn(lastBotOfBatchDD),
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
    if(longRST || resultsValid) begin
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
