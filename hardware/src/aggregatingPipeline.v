`timescale 1ns / 1ps

`include "leafElimination_header.v"

module aggregatingPipeline #(parameter PCOEFF_COUNT_BITWIDTH = 0) (
    input clk,
    input clk2x,
    input rst,
    input longRST,
    input[127:0] sharedTop,
    output isActive2x, // Instrumentation wire for profiling
    
    input isBotValid,
    input[127:0] bot,
    input lastBotOfBatch,
    input freezeCore,
    output almostFull,
    
    output resultsValid,
    output reg[PCOEFF_COUNT_BITWIDTH+35-1:0] pcoeffSum,
    output reg[PCOEFF_COUNT_BITWIDTH-1:0] pcoeffCount,
    
    output eccStatus
);

wire connectCountFromPipelineValid;
wire[5:0] connectCountFromPipeline;

reg[127:0] graphD; always @(posedge clk) graphD <= sharedTop & ~bot;

reg isBotValidD; always @(posedge clk) isBotValidD <= isBotValid;
reg lastBotOfBatchD; always @(posedge clk) lastBotOfBatchD <= lastBotOfBatch;
reg freezeCoreD; always @(posedge clk) freezeCoreD <= freezeCore;

wire[127:0] leafEliminatedGraphD;
leafElimination #(.DIRECTION(`DOWN)) le(graphD, leafEliminatedGraphD);

reg[127:0] leafEliminatedGraphDD; always @(posedge clk) leafEliminatedGraphDD <= leafEliminatedGraphD;
reg isBotValidDD; always @(posedge clk) isBotValidDD <= isBotValidD;
reg lastBotOfBatchDD; always @(posedge clk) lastBotOfBatchDD <= lastBotOfBatchD;
reg freezeCoreDD; always @(posedge clk) freezeCoreDD <= freezeCoreD;

wire[127:0] nonSingletonGraphDDD;
wire[5:0] singletonCountDDD;
singletonElimination se (clk, leafEliminatedGraphDD, nonSingletonGraphDDD, singletonCountDDD);
reg isBotValidDDD; always @(posedge clk) isBotValidDDD <= isBotValidDD;
reg lastBotOfBatchDDD; always @(posedge clk) lastBotOfBatchDDD <= lastBotOfBatchDD;
reg freezeCoreDDD; always @(posedge clk) freezeCoreDDD <= freezeCoreDD;

wire[5:0] savedSingletonCount;
streamingCountConnectedCore #(.EXTRA_DATA_WIDTH(1+6)) core (
    .clk(clk),
    .clk2x(clk2x),
    .rst(rst),
    .isActive2x(isActive2x),
    
    // Input side
    .isBotValid(isBotValidDDD),
    .graphIn(nonSingletonGraphDDD),
    .extraDataIn({lastBotOfBatchDDD, singletonCountDDD}),
    .freezeCore(freezeCoreDDD),
    .almostFull(almostFull),
    
    // Output side
    .resultValid(connectCountFromPipelineValid),
    .connectCount(connectCountFromPipeline),
    .extraDataOut({resultsValid, savedSingletonCount}),
    .eccStatus(eccStatus)
);

wire[35:0] pcoeff = 36'b000000000000000000000000000000000001 << (connectCountFromPipeline + savedSingletonCount);

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
