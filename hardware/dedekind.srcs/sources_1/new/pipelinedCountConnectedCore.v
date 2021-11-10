`timescale 1ns / 1ps

module hasFirstBitAnalysis(
    input clk,
    input[127:0] graphIn,
    output reg[1:0] hasBit64,
    // grouped in sets of 4, an element is marked '1' if its 4 bits contain the first bit of the 16 bits of the group
    output reg[4*8-1:0] hasFirstBit4,
    // grouped in sets of 4, an element is marked '1' if its 16 bits contain the first bit of the 64 bits of the group
    output reg[4*2-1:0] hasFirstBit16
);

reg[31:0] hasBit4;
wire[7:0] hasBit16;
genvar i;
generate
for(i = 0; i < 32; i = i + 1) always @(posedge clk) hasBit4[i] <= |graphIn[i*4+:4];
for(i = 0; i < 8; i = i + 1) assign hasBit16[i] = |hasBit4[i*4+:4];
for(i = 0; i < 2; i = i + 1) always @(posedge clk) hasBit64[i] <= |hasBit16[i*4+:4];
endgenerate

generate
for(i = 0; i < 8; i = i + 1) begin always @(posedge clk) begin
    hasFirstBit4[4*i+0] <= hasBit4[4*i];
    hasFirstBit4[4*i+1] <= !hasBit4[4*i] & hasBit4[4*i+1];
    hasFirstBit4[4*i+2] <= !hasBit4[4*i] & !hasBit4[4*i+1] & hasBit4[4*i+2];
    hasFirstBit4[4*i+3] <= !hasBit4[4*i] & !hasBit4[4*i+1] & !hasBit4[4*i+2] & hasBit4[4*i+3];
end end
endgenerate

generate
for(i = 0; i < 2; i = i + 1) begin always @(posedge clk) begin
    hasFirstBit16[4*i+0] <=  hasBit16[4*i];
    hasFirstBit16[4*i+1] <= !hasBit16[4*i] &  hasBit16[4*i+1];
    hasFirstBit16[4*i+2] <= !hasBit16[4*i] & !hasBit16[4*i+1] &  hasBit16[4*i+2];
    hasFirstBit16[4*i+3] <= !hasBit16[4*i] & !hasBit16[4*i+1] & !hasBit16[4*i+2] & hasBit16[4*i+3];
end end
endgenerate
endmodule


module newExtendingProducer (
    input clk,
    input[127:0] graphIn,
    input[127:0] extendedIn,
    input shouldGrabNewSeedIn,
    input[1:0] hasBit64,
    input[4*8-1:0] hasFirstBit4,
    input[4*2-1:0] hasFirstBit16,
    output reg[127:0] newExtendingOut
);

genvar i;
genvar j;
wire[63:0] isFirstBit2;
generate
for(i = 0; i < 4; i = i + 1) begin
    for(j = 0; j < 4; j = j + 1) begin
        assign isFirstBit2[i*8+j*2+0] = (|graphIn[16*i+4*j +: 2]) & hasFirstBit4[4*i+j] & hasFirstBit16[4*0+i];
        assign isFirstBit2[i*8+j*2+1] = !(|graphIn[16*i+4*j +: 2]) & hasFirstBit4[4*i+j] & hasFirstBit16[4*0+i];
    end
end
for(i = 0; i < 4; i = i + 1) begin
    for(j = 0; j < 4; j = j + 1) begin
        assign isFirstBit2[32+i*8+j*2+0] = (|graphIn[64+16*i+4*j +: 2]) & hasFirstBit4[4*(4+i)+j] & hasFirstBit16[4*1+i] & !hasBit64[0];
        assign isFirstBit2[32+i*8+j*2+1] = !(|graphIn[64+16*i+4*j +: 2]) & hasFirstBit4[4*(4+i)+j] & hasFirstBit16[4*1+i] & !hasBit64[0];
    end
end
endgenerate

generate
// synthesizes to 1 ALM module a piece
for(i = 0; i < 64; i = i + 1) begin
    always @(posedge clk) newExtendingOut[i*2+:2] <= shouldGrabNewSeedIn ? (isFirstBit2[i] ? (graphIn[i*2] ? 2'b01 : 2'b10) : 2'b00) : extendedIn[i*2+:2];
end
endgenerate
endmodule


module newSeedProductionPipeline (
    input clk,
    input[127:0] graphIn,
    input[127:0] extendedIn,
    input[5:0] connectCountIn,
    input shouldGrabNewSeedIn,
    
    output[127:0] newCurExtendingOut,
    output reg[5:0] connectCountOut
);

// PIPELINE STEP 1 and 2
wire[1:0] hasBit64;
wire[4*8-1:0] hasFirstBit4;
wire[4*2-1:0] hasFirstBit16;
hasFirstBitAnalysis firstBitAnalysis(
    clk,
    graphIn,
    hasBit64,
    hasFirstBit4,
    hasFirstBit16
);

reg[127:0] graphD; always @(posedge clk) graphD <= graphIn;
reg[127:0] graphDD; always @(posedge clk) graphDD <= graphD;
reg[127:0] extendedD; always @(posedge clk) extendedD <= extendedIn;
reg[127:0] extendedDD; always @(posedge clk) extendedDD <= extendedD;
reg shouldGrabNewSeedD; always @(posedge clk) shouldGrabNewSeedD <= shouldGrabNewSeedIn;
reg shouldGrabNewSeedDD; always @(posedge clk) shouldGrabNewSeedDD <= shouldGrabNewSeedD;
reg[5:0] connectCountD; always @(posedge clk) connectCountD <= connectCountIn;
reg[5:0] connectCountDD; always @(posedge clk) connectCountDD <= connectCountD;

// PIPELINE STEP 3
newExtendingProducer resultProducer(
    clk,
    graphDD,
    extendedDD,
    shouldGrabNewSeedDD,
    hasBit64,
    hasFirstBit4,
    hasFirstBit16,
    newCurExtendingOut
);

// Now we can finally produce the resulting connectionCount
wire shouldIncrementConnectionCount = shouldGrabNewSeedDD & (hasBit64[0] | hasBit64[1]);
always @(posedge clk) connectCountOut <= connectCountDD + shouldIncrementConnectionCount;

endmodule


`define EXPLORATION_PIPE_STAGES 6
`define EXPLORATION_PIPE_LEFTOVER_GRAPH_DELAYS 3
`define NEW_SEED_PIPE_STAGES 3

