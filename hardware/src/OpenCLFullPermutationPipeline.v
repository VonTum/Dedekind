`timescale 1ns / 1ps

module OpenCLFullPermutationPipeline(
    input clock,
    input clock2x, // apparently this specific name gives access to a 2x speed clock. Very useful!
    input resetn,
    input ivalid, 
    input iready,
    output ovalid,
    output oready,
    
    // we reuse bot to set the top, to save on inputs. 
    input startNewTop,
    input[63:0] botLower,
    input[63:0] botUpper,
    output[63:0] summedDataPcoeffCountOut   // first 16 bits pcoeffCountOut, last 48 bits summedDataOut
);

wire[127:0] inputBotOrTop = {botUpper, botLower};
wire botInValid = ivalid && !startNewTop;
wire topInValid = ivalid && startNewTop;

// Reset validation
localparam RESET_CYCLES = 30;
reg rst;
reg[$clog2(RESET_CYCLES):0] cyclesSinceReset = 0;

always @(posedge clock) begin
    if(!resetn) begin
        cyclesSinceReset <= 0;
        rst <= 1'b1;
    end else begin
        if(cyclesSinceReset >= RESET_CYCLES) begin
            rst <= 1'b0;
        end else begin
            cyclesSinceReset <= cyclesSinceReset + 1;
        end
    end
end

wire pipelineIsReadyForInput;
assign oready = pipelineIsReadyForInput && !rst; 

wire outputFIFOAlmostFull;
reg outputFIFOAlmostFullD; always @(posedge clock) outputFIFOAlmostFullD <= outputFIFOAlmostFull;
wire dataFromPipelineValid;
wire[63:0] dataFromPipeline;
MultiFullPermutationPipeline multiFullPermPipeline (
    .clk(clock),
    .clk2x(clock2x),
    .rst(rst),
    
    // Input side
    .writeBotIn(botInValid && oready),
    .writeTopIn(topInValid && oready),
    .botOrTop(inputBotOrTop),
    .readyForInput(pipelineIsReadyForInput),
    
    // Output side
    .slowDown(outputFIFOAlmostFullD),
    .summedDataPcoeffCountOutValid(dataFromPipelineValid),
    .summedDataPcoeffCountOut(dataFromPipeline)   // first 16 bits pcoeffCountOut, last 48 bits summedDataOut
);

(* dont_merge *) reg fastResponseTimeFIFORST; always @(posedge clock) fastResponseTimeFIFORST <= rst;
wire outputFIFOEmpty;
assign ovalid = !outputFIFOEmpty;
FIFO_MLAB #(.WIDTH(64), .ALMOST_FULL_MARGIN(16)) fastResponseTimeFIFO (
    .clk(clock),
    .rst(fastResponseTimeFIFORST),
    
    // input side
    .writeEnable(dataFromPipelineValid),
    .dataIn(dataFromPipeline),
    .almostFull(outputFIFOAlmostFull),
    
    // output side
    .readEnable(iready && !outputFIFOEmpty),
    .dataOut(summedDataPcoeffCountOut),
    .empty(outputFIFOEmpty)
);

endmodule

module MultiFullPermutationPipeline (
    input clk,
    input clk2x,
    input rst,
    
    // Input side
    input writeBotIn,
    input writeTopIn,
    input[127:0] botOrTop,
    output readyForInput,
    
    // Output side
    input slowDown,
    output summedDataPcoeffCountOutValid,
    output[63:0] summedDataPcoeffCountOut   // first 16 bits pcoeffCountOut, last 48 bits summedDataOut
);

// Long reset generation
reg[8:0] longRSTReg;
wire longRSTWire = longRSTReg != 9'b111111111;
always @(posedge clk) begin
    if(rst) begin
        longRSTReg <= 0;
    end else begin
        longRSTReg <= longRSTReg + longRSTWire;
    end
end
reg longRST; always @(posedge clk) longRST <= longRSTWire;

(* dont_merge *) reg pipelineLongRST; always @(posedge clk) pipelineLongRST <= longRST;
(* dont_merge *) reg outputFIFOLongRST; always @(posedge clk) outputFIFOLongRST <= longRST;

wire[1:0] topChannel;
wire stallInput;
wire resultOriginQueueEmpty;
topManager topMngr (
    .clk(clk),
    .rst(rst),
    
    .topIn(botOrTop),
    .topInValid(writeTopIn),
    
    .stallInput(stallInput),
    .pipelineIsEmpty(resultOriginQueueEmpty),
    
    .topChannel(topChannel)
);

wire[1:0] topChannelD;
hyperpipe #(.CYCLES(5), .WIDTH(2), .MAX_FAN(1)) topChannelDivider(clk, topChannel, topChannelD);

`define NUMBER_OF_PIPELINES 1

reg grabResults[`NUMBER_OF_PIPELINES+1-1:0];
wire resultsAvailable[`NUMBER_OF_PIPELINES+1-1:0];
wire[60:0] dataOut[`NUMBER_OF_PIPELINES+1-1:0];
wor eccStatus;

reg eccErrorOccured;

always @(posedge clk) begin
    if(rst) begin
        eccErrorOccured <= 0;
    end else begin
        if(eccStatus) eccErrorOccured <= 1;
    end
end

localparam SELECTION_BITWIDTH = $clog2(`NUMBER_OF_PIPELINES+1);

