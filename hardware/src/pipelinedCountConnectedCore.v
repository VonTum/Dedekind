`timescale 1ns / 1ps

`define NEW_SEED_HASBIT_DEPTH 3
`define NEW_SEED_HASBIT_OFFSET (1+`NEW_SEED_HASBIT_DEPTH)
`define NEW_SEED_DEPTH 4
`define EXPLORATION_DOWN_OFFSET 5
`define EXPLORATION_DEPTH 7

`define OFFSET_NSD `NEW_SEED_DEPTH
`define OFFSET_MID (`OFFSET_NSD+2)
`define OFFSET_DOWN (`OFFSET_MID+`EXPLORATION_DOWN_OFFSET)
`define OFFSET_EXPL (`OFFSET_MID+`EXPLORATION_DEPTH)
`define TOTAL_PIPELINE_STAGES `OFFSET_EXPL

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
reg[7:0] hasBit16;
genvar i;
generate
for(i = 0; i < 32; i = i + 1) begin always @(posedge clk) hasBit4[i] <= |graphIn[i*4+:4]; end
for(i = 0; i < 8; i = i + 1) begin always @(posedge clk) hasBit16[i] <= |hasBit4[i*4+:4]; end
for(i = 0; i < 2; i = i + 1) begin always @(posedge clk) hasBit64[i] = |hasBit16[i*4+:4]; end
endgenerate

reg[4*8-1:0] hasFirstBit4PreDelay;

generate
for(i = 0; i < 8; i = i + 1) begin
    always @(posedge clk) begin
        hasFirstBit4PreDelay[4*i+0] <= hasBit4[4*i];
        hasFirstBit4PreDelay[4*i+1] <= !hasBit4[4*i] & hasBit4[4*i+1];
        hasFirstBit4PreDelay[4*i+2] <= !hasBit4[4*i] & !hasBit4[4*i+1] & hasBit4[4*i+2];
        hasFirstBit4PreDelay[4*i+3] <= !hasBit4[4*i] & !hasBit4[4*i+1] & !hasBit4[4*i+2] & hasBit4[4*i+3];
    end
end
endgenerate
always @(posedge clk) hasFirstBit4 <= hasFirstBit4PreDelay;

generate
for(i = 0; i < 2; i = i + 1) begin
    always @(posedge clk) begin
        hasFirstBit16[4*i+0] <=  hasBit16[4*i];
        hasFirstBit16[4*i+1] <= !hasBit16[4*i] &  hasBit16[4*i+1];
        hasFirstBit16[4*i+2] <= !hasBit16[4*i] & !hasBit16[4*i+1] &  hasBit16[4*i+2];
        hasFirstBit16[4*i+3] <= !hasBit16[4*i] & !hasBit16[4*i+1] & !hasBit16[4*i+2] & hasBit16[4*i+3];
    end
end
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
    
    input[127:0] graphIn_START,
    input[127:0] extended_HASBIT,
    input shouldGrabNewSeed_START,
    
    output shouldIncrementConnectionCount,
    output[127:0] newCurExtendingOut_D
);

// PIPELINE STEP 1 and 2
wire[1:0] hasBit64_HASBIT;
wire[4*8-1:0] hasFirstBit4_HASBIT;
wire[4*2-1:0] hasFirstBit16_HASBIT;
hasFirstBitAnalysis firstBitAnalysis(
    clk,
    graphIn_START,
    hasBit64_HASBIT,
    hasFirstBit4_HASBIT,
    hasFirstBit16_HASBIT
);

// delays
wire[127:0] graphIn_HASBIT;
wire shouldGrabNewSeed_HASBIT;

hyperpipe #(.CYCLES(`NEW_SEED_HASBIT_DEPTH), .WIDTH(128)) graphIn_START_TO_HASBIT_DELAY (clk, graphIn_START, graphIn_HASBIT);
hyperpipe #(.CYCLES(`NEW_SEED_HASBIT_DEPTH), .WIDTH(1), .MAX_FAN(35)) shouldGrabNewSeed_START_TO_HASBIT_DELAY (clk, shouldGrabNewSeed_START, shouldGrabNewSeed_HASBIT);

