`timescale 1ns / 1ps

module OpenCLFullPermutationPipeline #(parameter TOTAL_FPP_COUNT = 11, parameter ENABLE_DEBUG_DATA = 0) (
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
    output[127:0] results   // first 3 bits ecc status, 13 bits pcoeffCountOut, last 48 bits summedDataOut
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

wire processorAlmostFull;
wire topManagerStall;
assign oready = !processorAlmostFull && !topManagerStall && !rst; 



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



// Top manager
wire outputFIFOAlmostFull;
reg outputFIFOAlmostFullD; always @(posedge clock) outputFIFOAlmostFullD <= outputFIFOAlmostFull;
wire[1:0] topChannel;
wire pipelineEmpty;
topManager topMngr (
    .clk(clock),
    .rst(rst),
    
    .topIn(mbfA),
    .topInValid(topInValid && oready),
    
    .stallInput(topManagerStall),
    .pipelineIsEmpty(pipelineEmpty && !outputFIFOAlmostFullD),
    
    .topChannel(topChannel)
);

reg prevTopManagerStall; always @(posedge clock) prevTopManagerStall <= topManagerStall;
wire newTopInstalled = !topManagerStall && prevTopManagerStall;


wire[TOTAL_FPP_COUNT * 30 - 1:0] activities2x;

wire eccStatus;
wire dataFromPipelineValid;
wire[60:0] summedDataPcoeffCountOutA;
wire[60:0] summedDataPcoeffCountOutB;
MultiFullPermutationPipeline #(.TOTAL_FPP_COUNT(TOTAL_FPP_COUNT)) multiFullPermPipeline (
    .clk(clock),
    .clk2x(clock2x),
    .rst(rst),
    .longRST(longRST),
    .topChannel(topChannel),
    .activities2x(activities2x),
    
    // Input side
    .writeBotIn(botInValid && oready),
    .botA(mbfA),
    .botB(mbfB),
    .almostFull(processorAlmostFull),
    .empty(pipelineEmpty),
    
    // Output side
    .slowDown(outputFIFOAlmostFullD),
    .resultValid(dataFromPipelineValid),
    .summedDataPcoeffCountOutA(summedDataPcoeffCountOutA),   // first 16 bits pcoeffCountOut, last 48 bits summedDataOut
    .summedDataPcoeffCountOutB(summedDataPcoeffCountOutB),   // first 16 bits pcoeffCountOut, last 48 bits summedDataOut
    
    .eccStatus(eccStatus)
);

wire eccErrorOccured;
cosmicRayDetection errorDetector (
    .clk(clock),
    .rst(rst),
    .eccStatus(eccStatus),
    
    .eccErrorOccured(eccErrorOccured)
);

// Debug monitor
wire[62:0] dataToOutA;
wire[63:0] dataToOutB;
generate
if(ENABLE_DEBUG_DATA) begin
    wire[62:0] debugDataA;
    wire[63:0] debugDataB;
    debugMonitor #(.NUMBER_OF_ACTIVITIES(30*TOTAL_FPP_COUNT)) debugMon(
        .clk(clock),
        .clk2x(clock2x),
        .rst(newTopInstalled),
        
        .activities2x(activities2x),
        .flagToTrack0(ivalid),
        .flagToTrack1(iready),
        .flagToTrack2(ovalid),
        .flagToTrack3(oready),
        
        .outputDataA(debugDataA),
        .outputDataB(debugDataB)
    );
    
    assign dataToOutA = newTopInstalled ? debugDataA : {2'b00, summedDataPcoeffCountOutA};
    assign dataToOutB = newTopInstalled ? debugDataB : {3'b000, summedDataPcoeffCountOutB};
end else begin
    assign dataToOutA = {2'b00, summedDataPcoeffCountOutA};
    assign dataToOutB = {3'b000, summedDataPcoeffCountOutB};
end
endgenerate

(* dont_merge *) reg fastResponseTimeFIFORST; always @(posedge clock) fastResponseTimeFIFORST <= rst;

wire[126:0] resultsData;
assign results = {eccErrorOccured, resultsData};
wire outputFIFOEmpty;
FIFO_MLAB #(.WIDTH(127), .ALMOST_FULL_MARGIN(16)) fastResponseTimeFIFO (
    .clk(clock),
    .rst(fastResponseTimeFIFORST),
    
    // input side
    .writeEnable(dataFromPipelineValid || newTopInstalled), // These should never occur at the same time, so should be safe
    .dataIn({dataToOutA, dataToOutB}),
    .almostFull(outputFIFOAlmostFull),
    
    // output side
    .readEnable(iready && !outputFIFOEmpty),
    .dataOut(resultsData),
    .empty(outputFIFOEmpty)
);
assign ovalid = !outputFIFOEmpty || eccErrorOccured;

endmodule

module MultiFullPermutationPipeline #(parameter TOTAL_FPP_COUNT = 5) (
    input clk,
    input clk2x,
    input rst,
    input longRST,
    input[1:0] topChannel,
    
    output[TOTAL_FPP_COUNT * 30 - 1:0] activities2x,
    
    // Input side
    input writeBotIn,
    input[127:0] botA,
    input[127:0] botB,
    output almostFull,
    output empty,
    
    // Output side
    input slowDown,
    output resultValid,
    output reg[60:0] summedDataPcoeffCountOutA,   // first 13 bits pcoeffCountOut, last 48 bits summedDataOut
    output reg[60:0] summedDataPcoeffCountOutB,   // first 13 bits pcoeffCountOut, last 48 bits summedDataOut
    
    output reg eccStatus
);

localparam FPP_PER_CHANNEL = TOTAL_FPP_COUNT / 2;

(* dont_merge *) reg outputFIFOLongRST; always @(posedge clk) outputFIFOLongRST <= longRST;

// Pipeline inputs
wire sharedSource;
wire[127:0] botToPipelines[TOTAL_FPP_COUNT-1:0];
wire[TOTAL_FPP_COUNT-1:0] writeToPipeline;

wire[TOTAL_FPP_COUNT-1:0] pipelineAlmostFulls;
wire[TOTAL_FPP_COUNT-1:0] pipelineAlmostEmpties;

// Pipeline outputs
wire[TOTAL_FPP_COUNT-1:0] outputFIFOEmpties;
wire[TOTAL_FPP_COUNT-1:0] pipelineDataRequests;
wire requestedPipelinesHaveData = &(~pipelineDataRequests | ~outputFIFOEmpties);

wire originIndexAvailable;
wire grabData = (originIndexAvailable ? requestedPipelinesHaveData : 0) && !slowDown;

wire[TOTAL_FPP_COUNT-1:0] pipelineReadRequests = grabData ? pipelineDataRequests : 0;
wire[60:0] dataFromPipelines[TOTAL_FPP_COUNT-1:0];

// ECC
wire originQueueECC;
wor pipelineECC;
wor outputFIFOECC;
always @(posedge clk) eccStatus <= originQueueECC || pipelineECC || outputFIFOECC;

localparam GRAB_LATENCY = 4; //M20K fifo latency
hyperpipe #(.CYCLES(GRAB_LATENCY+1/*+output reg*/), .WIDTH(1)) grabbingDataPipe(clk, grabData, resultValid);

generate
wor[60:0] outWOR_A;
wor[60:0] outWOR_B;
for(genvar i = 0; i <= FPP_PER_CHANNEL-1; i=i+1) begin
    assign botToPipelines[i] = botA;
    assign botToPipelines[FPP_PER_CHANNEL+i] = botB;
    
    assign outWOR_A = dataFromPipelines[i];
    assign outWOR_B = dataFromPipelines[FPP_PER_CHANNEL+i];
end
if(TOTAL_FPP_COUNT % 2 == 1) begin
    assign botToPipelines[TOTAL_FPP_COUNT-1] = sharedSource ? botB : botA;
    
    wire[60:0] sharedPipelineOutput = dataFromPipelines[TOTAL_FPP_COUNT-1];

    wire reqNotSharedA = |pipelineDataRequests[FPP_PER_CHANNEL-1:0];
    wire reqNotSharedB = |pipelineDataRequests[2*FPP_PER_CHANNEL-1 : FPP_PER_CHANNEL];
    
    wire doesNotUseSharedA;
    wire doesNotUseSharedB;
    hyperpipe #(.CYCLES(GRAB_LATENCY), .WIDTH(1)) reqNotSharedAPipe(clk, reqNotSharedA, doesNotUseSharedA);
    hyperpipe #(.CYCLES(GRAB_LATENCY), .WIDTH(1)) reqNotSharedBPipe(clk, reqNotSharedB, doesNotUseSharedB);
    
    always @(posedge clk) summedDataPcoeffCountOutA <= doesNotUseSharedA ? outWOR_A : sharedPipelineOutput;
    always @(posedge clk) summedDataPcoeffCountOutB <= doesNotUseSharedB ? outWOR_B : sharedPipelineOutput;
end else begin
    always @(posedge clk) summedDataPcoeffCountOutA <= outWOR_A;
    always @(posedge clk) summedDataPcoeffCountOutB <= outWOR_B;
end
endgenerate


wire[TOTAL_FPP_COUNT-1:0] selectedWriteToPipelines;
assign writeToPipeline = writeBotIn ? selectedWriteToPipelines : 0;
resourceDividerAB #(.NUMBER_OF_PIPELINES(TOTAL_FPP_COUNT)) selector (
    .clk(clk),
    .rst(rst),
    
    // Input side
    .accept(writeBotIn),
    .almostFull(almostFull),
    
    // Output side
    .almostFulls(pipelineAlmostFulls),
    .almostEmpties(pipelineAlmostEmpties),
    .selectedOut(selectedWriteToPipelines),
    .sharedSource(sharedSource)
);

// Expects sufficient readRequests while resetting, so that the output pipe is flushed properly
LowLatencyFastFIFO_M20K #(.WIDTH(TOTAL_FPP_COUNT), .DEPTH_LOG2(15/*32000*/)) resultOriginQueue (
    .clk(clk),
    .rst(rst),
    
    // input side
    .writeEnable(writeBotIn),
    .dataIn(selectedWriteToPipelines),
    .almostFull(), // Not connected, FIFO larger than it could possibly fill
    
    // Read Side
    .readEnable(grabData || !originIndexAvailable),
    .dataOut(pipelineDataRequests),
    .dataOutAvailable(originIndexAvailable),
    
    .eccStatus(originQueueECC)
);


generate
for(genvar pipelineI = 0; pipelineI < TOTAL_FPP_COUNT; pipelineI = pipelineI + 1) begin
    fullPermutationPipeline30WithOutFIFOAndSpacingRegisters permutationPipeline (
        .clk(clk),
        .clk2x(clk2x),
        .rst(rst),
        .longRST(longRST),
        .activities2x(activities2x[pipelineI*30 +: 30]),
        
        .topChannel(topChannel),
        .botIn(botToPipelines[pipelineI]),
        .writeBotIn(writeToPipeline[pipelineI]),
        .almostFull(pipelineAlmostFulls[pipelineI]),
        .almostEmpty(pipelineAlmostEmpties[pipelineI]),
        
        .readRequest(pipelineReadRequests[pipelineI]),
        .empty(outputFIFOEmpties[pipelineI]),
        .dataOut(dataFromPipelines[pipelineI]), // Holds the last valid data
        
        .eccStatusPipeline(pipelineECC),
        .eccStatusFIFO(outputFIFOECC)
    );
end
endgenerate

// Smoothing to make sure empty flag is conservative
wire nonEmptyIndicator = originIndexAvailable || writeBotIn;
reg[3:0] cyclesEmpty;
assign empty = cyclesEmpty == 15;
always @(posedge clk) begin
    if(nonEmptyIndicator || rst) cyclesEmpty <= 0;
    else cyclesEmpty <= cyclesEmpty + !empty;
end

endmodule

module resourceDivider #(parameter WIDTH = 5) (
    input clk,
    input rst,
    
    input accept,
    input[WIDTH-1:0] almostFulls,
    
    output reg[WIDTH-1:0] selectedOneHot
);

wire[WIDTH-1:0] nextOneHot;

always @(posedge clk) begin
    if(rst) begin
        selectedOneHot <= 1; // select first one
    end else begin
        if(accept) begin
            selectedOneHot <= nextOneHot;
        end
    end
end

wire[WIDTH-1:0] shifteds[WIDTH-1:0];
wire[WIDTH-1:0] selectedShifted[WIDTH-1:0];

generate
assign shifteds[0] = selectedOneHot;
assign selectedShifted[0] = selectedOneHot; // Stay in place as fallback
for(genvar i = 1; i < WIDTH; i = i + 1) begin
    assign shifteds[i] = {shifteds[i-1][0], shifteds[i-1][WIDTH-1:1]};
    wire isValidShifted = |(shifteds[i] & ~almostFulls);
    assign selectedShifted[i] = isValidShifted ? shifteds[i] : selectedShifted[i-1];
end
endgenerate

assign nextOneHot = selectedShifted[WIDTH-1];

endmodule

module resourceDividerAB #(parameter NUMBER_OF_PIPELINES = 2) (
    input clk,
    input rst,
    
    // Input side
    input accept,
    output almostFull,
    
    // Output side
    input[NUMBER_OF_PIPELINES-1:0] almostFulls,
    input[NUMBER_OF_PIPELINES-1:0] almostEmpties,
    output[NUMBER_OF_PIPELINES-1:0] selectedOut,
    output sharedSource
);

localparam UNIQUE_CH_WIDTH = NUMBER_OF_PIPELINES / 2;
localparam CH_WIDTH = (NUMBER_OF_PIPELINES+1) / 2;

wire[CH_WIDTH-1:0] selectedA;
wire[CH_WIDTH-1:0] selectedB;

wire[CH_WIDTH-1:0] almostFullsA;
wire[CH_WIDTH-1:0] almostFullsB;

wire[UNIQUE_CH_WIDTH-1:0] uniqueAlmostFullsA = almostFulls[UNIQUE_CH_WIDTH-1:0];
wire[UNIQUE_CH_WIDTH-1:0] uniqueAlmostFullsB = almostFulls[2*UNIQUE_CH_WIDTH-1:UNIQUE_CH_WIDTH];

assign almostFullsA[UNIQUE_CH_WIDTH-1:0] = uniqueAlmostFullsA;
assign almostFullsB[UNIQUE_CH_WIDTH-1:0] = uniqueAlmostFullsB;

assign selectedOut[UNIQUE_CH_WIDTH-1:0] = selectedA[UNIQUE_CH_WIDTH-1:0];
assign selectedOut[2*UNIQUE_CH_WIDTH-1:UNIQUE_CH_WIDTH] = selectedB[UNIQUE_CH_WIDTH-1:0];

if(NUMBER_OF_PIPELINES % 2 == 1) begin
    wire sharedAlmostEmpty = almostEmpties[NUMBER_OF_PIPELINES-1];
    reg sharedAlmostEmptyD; always @(posedge clk) sharedAlmostEmptyD <= sharedAlmostEmpty;
    reg sharedAlmostEmptyDD; always @(posedge clk) sharedAlmostEmptyDD <= sharedAlmostEmptyD;
    
    reg sharedSourceReg = 0;
    
    wire pipelineCanUseSharedA = sharedAlmostEmptyDD || (uniqueAlmostFullsA != 0);
    wire pipelineCanUseSharedB = sharedAlmostEmptyDD || (uniqueAlmostFullsB != 0);
    
    assign almostFullsA[CH_WIDTH-1] =  sharedSourceReg || !pipelineCanUseSharedA || almostFulls[NUMBER_OF_PIPELINES-1];
    assign almostFullsB[CH_WIDTH-1] = !sharedSourceReg || !pipelineCanUseSharedB || almostFulls[NUMBER_OF_PIPELINES-1];
    
    wire sharedSelected = selectedA[CH_WIDTH-1] || selectedB[CH_WIDTH-1]; // Exclusive, should never be both
    
    assign selectedOut[NUMBER_OF_PIPELINES-1] = sharedSelected;
    
    wire[$clog2(CH_WIDTH)-1:0] fullCountA; popcntNaive #(UNIQUE_CH_WIDTH) popcntA (uniqueAlmostFullsA, fullCountA);
    wire[$clog2(CH_WIDTH)-1:0] fullCountB; popcntNaive #(UNIQUE_CH_WIDTH) popcntB (uniqueAlmostFullsB, fullCountB);
    
    wire preferredNextSharedSource = fullCountA == fullCountB ? !sharedSourceReg : fullCountA < fullCountB; // Prefer B if it has more full pipelines
    
    reg sharedSourceInUse = 0;
    assign sharedSource = sharedSourceInUse;
    always @(posedge clk) begin
        /*Only switch if the shared pipeline isn't being used to prevent both channels from writing to shared pipeline*/
        if(accept) begin
            if(!sharedSelected) sharedSourceReg <= preferredNextSharedSource;
            sharedSourceInUse <= sharedSourceReg; // 1 Cycle delay to sync with selectedOut
        end
    end
end else begin
    assign sharedSource = 1'bZ;
end

assign almostFull = &almostFullsA || &almostFullsB;

// Selection mechanism

resourceDivider #(CH_WIDTH) divA(
    .clk(clk),
    .rst(rst),
    
    .accept(accept),
    .almostFulls(almostFullsA),
    
    .selectedOneHot(selectedA)
);

resourceDivider #(CH_WIDTH) divB(
    .clk(clk),
    .rst(rst),
    
    .accept(accept),
    .almostFulls(almostFullsB),
    
    .selectedOneHot(selectedB)
);

endmodule

module debugMonitor #(parameter NUMBER_OF_ACTIVITIES = 30) (
    input clk,
    input clk2x,
    input rst,
    
    input[NUMBER_OF_ACTIVITIES-1:0] activities2x,
    
    input flagToTrack0,
    input flagToTrack1,
    input flagToTrack2,
    input flagToTrack3,
    
    output[62:0] outputDataA,
    output[63:0] outputDataB
);

// Performance profiling with a measure of how many pipelines are working at any given time
reg[$clog2(2*NUMBER_OF_ACTIVITIES+1)-1:0] activityMeasure; // Instrumentation wire for profiling (0-2*NUMBER_OF_ACTIVITIES activity level)

// Extra slack registers
wire[NUMBER_OF_ACTIVITIES-1:0] activities2xD;
hyperpipe #(.CYCLES(5), .WIDTH(NUMBER_OF_ACTIVITIES)) activitiesPipe(clk2x, activities2x, activities2xD);

wire[5:0] actPipelineSums[NUMBER_OF_ACTIVITIES / 30 - 1 : 0];

genvar i;
generate
reg[3:0] actSums2x[NUMBER_OF_ACTIVITIES / 15 -1 : 0];
for(i = 0; i < NUMBER_OF_ACTIVITIES / 15; i = i + 1) begin
    wire[14:0] acts2x = activities2xD[15*i +: 15];
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

reg[$clog2(2*NUMBER_OF_ACTIVITIES+1)-1:0] activityMeasuresSubSums[NUMBER_OF_ACTIVITIES / 30 - 1 : 0];
generate
always @(posedge clk) activityMeasuresSubSums[0] <= actPipelineSums[0];
for(genvar i = 1; i < NUMBER_OF_ACTIVITIES / 30; i = i + 1) begin
    always @(posedge clk) activityMeasuresSubSums[i] <= activityMeasuresSubSums[i-1] + actPipelineSums[i];
end
endgenerate
always @(posedge clk) activityMeasure <= activityMeasuresSubSums[NUMBER_OF_ACTIVITIES / 30 - 1];

reg[49:0] activityCounter;
reg[40:0] clockCounter;
assign outputDataA[62:32] = activityCounter[49:19];
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
reg flagToTrack0D; always @(posedge clk) flagToTrack0D <= flagToTrack0;
reg flagToTrack1D; always @(posedge clk) flagToTrack1D <= flagToTrack1;
reg flagToTrack2D; always @(posedge clk) flagToTrack2D <= flagToTrack2;
reg flagToTrack3D; always @(posedge clk) flagToTrack3D <= flagToTrack3;

always @(posedge clk) begin
    if(rst) begin
        trackerCycles[0] <= 0;
        trackerCycles[1] <= 0;
        trackerCycles[2] <= 0;
        trackerCycles[3] <= 0;
    end else begin
        trackerCycles[0] <= trackerCycles[0] + flagToTrack0D;
        trackerCycles[1] <= trackerCycles[1] + flagToTrack1D;
        trackerCycles[2] <= trackerCycles[2] + flagToTrack2D;
        trackerCycles[3] <= trackerCycles[3] + flagToTrack3D;
    end
end

assign outputDataB = {trackerCycles[3][31:16], trackerCycles[2][31:16], trackerCycles[1][31:16], trackerCycles[0][31:16]};

endmodule


module fullPermutationPipeline30WithOutFIFOAndSpacingRegisters (
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


