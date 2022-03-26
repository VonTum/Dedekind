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

localparam SELECTION_BITWIDTH = $clog2(`NUMBER_OF_PIPELINES+1);

wire[SELECTION_BITWIDTH-1:0] selectedPipelineIn = writeTopIn ? 0 : 1; // top gets index 0
wire[SELECTION_BITWIDTH-1:0] selectedPipeline;
wire grabbingData = ovalid && iready;

FIFO_M20K #(.WIDTH(SELECTION_BITWIDTH), .DEPTH_LOG2(13/*8192*/), .ALMOST_FULL_MARGIN(128)) resultOriginQueue (
    .clk(clk),
    .rst(rst),
    
    // input side
    .writeEnable(writeTopIn || writeBotIn),
    .dataIn(selectedPipelineIn),
    .almostFull(/*TODO*/),
    
    // output side
    .readEnable(grabbingData),
    .dataOut(selectedPipeline),
    .empty(resultOriginQueueEmpty),
    .eccStatus(eccStatus)
);

reg eccErrorOccured;

always @(posedge clk) begin
    if(rst) begin
        eccErrorOccured <= 0;
    end else begin
        if(eccStatus) eccErrorOccured <= 1;
    end
end

assign ovalid = (resultsAvailable[selectedPipeline] && !resultOriginQueueEmpty) || eccErrorOccured;
assign summedDataPcoeffCountOut = {eccErrorOccured, 2'b00, dataOut[selectedPipeline]};


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
reg[45:0] activityCounter;
reg[40:0] clockCounter;
assign dataOut[0][60:31] = activityCounter[45:16];
assign dataOut[0][30:0] = clockCounter[40:10];
// The resulting occupancy is calculated as activityCounter / 40 / clockCounter
// Also, the occupancy returned for a top is the occupancy of the PREVIOUS top
// For an outside observer, occupancy is computed as (dataOut[60:31] << 6) / 40.0 / dataOut[30:0]

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
    
    .topChannel(topChannelD),
    .bot(botOrTop),
    .writeBot(writeBotIn),
    .readyForInputBot(pipelineIsReady),
    
    .grabResults(grabResults[1]),
    .resultsAvailable(resultsAvailable[1]),
    .pcoeffSum(dataOut[1][47:0]),
    .pcoeffCount(dataOut[1][60:48]),
    .eccStatus(eccStatus)
);

endmodule

