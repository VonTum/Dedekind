
module pcoeffProcessorUnit (
    input clk,
    input clk2x,
    input rst,
    input longRST,
    output[29:0] activities2x, // Instrumentation wires for profiling
    
    input[1:0] topChannel,
    
    // Input side
    input[127:0] botIn,
    input writeBotIn,
    output reg almostFull,
    output almostEmpty,
    
    // Output side
    input readRequest,
    output empty,
    output[60:0] dataOut,
    
    output reg eccStatusPipeline,
    output reg eccStatusFIFO
);

(* dont_merge *) reg[1:0] topChannelD; always @(posedge clk) topChannelD <= topChannel;
(* dont_merge *) reg[1:0] topChannelDD; always @(posedge clk) topChannelDD <= topChannelD;
(* dont_merge *) reg[1:0] topChannelDDD; always @(posedge clk) topChannelDDD <= topChannelDD;

(* dont_merge *) reg longRSTD; always @(posedge clk) longRSTD <= longRST;
(* dont_merge *) reg longRSTDD; always @(posedge clk) longRSTDD <= longRSTD;
(* dont_merge *) reg longRSTDDD; always @(posedge clk) longRSTDDD <= longRSTDD;

(* dont_merge *) reg rstD; always @(posedge clk) rstD <= rst;
(* dont_merge *) reg rstDD; always @(posedge clk) rstDD <= rstD;
(* dont_merge *) reg rstDDD; always @(posedge clk) rstDDD <= rstDD;

wire pipelineECC;
reg pipelineECCD; always @(posedge clk) pipelineECCD <= pipelineECC;
reg pipelineECCDD; always @(posedge clk) pipelineECCDD <= pipelineECCD;
always @(posedge clk) eccStatusPipeline <= pipelineECCDD;

wire[60:0] dataFromPipeline;
wire dataFromPipelineValid;
wire outputFifoAlmostFull;
// Extra registers to allow long distance between pipeline and fifo
reg[60:0] dataFromPipelineD; always @(posedge clk) dataFromPipelineD <= dataFromPipeline;
reg[60:0] dataFromPipelineDD; always @(posedge clk) dataFromPipelineDD <= dataFromPipelineD;
reg dataFromPipelineValidD; always @(posedge clk) dataFromPipelineValidD <= dataFromPipelineValid;
reg dataFromPipelineValidDD; always @(posedge clk) dataFromPipelineValidDD <= dataFromPipelineValidD;
reg outputFifoAlmostFullD; always @(posedge clk) outputFifoAlmostFullD <= outputFifoAlmostFull;
reg outputFifoAlmostFullDD; always @(posedge clk) outputFifoAlmostFullDD <= outputFifoAlmostFullD;

wire almostFullPre;
reg almostFullPreD; always @(posedge clk) almostFullPreD <= almostFullPre;
reg almostFullPreDD; always @(posedge clk) almostFullPreDD <= almostFullPreD;
always @(posedge clk) almostFull <= almostFullPreDD;

(* dont_merge *) reg[127:0] botInD; always @(posedge clk) botInD <= botIn;
(* dont_merge *) reg[127:0] botInDD; always @(posedge clk) botInDD <= botInD;

(* dont_merge *) reg writeBotInD; always @(posedge clk) writeBotInD <= writeBotIn;
(* dont_merge *) reg writeBotInDD; always @(posedge clk) writeBotInDD <= writeBotInD;

fullPermutationPipeline30 fullPermutationPipeline30 (
    .clk(clk),
    .clk2x(clk2x),
    .rst(rstDDD),
    .longRST(longRSTDDD),
    .activities2x(activities2x),
    
    .topChannel(topChannelDDD),
    .botIn(botInDD),
    .writeBotIn(writeBotInDD),
    .almostFull(almostFullPre),
    .almostEmpty(almostEmpty),
    
    .slowDown(outputFifoAlmostFullDD),
    .resultValid(dataFromPipelineValid),
    .pcoeffSum(dataFromPipeline[47:0]),
    .pcoeffCount(dataFromPipeline[60:48]),
    .eccStatus(pipelineECC)
);

wire fifoECC;
// Has enough buffer space to empty all pipelines into it. If not backpressured it shouldn't ever really reach anywhere near the limit
FastFIFO_SAFE_M20K #(.WIDTH(48+13), .DEPTH_LOG2(9), .ALMOST_FULL_MARGIN(16), .HOLD_LAST_READ(0)) outputFIFO (
    .clk(clk),
    .rst(longRST),
    
    // input side
    .writeEnable(dataFromPipelineValidDD),
    .dataIn(dataFromPipelineDD),
    .almostFull(outputFifoAlmostFull),
    
    // Read Side
    .readRequest(readRequest),
    .dataOut(dataOut), // Forced to 0 when not reading
    .empty(empty),
    .dataOutValid(),
    .eccStatus(fifoECC)
);

always @(posedge clk) eccStatusFIFO <= fifoECC;

endmodule

// Mock of fullPermutationPipeline for shorter elaboration time
module fullPermutationPipeline30_MOCK_MOCK_MOCK_MOCK (
    input clk,
    input clk2x,
    input rst,
    input longRST,
    output[29:0] activities2x, // Instrumentation wires for profiling
    
    input[1:0] topChannel,
    
    // Input side
    input[127:0] botIn,
    input writeBotIn,
    output reg almostFull,
    
    // Output side
    input slowDown,
    output resultValid,
    output reg[47:0] pcoeffSum,
    output reg[12:0] pcoeffCount,
    output reg eccStatus
);

endmodule
