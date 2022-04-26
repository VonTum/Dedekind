`timescale 1ns / 1ps

module OpenCLFullPermutationPipeline(
    input clock,
    input clock2x, // apparently this specific name gives access to a 2x speed clock. Very useful!
    input resetn,
    input ivalid, 
    input iready,
    output ovalid,
    output oready,
    
    input[127:0] mbfUppers,
    input[127:0] mbfLowers,
    input startNewTop, // we reuse bot to set the top, to save on inputs. 
    output[127:0] results   // first 16 bits pcoeffCountOut, last 48 bits summedDataOut
);

wire[127:0] mbfA = {mbfUppers[127:64], mbfLowers[127:64]};
wire[127:0] mbfB = {mbfUppers[63:0], mbfLowers[63:0]};
wire botInValid = ivalid && !startNewTop;
wire topInValid = ivalid && startNewTop;

// Reset validation
localparam RESET_CYCLES = 35;
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



// Long reset generation
reg[8:0] longRSTReg;
wire longRSTWire = longRSTReg != 9'b111111111;
always @(posedge clock) begin
    if(rst) begin
        longRSTReg <= 0;
    end else begin
        longRSTReg <= longRSTReg + longRSTWire;
    end
end
reg longRST; always @(posedge clock) longRST <= longRSTWire;



wire outputFIFOAlmostFull;
reg outputFIFOAlmostFullD; always @(posedge clock) outputFIFOAlmostFullD <= outputFIFOAlmostFull;
wire dataFromPipelineValid;
wire[63:0] summedDataPcoeffCountOutA;
wire[63:0] summedDataPcoeffCountOutB;
MultiFullPermutationPipeline multiFullPermPipeline (
    .clk(clock),
    .clk2x(clock2x),
    .rst(rst),
    .longRST(longRST),
    
    .portStatusesMonitor({ivalid, iready, ovalid, oready}),
    
    // Input side
    .writeBotIn(botInValid && oready),
    .writeTopIn(topInValid && oready),
    .mbfA(mbfA),
    .mbfB(mbfB),
    .readyForInput(pipelineIsReadyForInput),
    
    // Output side
    .slowDown(outputFIFOAlmostFullD),
    .resultValid(dataFromPipelineValid),
    .summedDataPcoeffCountOutA(summedDataPcoeffCountOutA),   // first 16 bits pcoeffCountOut, last 48 bits summedDataOut
    .summedDataPcoeffCountOutB(summedDataPcoeffCountOutB)   // first 16 bits pcoeffCountOut, last 48 bits summedDataOut
);

(* dont_merge *) reg fastResponseTimeFIFORST; always @(posedge clock) fastResponseTimeFIFORST <= rst;
wire outputFIFOEmpty;
assign ovalid = !outputFIFOEmpty;
FIFO_MLAB #(.WIDTH(128), .ALMOST_FULL_MARGIN(16)) fastResponseTimeFIFO (
    .clk(clock),
    .rst(fastResponseTimeFIFORST),
    
    // input side
    .writeEnable(dataFromPipelineValid),
    .dataIn({summedDataPcoeffCountOutA, summedDataPcoeffCountOutB}),
    .almostFull(outputFIFOAlmostFull),
    
    // output side
    .readEnable(iready && !outputFIFOEmpty),
    .dataOut(results),
    .empty(outputFIFOEmpty)
);

endmodule

module MultiFullPermutationPipeline (
    input clk,
    input clk2x,
    input rst,
    input longRST,
    
    input[3:0] portStatusesMonitor,
    
    // Input side
    input writeBotIn,
    input writeTopIn,
    input[127:0] mbfA,
    input[127:0] mbfB,
    output readyForInput,
    
    // Output side
    input slowDown,
    output resultValid,
    output[63:0] summedDataPcoeffCountOutA,   // first 16 bits pcoeffCountOut, last 48 bits summedDataOut
    output[63:0] summedDataPcoeffCountOutB   // first 16 bits pcoeffCountOut, last 48 bits summedDataOut
);

`define TOTAL_FPP_COUNT 2

