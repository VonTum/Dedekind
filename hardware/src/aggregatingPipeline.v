`timescale 1ns / 1ps

module aggregatingPipeline #(parameter PCOEFF_COUNT_BITWIDTH = 10) (
    input clk,
    input rst,
    input[127:0] top,
    
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


streamingCountConnectedCore #(.EXTRA_DATA_WIDTH(1)) core (
    .clk(clk),
    .rst(rst),
    .top(top),
    
    // Input side
    .isBotValid(isBotValid),
    .bot(bot),
    .extraDataIn(lastBotOfBatch),
    .slowDownInput(slowDownInput),
    
    // Output side
    .resultValid(connectCountFromPipelineValid),
    .connectCount(connectCountFromPipeline),
    .extraDataOut(resultsValid),
    .eccStatus(eccFromPipeline)
);

assign eccStatus = eccFromPipeline || (connectCountFromPipeline > 35); // Connect Count should *never* be > 35
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