// PIPELINE STEP 3
newExtendingProducer resultProducer(
    clk,
    graphIn_HASBIT,
    extended_HASBIT,
    shouldGrabNewSeed_HASBIT,
    hasBit64_HASBIT,
    hasFirstBit4_HASBIT,
    hasFirstBit16_HASBIT,
    newCurExtendingOut_D
);

// Now we can finally produce the resulting connectionCount
assign shouldIncrementConnectionCount = shouldGrabNewSeed_HASBIT & (hasBit64_HASBIT[0] | hasBit64_HASBIT[1]);

endmodule

module explorationPipeline(
    input clk,
    input[1:0] topChannel,
    
    input[127:0] leftoverGraph_PRE_DOWN,
    input[127:0] curExtendingIn,
    
    output reg[127:0] reducedGraphOut_DOWN,
    output reg[127:0] extendedOut_DOWN,
    output reg runEnd,
    output shouldGrabNewSeedOut
);

// PIPELINE STEP 1, 2, 3
wire[127:0] monotonizedUp_PRE_MID; pipelinedMonotonizeUp mUp(clk, curExtendingIn, monotonizedUp_PRE_MID);

// Instead of this
//reg[127:0] midPoint_MID; always @(posedge clk) midPoint_MID <= monotonizedUp_PRE_MID & leftoverGraphIn_PRE_MID;
// Use top, that way we can reduce resource usage by moving leftoverGraphIn to a longer shift register
wire[127:0] top;
topReceiver receiver(clk, topChannel, top);
reg[127:0] midPoint_MID; always @(posedge clk) midPoint_MID <= monotonizedUp_PRE_MID & top;

// PIPELINE STEP 3, 4
wire[127:0] monotonizedDown_PRE_DOWN; pipelinedMonotonizeDown mDown(clk, midPoint_MID, monotonizedDown_PRE_DOWN);
reg[127:0] midPoint_POST_MID;
reg[127:0] midPoint_PRE_DOWN;
always @(posedge clk) begin
    midPoint_POST_MID <= midPoint_MID;
    midPoint_PRE_DOWN <= midPoint_POST_MID;
end

reg[127:0] midPoint_DOWN;
reg[127:0] leftoverGraphIn_DOWN;
always @(posedge clk) begin
    midPoint_DOWN <= midPoint_PRE_DOWN;
    reducedGraphOut_DOWN <= leftoverGraph_PRE_DOWN & ~monotonizedDown_PRE_DOWN;
    extendedOut_DOWN <= leftoverGraph_PRE_DOWN & monotonizedDown_PRE_DOWN;
    leftoverGraphIn_DOWN <= leftoverGraph_PRE_DOWN;
end

// PIPELINE STEP 5
reg[7:0] reducedGraphIsZeroIntermediates_SUMMARIZE;
reg[15:0] extentionFinishedIntermediates_SUMMARIZE;

genvar i;
generate
    for(i = 0; i < 8; i = i + 1) begin always @(posedge clk) reducedGraphIsZeroIntermediates_SUMMARIZE[i] <= |reducedGraphOut_DOWN[16*i +: 16]; end
    for(i = 0; i < 16; i = i + 1) begin always @(posedge clk) extentionFinishedIntermediates_SUMMARIZE[i] <= (extendedOut_DOWN[8*i +: 8] == midPoint_DOWN[8*i +: 8]); end
endgenerate
// PIPELINE STEP 6
always @(posedge clk) runEnd <= !(|reducedGraphIsZeroIntermediates_SUMMARIZE); // the new leftoverGraph is empty, request a new graph
// split
 // no change? Then grab the next seed to extend
reg extentionFinished_SUMMARIZE_D; always @(posedge clk) extentionFinished_SUMMARIZE_D <= &extentionFinishedIntermediates_SUMMARIZE;