wire eccErrorOccured;

(* dont_merge *) reg outputFIFOLongRST; always @(posedge clk) outputFIFOLongRST <= longRST;

wire[1:0] topChannel;
wire stallInput;
wire resultOriginQueueEmpty;
topManager topMngr (
    .clk(clk),
    .rst(rst),
    
    .topIn(mbfA),
    .topInValid(writeTopIn),
    
    .stallInput(stallInput),
    .pipelineIsEmpty(resultOriginQueueEmpty),
    
    .topChannel(topChannel)
);

// Pipeline inputs
wire[127:0] botToPipelines[`TOTAL_FPP_COUNT-1:0];
wire[`TOTAL_FPP_COUNT-1:0] writeToPipeline;

wire[`TOTAL_FPP_COUNT-1:0] pipelineReadies;

// Pipeline outputs
wire[`TOTAL_FPP_COUNT-1:0] outputFIFOEmpties;
wire[`TOTAL_FPP_COUNT-1:0] pipelineDataRequests;
wire requestedPipelinesHaveData = &(~pipelineDataRequests | ~outputFIFOEmpties);

wire grabData = ((resultOriginQueueEmpty ? 0 : requestedPipelinesHaveData) || eccErrorOccured) && !slowDown;

wire[`TOTAL_FPP_COUNT-1:0] pipelineReadRequests = grabData ? pipelineDataRequests : 0;
wire[60:0] dataFromPipelines[`TOTAL_FPP_COUNT-1:0];

// Debug
wire[`TOTAL_FPP_COUNT * 30 - 1:0] pipelineActivities;
wire originQueueECC;
reg[`TOTAL_FPP_COUNT-1:0] pipelineECCs;
reg[`TOTAL_FPP_COUNT-1:0] outputFIFOECCs;
eccMonitor #(.NUMBER_OF_PIPELINES(`TOTAL_FPP_COUNT)) eccMon (
    clk, rst, 
    originQueueECC, pipelineECCs, outputFIFOECCs,
    eccErrorOccured
);


// OriginQueue
wire readDebugDataPre;
wire resultOriginQueueAlmostFull;
FIFO_M20K #(.WIDTH(`TOTAL_FPP_COUNT + 1), .DEPTH_LOG2(13/*8192*/), .ALMOST_FULL_MARGIN(128)) resultOriginQueue (
    .clk(clk),
    .rst(rst),
    
    // input side
    .writeEnable(writeTopIn || writeBotIn),
    .dataIn({writeToPipeline, writeTopIn}),
    .almostFull(resultOriginQueueAlmostFull),
    
    // output side
    .readEnable(grabData),
    .dataOut({pipelineDataRequests, readDebugDataPre}),
    .empty(resultOriginQueueEmpty),
    .eccStatus(originQueueECC)
);

wire readDebugData;
hyperpipe #(.CYCLES(4/*M20K fifo latency*/)) readDebugDataPipe(clk, resultOriginQueueEmpty ? 0 : readDebugDataPre, readDebugData);
hyperpipe #(.CYCLES(4/*M20K fifo latency*/), .WIDTH(1)) grabbingDataPipe(clk, grabData, resultValid);

