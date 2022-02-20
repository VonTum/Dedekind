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

// Registers to have basically a 1-deep queue, so that the module can "show it's readyness" before 40 clock cycles after reset
reg[127:0] storedInputBotOrTop;
reg botInValid;
reg topInValid;

always @(posedge clock) begin
    if(!resetn) begin
        botInValid <= 0;
        topInValid <= 0;
    end else begin
        if(oready) begin
            storedInputBotOrTop <= {botUpper, botLower};
            botInValid <= ivalid && !startNewTop;
            topInValid <= ivalid && startNewTop;
        end
    end
end

wire rst;
wire isInitialized;
resetNormalizer rstNormalizer(clock, resetn, rst, isInitialized);

wire pipelineIsReadyForInput;
assign oready = (pipelineIsReadyForInput && isInitialized) || (!botInValid && !topInValid && !rst); 

MultiFullPermutationPipeline multiFullPermPipeline (
    .clk(clock),
    .clk2x(clock2x),
    .rst(rst),
    
    // Input side
    .writeBotIn(botInValid && oready),
    .writeTopIn(topInValid && oready),
    .botOrTop(storedInputBotOrTop),
    .readyForInput(pipelineIsReadyForInput),
    
    // Output side
    .iready(iready),
    .ovalid(ovalid),
    .summedDataPcoeffCountOut(summedDataPcoeffCountOut)   // first 16 bits pcoeffCountOut, last 48 bits summedDataOut
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
    input iready,
    output ovalid,
    output[63:0] summedDataPcoeffCountOut   // first 16 bits pcoeffCountOut, last 48 bits summedDataOut
);

wire sendingBotResult = iready && ovalid;

wire[127:0] top;
wire stallInput;
wire resultOriginQueueEmpty;
topManager topMngr (
    .clk(clk),
    .rst(rst),
    
    .topIn(botOrTop),
    .topInValid(writeTopIn),
    
    .topOut(top),
    
    .stallInput(stallInput),
    .pipelineIsEmpty(resultOriginQueueEmpty)
);

`define NUMBER_OF_PIPELINES 1

reg grabResults[`NUMBER_OF_PIPELINES+1-1:0];
wire resultsAvailable[`NUMBER_OF_PIPELINES+1-1:0];
wire[63:0] dataOut[`NUMBER_OF_PIPELINES+1-1:0];

localparam SELECTION_BITWIDTH = $clog2(`NUMBER_OF_PIPELINES+1);

wire[SELECTION_BITWIDTH-1:0] selectedPipelineIn = writeTopIn ? 0 : 1; // top gets index 0
wire[SELECTION_BITWIDTH-1:0] selectedPipeline;
wire grabbingData = ovalid && iready;
FIFO #(.WIDTH(SELECTION_BITWIDTH), .DEPTH_LOG2(13/*8192*/)) resultOriginQueue (
    .clk(clk),
    .rst(rst),
    
    // input side
    .writeEnable(writeTopIn || writeBotIn),
    .dataIn(selectedPipelineIn),
    .full(),
    .usedw(),
    
    // output side
    .readEnable(grabbingData),
    .dataOut(selectedPipeline),
    .empty(resultOriginQueueEmpty)
);

assign ovalid = resultsAvailable[selectedPipeline] && !resultOriginQueueEmpty;
assign summedDataPcoeffCountOut = dataOut[selectedPipeline];


integer i;
always @(*) begin
    for(i = 0; i < `NUMBER_OF_PIPELINES+1; i=i+1) begin
        if(i == selectedPipeline) grabResults[i] <= grabbingData;
        else grabResults[i] <= 0;
    end
end

// Performance profiling with a measure of how many pipelines are working at any given time
wire[5:0] activityMeasure; // Instrumentation wire for profiling (0-40 activity level)

assign resultsAvailable[0] = 1;
reg[47:0] activityCounter;
reg[41:0] clockCounter;
assign dataOut[0][63:32] = activityCounter[47:16];
assign dataOut[0][31:0] = clockCounter[41:10];
// The resulting occupancy is calculated as activityCounter / 40 / clockCounter
// Also, the occupancy returned for a top is the occupancy of the PREVIOUS top
// For an outside observer, occupancy is computed as (dataOut[63:32] << 6) / 40.0 / dataOut[31:0]

always @(posedge clk) begin
    if(rst || grabResults[0]) begin
        clockCounter <= 0;
        activityCounter <= 0;
    end else begin
        clockCounter <= clockCounter + 1;
        activityCounter <= activityCounter + activityMeasure;
    end
end


wire pipelineIsReady;
assign readyForInput = pipelineIsReady && !stallInput;
fullPermutationPipeline permutationPipeline (
    .clk(clk),
    .clk2x(clk2x),
    .rst(rst),
    .activityMeasure(activityMeasure),
    
    .top(top),
    .bot(botOrTop),
    .writeBot(writeBotIn),
    .readyForInputBot(pipelineIsReady),
    
    .grabResults(grabResults[1]),
    .resultsAvailable(resultsAvailable[1]),
    .pcoeffSum(dataOut[1][47:0]),
    .pcoeffCount(dataOut[1][60:48]),
    .eccStatus(dataOut[1][63])
);
// define missing bits
assign dataOut[1][62:61] = 0;

endmodule