wire[SELECTION_BITWIDTH-1:0] selectedPipelineIn = writeTopIn ? 0 : 1; // top gets index 0
wire[SELECTION_BITWIDTH-1:0] selectedPipeline;
wire grabbingData = ((resultsAvailable[selectedPipeline] && !resultOriginQueueEmpty) || eccErrorOccured) && !slowDown;

wire resultOriginQueueAlmostFull;
FIFO_M20K #(.WIDTH(SELECTION_BITWIDTH), .DEPTH_LOG2(13/*8192*/), .ALMOST_FULL_MARGIN(128)) resultOriginQueue (
    .clk(clk),
    .rst(rst),
    
    // input side
    .writeEnable(writeTopIn || writeBotIn),
    .dataIn(selectedPipelineIn),
    .almostFull(resultOriginQueueAlmostFull),
    
    // output side
    .readEnable(grabbingData),
    .dataOut(selectedPipeline),
    .empty(resultOriginQueueEmpty),
    .eccStatus(eccStatus)
);

wire[SELECTION_BITWIDTH-1:0] selectedPipeDelayed;
hyperpipe #(.CYCLES(4/*M20K fifo latency*/), .WIDTH(SELECTION_BITWIDTH)) selectedPipePipe(clk, selectedPipeline, selectedPipeDelayed);
hyperpipe #(.CYCLES(4/*M20K fifo latency*/), .WIDTH(1)) grabbingDataPipe(clk, grabbingData, summedDataPcoeffCountOutValid);

assign summedDataPcoeffCountOut = {eccErrorOccured, 2'b00, dataOut[selectedPipeDelayed]};

integer i;
always @(*) begin
    for(i = 0; i < `NUMBER_OF_PIPELINES+1; i=i+1) begin
        if(i == selectedPipeline) grabResults[i] = grabbingData;
        else grabResults[i] = 0;
    end
end

`define ACTIVITY_MEASURE_LIMIT 60

// Performance profiling with a measure of how many pipelines are working at any given time
wire[5:0] activityMeasure; // Instrumentation wire for profiling (0-`ACTIVITY_MEASURE_LIMIT activity level)

assign resultsAvailable[0] = 1;
reg[45:0] activityCounter;
reg[40:0] clockCounter;
assign dataOut[0][60:31] = activityCounter[45:16];
assign dataOut[0][30:0] = clockCounter[40:10];
// The resulting occupancy is calculated as activityCounter / `ACTIVITY_MEASURE_LIMIT / clockCounter
// Also, the occupancy returned for a top is the occupancy of the PREVIOUS top
// For an outside observer, occupancy is computed as (dataOut[60:31] << 6) / `ACTIVITY_MEASURE_LIMIT / dataOut[30:0]

always @(posedge clk) begin
    if(rst || (selectedPipeDelayed == 0)) begin
        clockCounter <= 0;
        activityCounter <= 0;
    end else begin
        clockCounter <= clockCounter + 1;
        activityCounter <= activityCounter + activityMeasure;
    end
end

wire pipelineIsReady;
assign readyForInput = pipelineIsReady && !stallInput && !resultOriginQueueAlmostFull;

wire dataFromPipelineValid;
wire outputFifoAlmostFull;
wire[60:0] dataFromPipeline;
fullPermutationPipeline30 permutationPipeline (
    .clk(clk),
    .clk2x(clk2x),
    .rst(rst),
    .longRST(pipelineLongRST),
    .activityMeasure(activityMeasure),
    
    .topChannel(topChannelD),
    .bot(botOrTop),
    .writeBot(writeBotIn),
    .readyForInputBot(pipelineIsReady),
    
    .slowDown(outputFIFOAlmostFull),
    .resultValid(dataFromPipelineValid),
    .pcoeffSum(dataFromPipeline[47:0]),
    .pcoeffCount(dataFromPipeline[60:48]),
    .eccStatus(eccStatus)
);

wire outputFIFOEmpty;
assign resultsAvailable[1] = !outputFIFOEmpty;

// Has enough buffer space to empty all pipelines into it. If not backpressured it shouldn't ever really reach anywhere near the limit
FastFIFO_SAFE_M20K #(.WIDTH(48+13), .DEPTH_LOG2(10), .ALMOST_FULL_MARGIN(700)) outputFIFO (
    .clk(clk),
    .rst(outputFIFOLongRST),
    
    // input side
    .writeEnable(dataFromPipelineValid),
    .dataIn(dataFromPipeline),
    .almostFull(outputFIFOAlmostFull),
    
    // Read Side
    .readRequest(grabResults[1]),
    .dataOut(dataOut[1]), // Holds the last valid data
    .empty(outputFIFOEmpty),
    .dataOutValid(),
    .eccStatus(eccStatus)
);

endmodule