module explorationPipeline(
    input clk,
    input[127:0] top,
    
    input[127:0] leftoverGraphInDelayed,
    input[127:0] curExtendingIn,
    
    output reg[127:0] reducedGraphOut,
    output reg[127:0] extendedOut,
    output reg runEnd,
    output shouldGrabNewSeedOut
);

// PIPELINE STEP 1, 2
wire[127:0] monotonizedUp; pipelinedMonotonizeUp mUp(clk, curExtendingIn, monotonizedUp);
reg[127:0] midPoint; always @(posedge clk) midPoint <= monotonizedUp & top;

// PIPELINE STEP 3, 4
wire[127:0] monotonizedDown; pipelinedMonotonizeDown mDown(clk, midPoint, monotonizedDown);
reg[127:0] extended; always @(posedge clk) extended <= leftoverGraphInDelayed & monotonizedDown;
reg[127:0] reducedGraph; always @(posedge clk) reducedGraph <= leftoverGraphInDelayed & ~monotonizedDown;

// delays
reg[127:0] midPointD; always @(posedge clk) midPointD <= midPoint;
reg[127:0] midPointDD; always @(posedge clk) midPointDD <= midPointD;

// PIPELINE STEP 5
reg[31:0] reducedGraphIsZeroIntermediates;
reg[63:0] extentionFinishedIntermediates;
reg[127:0] reducedGraphD; always @(posedge clk) reducedGraphD <= reducedGraph;
reg[127:0] extendedD; always @(posedge clk) extendedD <= extended;

genvar i;
generate
    for(i = 0; i < 32; i = i + 1) always @(posedge clk) reducedGraphIsZeroIntermediates[i] <= |reducedGraph[4*i +: 4];
    for(i = 0; i < 64; i = i + 1) always @(posedge clk) extentionFinishedIntermediates[i] <= (extended[2*i +: 2] == midPointDD[2*i +: 2]);
endgenerate

// PIPELINE STEP 6
always @(posedge clk) runEnd <= !(|reducedGraphIsZeroIntermediates); // the new leftoverGraph is empty, request a new graph
// split
reg[3:0] extentionFinished; // no change? Then grab the next seed to extend

generate
for(i = 0; i < 4; i = i + 1) always @(posedge clk) extentionFinished[i] <= &extentionFinishedIntermediates[16*i +: 16];
endgenerate

always @(posedge clk) begin
    reducedGraphOut <= reducedGraphD;
    extendedOut <= extendedD;
end

// a single OR gate, to define shouldGrabNewSeed. 
// VERY INTERESTING! This was also a horrible bug. The standard path is just shouldGrabNewSeed = extentionFinished. 
// But if the resulting left over graph is empty, then we must have reached the end of the exploration, so we can quit early!
// This saves a single cycle in rare cases, and it fixes the aforementioned horrible bug :P
assign shouldGrabNewSeedOut = runEnd | &extentionFinished;

endmodule

// requires a reset signal of at least `EXPLORATION_PIPE_STAGES + `NEW_SEED_PIPE_STAGES cycles, or more!
module pipelinedCountConnectedCore #(parameter EXTRA_DATA_WIDTH = 10, parameter DATA_IN_LATENCY = 4) (
	 input clk,
	 input rst,
	 input[127:0] top, // top wire remains constant for the duration of a run, no need to factor it into pipeline
	 
	 // input side
	 output request,
	 input[127:0] graphIn,
	 input start,
	 input[5:0] connectCountIn,
	 input[EXTRA_DATA_WIDTH-1:0] extraDataIn,
	 
	 // output side
	 output done,
	 output[5:0] connectCount,
	 output[EXTRA_DATA_WIDTH-1:0] extraDataOut
);