// a single OR gate, to define shouldGrabNewSeed. 
// VERY INTERESTING! This was also a horrible bug. The standard path is just shouldGrabNewSeed = extentionFinished. 
// But if the resulting left over graph is empty, then we must have reached the end of the exploration, so we can quit early!
// This saves a single cycle in rare cases, and it fixes the aforementioned horrible bug :P
assign shouldGrabNewSeedOut = runEnd | extentionFinished_SUMMARIZE_D;

endmodule

// The combinatorial pipeline that does all the work. Loopback is done outside of this module through combinatorialStateIn/Out
// Pipeline stages are marked by wire_STAGE for clarity
// If graphInAvailable == 0, then graphIn must == 128'b0
module pipelinedCountConnectedCombinatorial #(parameter EXTRA_DATA_WIDTH = 10) (
    input clk,
    input rst,
    input[1:0] topChannel,
    
    // input side
    input[127:0] graphIn,
    input graphInAvailable,
    input[EXTRA_DATA_WIDTH-1:0] extraDataIn,
    
    // state loop
    input runEndIn,
    input[127:0] extendedIn_HASBIT,
    input[127:0] leftoverGraphIn,
    input[127:0] reducedGraphIn,
    input shouldGrabNewSeedIn,
    input[1:0] graphSelectorIn,
    input validIn,
    input[5:0] storedConnectionCountIn_NSD,
    input[EXTRA_DATA_WIDTH-1:0] storedExtraDataIn,
    
    output request_EXPL,
    output[127:0] extendedOut_DOWN,
    output[127:0] leftoverGraphOut_PRE_DOWN,
    output[127:0] reducedGraphOut_DOWN,
    output shouldGrabNewSeedOut_EXPL,
    output[1:0] graphSelectorOut_EXPL,
    output reg validOut_D, 
    output reg[5:0] connectionCountOut_NSD_D, 
    output reg[EXTRA_DATA_WIDTH-1:0] storedExtraDataOut_D,
    output reg eccStatus
);

always @(posedge clk) storedExtraDataOut_D <= runEndIn ? extraDataIn : storedExtraDataIn;
always @(posedge clk) validOut_D <= runEndIn ? graphInAvailable : validIn;

// PIPELINE STEP 5
`define GRAPH_RESET 2'b00
`define GRAPH_START 2'b01
`define GRAPH_NEW_SEED 2'b10
`define GRAPH_LEFTOVER 2'b11

wire[127:0] graphMultiplexer[3:0];
assign graphMultiplexer[`GRAPH_RESET] = 128'b0;
assign graphMultiplexer[`GRAPH_START] = graphIn;
assign graphMultiplexer[`GRAPH_NEW_SEED] = reducedGraphIn;
assign graphMultiplexer[`GRAPH_LEFTOVER] = leftoverGraphIn;

// PIPELINE STEP "NEW SEED PRODUCTION"
// Generation of new seed, find index and test if graph is 0 to increment connectCount

reg[127:0] leftoverGraph_START; always @(posedge clk) leftoverGraph_START <= graphMultiplexer[graphSelectorIn];
reg shouldGrabNewSeed_START;

always @(posedge clk) shouldGrabNewSeed_START <= shouldGrabNewSeedIn;

wire shouldIncrementConnectionCount_NSD;
wire[127:0] curExtending_MID;
newSeedProductionPipeline newSeedProductionPipe(clk, leftoverGraph_START, extendedIn_HASBIT, shouldGrabNewSeed_START, shouldIncrementConnectionCount_NSD, curExtending_MID);

wire eccStatusWire;
always @(posedge clk) eccStatus <= eccStatusWire;
shiftRegister_M20K #(.CYCLES(`OFFSET_DOWN - 2), .WIDTH(128)) newSeedProductionPipeBypassLeftoverGraphWireDelay(clk,
    leftoverGraph_START,
    leftoverGraphOut_PRE_DOWN,
    eccStatusWire
);

wire runEndIn_NSD;
hyperpipe #(.CYCLES(`OFFSET_NSD), .WIDTH(1)) runEndInPipe(clk,
    runEndIn,
    runEndIn_NSD
);

