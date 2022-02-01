`timescale 1ns / 1ps

module aggregatingPipeline (
    input clk,
    input clk2x,
    input rst,
    input[127:0] top,
    
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


// 1 PIPE STEP
reg[127:0] graph; always @(posedge clk) graph <= top & ~bot;

// 2 PIPE STEP
wire[127:0] leafEliminatedGraph;
leafElimination #(.DIRECTION(`DOWN)) le(clk, graph, leafEliminatedGraph);

wire isBotValidD; hyperpipe #(.CYCLES(4)) isBotValidPipe(clk, isBotValid, isBotValidD);
wire lastBotOfBatchD; hyperpipe #(.CYCLES(4)) lastBotOfBatchPipe(clk, lastBotOfBatch, lastBotOfBatchD);



streamingCountConnectedCore #(.EXTRA_DATA_WIDTH(1)) core (
    .clk(clk),
    .clk2x(clk2x),
    .rst(rst),
    
    // Input side
    .isBotValid(isBotValidD),
    .graphIn(leafEliminatedGraph),
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