wire[127:0] leftoverGraph;
wire[127:0] curExtending;
wire[5:0] connectionCount;
wire valid;
wire[EXTRA_DATA_WIDTH-1:0] extraData;


wire[127:0] reducedGraphPreDelay;
wire[127:0] extendedPreDelay;
wire shouldGrabNewSeedPreDelay;
// PIPELINE STEP 1
explorationPipeline explorationPipe(clk, top, leftoverGraph, curExtending, reducedGraphPreDelay, extendedPreDelay, request, shouldGrabNewSeedPreDelay);

// PIPELINE STEP 4
// Produce outputs from this run if runEnd
assign done = valid & request;
assign extraDataOut = extraData;
assign connectCount = connectionCount;

// delay other wires for DATA_IN_LATENCY
wire[127:0] reducedGraph;
wire[127:0] extended;
wire shouldGrabNewSeed;
wire[127:0] leftoverGraphForSelection;
wire validPostDelay;
wire runEnd;
wire[5:0] connectionCountPostDelay;
wire[EXTRA_DATA_WIDTH-1:0] extraDataPostDelay;
hyperpipe #(.CYCLES(`EXPLORATION_PIPE_STAGES - `EXPLORATION_PIPE_LEFTOVER_GRAPH_DELAYS + DATA_IN_LATENCY), .WIDTH(128)) leftoverGraphToSelectorPipe(clk, leftoverGraph, leftoverGraphForSelection);
hyperpipe #(.CYCLES(DATA_IN_LATENCY), .WIDTH(128)) reducedGraphDataInPipe(clk, reducedGraphPreDelay, reducedGraph);
hyperpipe #(.CYCLES(DATA_IN_LATENCY), .WIDTH(128)) extendedDataInPipe(clk, extendedPreDelay, extended);
hyperpipe #(.CYCLES(DATA_IN_LATENCY), .WIDTH(1)) runEndDataInPipe(clk, request, runEnd);
hyperpipe #(.CYCLES(DATA_IN_LATENCY), .WIDTH(1)) shouldGrabNewSeedDataInPipe(clk, shouldGrabNewSeedPreDelay, shouldGrabNewSeed);
hyperpipe #(.CYCLES(DATA_IN_LATENCY), .WIDTH(1)) validDataInPipe(clk, valid, validPostDelay);
hyperpipe #(.CYCLES(DATA_IN_LATENCY), .WIDTH(6)) connectionCountDataInPipe(clk, connectionCount, connectionCountPostDelay);
hyperpipe #(.CYCLES(DATA_IN_LATENCY), .WIDTH(EXTRA_DATA_WIDTH)) extraDataDataInPipe(clk, extraData, extraDataPostDelay);

// PIPELINE STEP 5
// Inputs become available
reg[127:0] selectedLeftoverGraph;
always @(posedge clk, posedge rst) begin
    if(rst) selectedLeftoverGraph <= 0;
    else if(start) selectedLeftoverGraph <= graphIn; // synthesizes to 128 5-in 1-out LUTs
    else if(shouldGrabNewSeed) selectedLeftoverGraph <= reducedGraph;
    else selectedLeftoverGraph <= leftoverGraphForSelection;
end
reg[EXTRA_DATA_WIDTH-1:0] finalExtraData; always @(posedge clk) finalExtraData <= runEnd ? extraDataIn : extraDataPostDelay;
reg[5:0] selectedConnectCount; always @(posedge clk) selectedConnectCount <= start ? connectCountIn : connectionCountPostDelay;
reg finalValid; 
always @(posedge clk) begin
    if(start) 
        finalValid <= 1;
    else if(runEnd)
        finalValid <= 0;
    else
        finalValid <= validPostDelay;
end
reg shouldGrabNewSeedDelayed; always @(posedge clk) shouldGrabNewSeedDelayed <= shouldGrabNewSeed;
reg[127:0] extendedDelayed; always @(posedge clk) extendedDelayed <= extended;

// PIPELINE STEP 6
// Generation of new seed, find index and test if graph is 0 to increment connectCount
wire[5:0] finalConnectionCount;
newSeedProductionPipeline newSeedProductionPipe (clk, selectedLeftoverGraph, extendedDelayed, selectedConnectCount, shouldGrabNewSeedDelayed, curExtending, finalConnectionCount);

hyperpipe #(.CYCLES(`NEW_SEED_PIPE_STAGES + `EXPLORATION_PIPE_LEFTOVER_GRAPH_DELAYS), .WIDTH(128)) leftoverGraphPipe(clk, selectedLeftoverGraph, leftoverGraph);
hyperpipe #(.CYCLES(`EXPLORATION_PIPE_STAGES), .WIDTH(6)) connectCountPipe(clk, finalConnectionCount, connectionCount);
hyperpipe #(.CYCLES(`EXPLORATION_PIPE_STAGES + `NEW_SEED_PIPE_STAGES), .WIDTH(1)) validPipe(clk, finalValid, valid);
hyperpipe #(.CYCLES(`EXPLORATION_PIPE_STAGES + `NEW_SEED_PIPE_STAGES), .WIDTH(EXTRA_DATA_WIDTH)) extraDataPipe(clk, finalExtraData, extraData);

endmodule