always @(posedge clk) connectionCountOut_NSD_D <= (runEndIn_NSD ? 0 : storedConnectionCountIn_NSD) + shouldIncrementConnectionCount_NSD;


// PIPELINE STEP "EXPLORATION"
// output wire[127:0] selectedLeftoverGraphOut_EXPL;
// output wire[127:0] extendedOut_DOWN;
// output wire shouldGrabNewSeedOut_EXPL;
// output wire request_EXPL;
explorationPipeline explorationPipe(clk, topChannel, leftoverGraphOut_PRE_DOWN, curExtending_MID, reducedGraphOut_DOWN, extendedOut_DOWN, request_EXPL, shouldGrabNewSeedOut_EXPL);


wire rstLocal; // Manual reset tree, can't use constraints to have it generate it for me. 
hyperpipe #(.CYCLES(2)) rstPipe(clk, rst, rstLocal);

// Inputs become available
assign graphSelectorOut_EXPL = 
    rstLocal ? `GRAPH_RESET : 
    request_EXPL ? `GRAPH_START : 
    shouldGrabNewSeedOut_EXPL ? `GRAPH_NEW_SEED : `GRAPH_LEFTOVER;

endmodule


// requires a reset signal of at least 2*(MAX_PIPELINE_DEPTH+DATA_IN_LATENCY) cycles, or more!
module pipelinedCountConnectedCore #(parameter EXTRA_DATA_WIDTH = 10, parameter DATA_IN_LATENCY = 4) (
    input clk,
    input rst,
    input[1:0] topChannel,
    output isActive, // Instrumentation wire for profiling
    
    // input side
    output request,
    input[127:0] graphIn,
    input graphInAvailable,
    input[EXTRA_DATA_WIDTH-1:0] extraDataIn,
    
    // output side
    output done,
    output[5:0] connectCountOut,
    output[EXTRA_DATA_WIDTH-1:0] extraDataOut,
    output reg eccStatus
);

wor eccStatusWOR;
always @(posedge clk) eccStatus <= eccStatusWOR;

wire runEndIn;
wire[127:0] extendedIn_HASBIT;
wire[127:0] leftoverGraphIn;
wire[127:0] reducedGraphIn;
wire shouldGrabNewSeedIn;
wire[1:0] graphSelectorIn;
wire validIn;
wire[5:0] storedConnectionCountIn_NSD;
wire[EXTRA_DATA_WIDTH-1:0] storedExtraDataIn;

wire requestOut_EXPL;
wire[127:0] extendedOut_DOWN;
wire[127:0] leftoverGraphOut_PRE_DOWN;
wire[127:0] reducedGraphOut_DOWN;
wire shouldGrabNewSeedOut_EXPL;
wire[1:0] graphSelectorOut_EXPL;
wire validOut_D;
wire[5:0] connectionCountOut_NSD_D;
wire[EXTRA_DATA_WIDTH-1:0] storedExtraDataOut_D;

assign request = requestOut_EXPL;

pipelinedCountConnectedCombinatorial #(EXTRA_DATA_WIDTH) combinatorialComponent (
    clk,
    rst,
    topChannel,
    
    // input side
    graphIn,
    graphInAvailable,
    extraDataIn,
    
    // combinatorial state loop
    runEndIn,
    extendedIn_HASBIT,
    leftoverGraphIn,
    reducedGraphIn,
    shouldGrabNewSeedIn,
    graphSelectorIn,
    validIn,
    storedConnectionCountIn_NSD,
    storedExtraDataIn,
    
    requestOut_EXPL,
    extendedOut_DOWN,
    leftoverGraphOut_PRE_DOWN,
    reducedGraphOut_DOWN,
    shouldGrabNewSeedOut_EXPL,
    graphSelectorOut_EXPL,
    validOut_D,
    connectionCountOut_NSD_D,
    storedExtraDataOut_D,
    eccStatusWOR
);