generate
for(genvar pipelineI = 0; pipelineI < `TOTAL_FPP_COUNT; pipelineI = pipelineI + 1) begin
    
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
    always @(posedge clk) pipelineECCs[pipelineI] <= pipelineECCDD;
    
    wire[60:0] dataFromPipeline;
    wire dataFromPipelineValid;
    wire outputFifoAlmostFull;
    // Extra registers to allow long distance between pipeline and fifo
    reg[60:0] dataFromPipelineD; always @(posedge clk) dataFromPipelineD <= dataFromPipeline;
    reg dataFromPipelineValidD; always @(posedge clk) dataFromPipelineValidD <= dataFromPipelineValid;
    reg outputFifoAlmostFullD; always @(posedge clk) outputFifoAlmostFullD <= outputFifoAlmostFull;
    fullPermutationPipeline30 permutationPipeline (
        .clk(clk),
        .clk2x(clk2x),
        .rst(rstDDD),
        .longRST(longRSTDDD),
        .activities2x(pipelineActivities[pipelineI*30 +: 30]),
        
        .topChannel(topChannelDDD),
        .bot(botToPipelines[pipelineI]),
        .writeBot(writeToPipeline[pipelineI]),
        .readyForInputBot(pipelineReadies[pipelineI]),
        
        .slowDown(outputFifoAlmostFullD),
        .resultValid(dataFromPipelineValid),
        .pcoeffSum(dataFromPipeline[47:0]),
        .pcoeffCount(dataFromPipeline[60:48]),
        .eccStatus(pipelineECC)
    );
    
    wire fifoECC;
    // Has enough buffer space to empty all pipelines into it. If not backpressured it shouldn't ever really reach anywhere near the limit
    FastFIFO_SAFE_M20K #(.WIDTH(48+13), .DEPTH_LOG2(9), .ALMOST_FULL_MARGIN(16)) outputFIFO (
        .clk(clk),
        .rst(outputFIFOLongRST),
        
        // input side
        .writeEnable(dataFromPipelineValidD),
        .dataIn(dataFromPipelineD),
        .almostFull(outputFifoAlmostFull),
        
        // Read Side
        .readRequest(pipelineReadRequests[pipelineI]),
        .dataOut(dataFromPipelines[pipelineI]), // Holds the last valid data
        .empty(outputFIFOEmpties[pipelineI]),
        .dataOutValid(),
        .eccStatus(fifoECC)
    );
    always @(posedge clk) outputFIFOECCs[pipelineI] <= fifoECC;
end
endgenerate



wire[62:0] debugDataA;
wire[63:0] debugDataB;

debugMonitor #(.NUMBER_OF_ACTIVITIES(30*`TOTAL_FPP_COUNT)) debugMon(
    clk, clk2x, readDebugData, 
    
    pipelineActivities,
    
    portStatusesMonitor,
    
    debugDataA, debugDataB
);


// Connections for dual-stream mbfA, mbfB
assign writeToPipeline = writeBotIn? 2'b11 : 2'b00;
assign botToPipelines[0] = mbfA;
assign botToPipelines[1] = mbfB;

wire mbfAReady = pipelineReadies[0];
wire mbfBReady = pipelineReadies[1];
assign readyForInput = mbfAReady && mbfBReady && !stallInput && !resultOriginQueueAlmostFull;


wire[60:0] selectedDataA = dataFromPipelines[0];
wire[60:0] selectedDataB = dataFromPipelines[1];

assign summedDataPcoeffCountOutA = {eccErrorOccured, (readDebugData ? debugDataA : {2'b00, selectedDataA})};
assign summedDataPcoeffCountOutB = readDebugData ? debugDataB : {3'b000, selectedDataB};

endmodule



module eccMonitor #(parameter NUMBER_OF_PIPELINES = 2) (
    input clk,
    input rst,
    
    input originQueueECC,
    input[NUMBER_OF_PIPELINES-1:0] pipelineECCs,
    input[NUMBER_OF_PIPELINES-1:0] outputFIFOECCs,
    
    output reg eccErrorOccured
);

// ECC Detection
wire eccStatus = originQueueECC || |pipelineECCs || |outputFIFOECCs;
always @(posedge clk) begin
    if(rst) begin
        eccErrorOccured <= 0;
    end else begin
        if(eccStatus) eccErrorOccured <= 1;
    end
end

endmodule

module debugMonitor #(parameter NUMBER_OF_ACTIVITIES = 30) (
    input clk,
    input clk2x,
    input rst,
    
    input[NUMBER_OF_ACTIVITIES-1:0] activities2x,
    
    input[3:0] flagsToTrack,
    
    output[62:0] outputDataA,
    output[63:0] outputDataB
);

// Activity measures
`define ACTIVITY_MEASURE_LIMIT 60

// Performance profiling with a measure of how many pipelines are working at any given time
reg[6:0] activityMeasure; // Instrumentation wire for profiling (0-`ACTIVITY_MEASURE_LIMIT activity level)

// Extra slack registers
reg[NUMBER_OF_ACTIVITIES-1:0] activities2xD; always @(posedge clk2x) activities2xD <= activities2x;
reg[NUMBER_OF_ACTIVITIES-1:0] activities2xDD; always @(posedge clk2x) activities2xDD <= activities2xD;

wire[5:0] actPipelineSums[NUMBER_OF_ACTIVITIES / 30 - 1 : 0];

genvar i;
generate
reg[3:0] actSums2x[NUMBER_OF_ACTIVITIES / 15 -1 : 0];
for(i = 0; i < NUMBER_OF_ACTIVITIES / 15; i = i + 1) begin
    wire[14:0] acts2x = activities2xDD[15*i +: 15];
    reg[2:0] sumA; reg[2:0] sumB; reg[2:0] sumC;
    always @(posedge clk2x) begin
        sumA <= acts2x[0] + acts2x[1] + acts2x[2] + acts2x[3] + acts2x[4];
        sumB <= acts2x[5] + acts2x[6] + acts2x[7] + acts2x[8] + acts2x[9];
        sumC <= acts2x[10] + acts2x[11] + acts2x[12] + acts2x[13] + acts2x[14];
        actSums2x[i] <= sumA + sumB + sumC;
    end
end
for(i = 0; i < NUMBER_OF_ACTIVITIES / 30; i = i + 1) begin
    reg[4:0] pipelineSum2x;
    reg[4:0] pipelineSum2xD;
    reg[5:0] pipelineSum2xDSum;
    reg[5:0] pipelineSumSync;
    always @(posedge clk2x) begin
        pipelineSum2x <= actSums2x[i*2] + actSums2x[i*2+1];
        pipelineSum2xD <= pipelineSum2x;
        pipelineSum2xDSum <= pipelineSum2x + pipelineSum2xD;
    end
    always @(posedge clk) pipelineSumSync <= pipelineSum2xDSum;
    hyperpipe #(.CYCLES(3), .WIDTH(6)) pipelineSumPipe(clk, pipelineSumSync, actPipelineSums[i]);
end

endgenerate

always @(posedge clk) activityMeasure <= actPipelineSums[0] + actPipelineSums[1];

reg[45:0] activityCounter;
reg[40:0] clockCounter;
assign outputDataA[62:32] = activityCounter[45:15];
assign outputDataA[31:0] = clockCounter[40:9];
// The resulting occupancy is calculated as activityCounter / `ACTIVITY_MEASURE_LIMIT / clockCounter
// Also, the occupancy returned for a top is the occupancy of the PREVIOUS top
// For an outside observer, occupancy is computed as (dataOut[60:31] << 6) / `ACTIVITY_MEASURE_LIMIT / dataOut[30:0]

always @(posedge clk) begin
    if(rst) begin
        clockCounter <= 0;
        activityCounter <= 0;
    end else begin
        clockCounter <= clockCounter + 1;
        activityCounter <= activityCounter + activityMeasure;
    end
end

reg[31:0] trackerCycles[3:0];

// Extra slack
reg[3:0] flagsToTrackD; always @(posedge clk) flagsToTrackD <= flagsToTrack;

always @(posedge clk) begin
    if(rst) begin
        trackerCycles[0] <= 0;
        trackerCycles[1] <= 0;
        trackerCycles[2] <= 0;
        trackerCycles[3] <= 0;
    end else begin
        trackerCycles[0] <= trackerCycles[0] + flagsToTrackD[0];
        trackerCycles[1] <= trackerCycles[1] + flagsToTrackD[1];
        trackerCycles[2] <= trackerCycles[2] + flagsToTrackD[2];
        trackerCycles[3] <= trackerCycles[3] + flagsToTrackD[3];
    end
end

assign outputDataB = {trackerCycles[3][31:16], trackerCycles[2][31:16], trackerCycles[1][31:16], trackerCycles[0][31:16]};

endmodule