wire eccStatusLoopBackPipeExtendedECCWire;
reg eccStatusLoopBackPipeExtendedECC; always @(posedge clk) eccStatusLoopBackPipeExtendedECC <= eccStatusLoopBackPipeExtendedECCWire;
assign eccStatusWOR = eccStatusLoopBackPipeExtendedECC;

shiftRegister_M20K #(.CYCLES(`TOTAL_PIPELINE_STAGES - `OFFSET_DOWN + DATA_IN_LATENCY + `NEW_SEED_HASBIT_OFFSET), .WIDTH(128)) loopBackPipeExtended (clk,
    extendedOut_DOWN,
    extendedIn_HASBIT,
    eccStatusLoopBackPipeExtendedECCWire
);

// delays
wire[5:0] storedConnectionCountIn;
hyperpipe #(.CYCLES(`TOTAL_PIPELINE_STAGES + DATA_IN_LATENCY - (`OFFSET_NSD + 1)), .WIDTH(6)) connectCountOutSyncPipe(clk,
    connectionCountOut_NSD_D,
    storedConnectionCountIn
);

hyperpipe #(.CYCLES(`TOTAL_PIPELINE_STAGES - `OFFSET_EXPL + DATA_IN_LATENCY), .WIDTH(1+1+2)) loopBackPipeAllData(clk,
    {requestOut_EXPL, shouldGrabNewSeedOut_EXPL, graphSelectorOut_EXPL},
    {runEndIn, shouldGrabNewSeedIn, graphSelectorIn}
);

hyperpipe #(.CYCLES(`OFFSET_NSD), .WIDTH(6)) storedConnectionCountDelayedPipe(clk,
    storedConnectionCountIn,
    storedConnectionCountIn_NSD
);

hyperpipe #(.CYCLES(`TOTAL_PIPELINE_STAGES - `OFFSET_DOWN + DATA_IN_LATENCY), .WIDTH(128)) loopBackPipeReducedGraph(clk,
    reducedGraphOut_DOWN,
    reducedGraphIn
);

wire loopBackPipeLeftOverGraphECCWire;
reg loopBackPipeLeftOverGraphECC; always @(posedge clk) loopBackPipeLeftOverGraphECC <= loopBackPipeLeftOverGraphECCWire;
assign eccStatusWOR = loopBackPipeLeftOverGraphECC;

shiftRegister_M20K #(.CYCLES(`TOTAL_PIPELINE_STAGES - (`OFFSET_DOWN-1) + DATA_IN_LATENCY), .WIDTH(128)) loopBackPipeLeftoverGraph(clk,
    leftoverGraphOut_PRE_DOWN,
    leftoverGraphIn,
    loopBackPipeLeftOverGraphECCWire
);

wire loopBackPipeValidAndExtraDataECCWire;
reg loopBackPipeValidAndExtraDataECC; always @(posedge clk) loopBackPipeValidAndExtraDataECC <= loopBackPipeValidAndExtraDataECCWire;
assign eccStatusWOR = loopBackPipeValidAndExtraDataECC;

shiftRegister_M20K #(.CYCLES(`TOTAL_PIPELINE_STAGES - 1 + DATA_IN_LATENCY), .WIDTH(1+EXTRA_DATA_WIDTH)) loopBackPipeValidAndExtraData (clk,
    {validOut_D, storedExtraDataOut_D},
    {validIn, storedExtraDataIn},
    loopBackPipeValidAndExtraDataECCWire
);

// Lots of slack, latency isn't important and we want minimal influence on the resulting hardware
hyperpipe #(.CYCLES(7)) activityMonitorPipe(clk, validIn, isActive);

`define OUTPUT_DELAY 3
hyperpipe #(.CYCLES(`OUTPUT_DELAY)) donePipe(clk, runEndIn & validIn, done);
hyperpipe #(.CYCLES(`OUTPUT_DELAY), .WIDTH(6)) connectCountOutPipe(clk, storedConnectionCountIn, connectCountOut);
hyperpipe #(.CYCLES(`OUTPUT_DELAY), .WIDTH(EXTRA_DATA_WIDTH)) extraDataOutPipe(clk, storedExtraDataIn, extraDataOut);

endmodule
