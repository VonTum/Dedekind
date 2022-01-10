`timescale 1ns / 1ps

`define NEW_SEED_HASBIT_DEPTH 3
`define NEW_SEED_HASBIT_OFFSET 1+`NEW_SEED_HASBIT_DEPTH
`define NEW_SEED_DEPTH 4
`define EXPLORATION_DEPTH 3

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
    input[127:0] graphIn,
    input[127:0] extendedIn,
    input shouldGrabNewSeedIn,
    input[1:0] hasBit64,
    input[4*8-1:0] hasFirstBit4,
    input[4*2-1:0] hasFirstBit16,
    output[127:0] newExtendingOut
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
    assign newExtendingOut[i*2+:2] = shouldGrabNewSeedIn ? (isFirstBit2[i] ? (graphIn[i*2] ? 2'b01 : 2'b10) : 2'b00) : extendedIn[i*2+:2];
end
endgenerate
endmodule


module newSeedProductionPipeline (
    input clk,
    
    input[127:0] graphIn,
    input[127:0] extended_HASBIT,
    input shouldGrabNewSeed,
    
    output shouldIncrementConnectionCount,
    output[127:0] newCurExtendingOut
);

reg[127:0] graphIn_START;
reg shouldGrabNewSeed_START;

always @(posedge clk) begin
    graphIn_START <= graphIn;
    shouldGrabNewSeed_START <= shouldGrabNewSeed;
end

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

hyperpipe #(.CYCLES(`NEW_SEED_HASBIT_DEPTH), .WIDTH(128+1)) START_TO_HASBIT_DELAY (clk,
    {graphIn_START, shouldGrabNewSeed_START},
    {graphIn_HASBIT, shouldGrabNewSeed_HASBIT}
);

// PIPELINE STEP 3
newExtendingProducer resultProducer(
    graphIn_HASBIT,
    extended_HASBIT,
    shouldGrabNewSeed_HASBIT,
    hasBit64_HASBIT,
    hasFirstBit4_HASBIT,
    hasFirstBit16_HASBIT,
    newCurExtendingOut
);

// Now we can finally produce the resulting connectionCount
assign shouldIncrementConnectionCount = shouldGrabNewSeed_HASBIT & (hasBit64_HASBIT[0] | hasBit64_HASBIT[1]);

endmodule

module explorationPipeline(
    input clk,
    
    input[127:0] leftoverGraphIn,
    input[127:0] curExtendingIn,
    
    output reg[127:0] reducedGraphOut,
    output reg[127:0] extendedOut,
    output reg runEnd,
    output reg shouldGrabNewSeedOut
);

// PIPELINE STEP 1, 2
wire[127:0] monotonizedUp; monotonizeUp mUp(curExtendingIn, monotonizedUp);
reg[127:0] midPoint_MID; always @(posedge clk) midPoint_MID <= monotonizedUp & leftoverGraphIn;

// delays 
reg[127:0] leftoverGraphIn_MID;
always @(posedge clk) begin
    leftoverGraphIn_MID <= leftoverGraphIn;
end

// PIPELINE STEP 3, 4
wire[127:0] monotonizedDown_MID; monotonizeDown mDown(midPoint_MID, monotonizedDown_MID);

reg[127:0] monotonizedDown_DOWN;
reg[127:0] leftoverGraphIn_DOWN;
reg[127:0] midPoint_DOWN;
always @(posedge clk) begin
    monotonizedDown_DOWN <= monotonizedDown_MID;
    leftoverGraphIn_DOWN <= leftoverGraphIn_MID;
    midPoint_DOWN <= midPoint_MID;
end

wire[127:0] reducedGraphOut_DOWN = leftoverGraphIn_DOWN & ~monotonizedDown_DOWN;
wire[127:0] extendedOut_DOWN = leftoverGraphIn_DOWN & monotonizedDown_DOWN;

// PIPELINE STEP 5
wire[31:0] reducedGraphIsZeroIntermediates_DOWN;
wire[63:0] extentionFinishedIntermediates_DOWN;

genvar i;
generate
    for(i = 0; i < 32; i = i + 1) begin assign reducedGraphIsZeroIntermediates_DOWN[i] = |reducedGraphOut_DOWN[4*i +: 4]; end
    for(i = 0; i < 64; i = i + 1) begin assign extentionFinishedIntermediates_DOWN[i] = (extendedOut_DOWN[2*i +: 2] == midPoint_DOWN[2*i +: 2]); end
endgenerate
// PIPELINE STEP 6
wire runEnd_DOWN = !(|reducedGraphIsZeroIntermediates_DOWN); // the new leftoverGraph is empty, request a new graph
// split
wire[3:0] extentionFinished_DOWN; // no change? Then grab the next seed to extend

generate
for(i = 0; i < 4; i = i + 1) begin assign extentionFinished_DOWN[i] = &extentionFinishedIntermediates_DOWN[16*i +: 16]; end
endgenerate

// a single OR gate, to define shouldGrabNewSeed. 
// VERY INTERESTING! This was also a horrible bug. The standard path is just shouldGrabNewSeed = extentionFinished. 
// But if the resulting left over graph is empty, then we must have reached the end of the exploration, so we can quit early!
// This saves a single cycle in rare cases, and it fixes the aforementioned horrible bug :P
always @(posedge clk) begin
    shouldGrabNewSeedOut <= runEnd_DOWN | &extentionFinished_DOWN;
    runEnd <= runEnd_DOWN;
    extendedOut <= extendedOut_DOWN;
    reducedGraphOut <= reducedGraphOut_DOWN;
end

endmodule

`define OFFSET_NSD `NEW_SEED_DEPTH
`define OFFSET_MID (`OFFSET_NSD+1)
`define OFFSET_EXPL (`OFFSET_MID+`EXPLORATION_DEPTH)
`define TOTAL_PIPELINE_STAGES `OFFSET_EXPL


// The combinatorial pipeline that does all the work. Loopback is done outside of this module through combinatorialStateIn/Out
// Pipeline stages are marked by wire_STAGE for clarity
module pipelinedCountConnectedCombinatorial #(parameter EXTRA_DATA_WIDTH = 10, parameter STARTING_CONNECT_COUNT_LAG = 3) (
    input clk,
    input rst,
    
    // input side
    input[127:0] graphIn,
    input start,
    input[5:0] startingConnectCountIn_DELAYED,
    input[EXTRA_DATA_WIDTH-1:0] extraDataIn,
    
    // state loop
    input runEndIn,
    input[127:0] extendedIn_HASBIT,
    input[127:0] selectedLeftoverGraphIn,
    input shouldGrabNewSeedIn, 
    input validIn, 
    input[5:0] storedConnectionCountIn, 
    input[EXTRA_DATA_WIDTH-1:0] storedExtraDataIn,
    
    output request_EXPL,
    output[127:0] extendedOut_EXPL,
    output[127:0] selectedLeftoverGraphOut_EXPL,
    output shouldGrabNewSeedOut_EXPL,
    output validOut, 
    output[5:0] connectionCountOut_EXPL, 
    output[EXTRA_DATA_WIDTH-1:0] storedExtraDataOut
);

assign storedExtraDataOut = runEndIn ? extraDataIn : storedExtraDataIn;
assign validOut = start ? 1 : (runEndIn ? 0 : validIn);

// PIPELINE STEP 5
// Inputs become available
wire[127:0] leftoverGraphWire = rst ? 0 : start ? graphIn : selectedLeftoverGraphIn;

// PIPELINE STEP "NEW SEED PRODUCTION"
// Generation of new seed, find index and test if graph is 0 to increment connectCount

wire shouldIncrementConnectionCount_NSD;
wire[127:0] curExtendingWire_NSD;
newSeedProductionPipeline newSeedProductionPipe(clk, leftoverGraphWire, extendedIn_HASBIT, shouldGrabNewSeedIn, shouldIncrementConnectionCount_NSD, curExtendingWire_NSD);
// delays of other wires

wire start_NSD;
wire[127:0] leftoverGraphWire_NSD;
wire[5:0] storedConnectionCountIn_NSD;
hyperpipe #(.CYCLES(`NEW_SEED_DEPTH), .WIDTH(1+128+6)) newSeedProductionPipeBypassDelay(clk,
    {start, leftoverGraphWire, storedConnectionCountIn},
    {start_NSD, leftoverGraphWire_NSD, storedConnectionCountIn_NSD}
);

// PIPELINE STAGE
reg[127:0] curExtending_MID;
reg[127:0] leftoverGraph_MID;
always @(posedge clk) begin
    curExtending_MID <= curExtendingWire_NSD;
    leftoverGraph_MID <= leftoverGraphWire_NSD;
end

wire start_DELAYED;
wire shouldIncrementConnectionCount_DELAYED;
wire[5:0] storedConnectionCountIn_DELAYED;
hyperpipe #(.CYCLES(STARTING_CONNECT_COUNT_LAG - `OFFSET_NSD), .WIDTH(1+1+6)) startingConnectCountSyncPipe(clk,
    {start_NSD, shouldIncrementConnectionCount_NSD, storedConnectionCountIn_NSD},
    {start_DELAYED, shouldIncrementConnectionCount_DELAYED, storedConnectionCountIn_DELAYED}
);

wire[5:0] connectionCountOut_DELAYED = (start_DELAYED ? startingConnectCountIn_DELAYED : storedConnectionCountIn_DELAYED) + shouldIncrementConnectionCount_DELAYED;


// PIPELINE STEP "EXPLORATION"
wire[127:0] reducedGraph_EXPL;
// output wire[127:0] extendedOut_EXPL;
// output wire shouldGrabNewSeedOut_EXPL;
// output wire request_EXPL;
explorationPipeline explorationPipe(clk, leftoverGraph_MID, curExtending_MID, reducedGraph_EXPL, extendedOut_EXPL, request_EXPL, shouldGrabNewSeedOut_EXPL);


// delays
wire[127:0] leftoverGraph_EXPL;
hyperpipe #(.CYCLES(`EXPLORATION_DEPTH), .WIDTH(128)) explorationBypassDelay(clk,
    leftoverGraph_MID,
    leftoverGraph_EXPL
);
// output wire[5:0] connectionCountOut_EXPL;
hyperpipe #(.CYCLES(`OFFSET_EXPL - STARTING_CONNECT_COUNT_LAG), .WIDTH(6)) connectCountOutSyncPipe(clk,
    connectionCountOut_DELAYED,
    connectionCountOut_EXPL
);

// output wire[127:0] selectedLeftoverGraphOut_EXPL;
assign selectedLeftoverGraphOut_EXPL = shouldGrabNewSeedOut_EXPL ? reducedGraph_EXPL : leftoverGraph_EXPL;
endmodule


// requires a reset signal of at least 2*(MAX_PIPELINE_DEPTH+DATA_IN_LATENCY) cycles, or more!
module pipelinedCountConnectedCore #(parameter EXTRA_DATA_WIDTH = 10, parameter DATA_IN_LATENCY = 4, parameter STARTING_CONNECT_COUNT_LAG = 3) (
    input clk,
    input rst,
    
    // input side
    output request,
    input[127:0] graphIn,
    input start,
    input[5:0] startingConnectCountIn_DELAYED,
    input[EXTRA_DATA_WIDTH-1:0] extraDataIn,
    
    // output side
    output done,
    output[5:0] connectCount,
    output[EXTRA_DATA_WIDTH-1:0] extraDataOut
);

wire runEndIn;
wire[127:0] extendedIn_HASBIT;
wire[127:0] selectedLeftoverGraphIn;
wire shouldGrabNewSeedIn;
wire validIn;
wire[5:0] storedConnectionCountIn;
wire[EXTRA_DATA_WIDTH-1:0] storedExtraDataIn;

assign done = runEndIn & validIn;
assign connectCount = storedConnectionCountIn;
assign extraDataOut = storedExtraDataIn;

wire requestOut_EXPL;
wire[127:0] extendedOut_EXPL;
wire[127:0] selectedLeftoverGraphOut_EXPL;
wire shouldGrabNewSeedOut_EXPL;
wire validOut;
wire[5:0] connectionCountOut_EXPL;
wire[EXTRA_DATA_WIDTH-1:0] storedExtraDataOut;

assign request = requestOut_EXPL;

pipelinedCountConnectedCombinatorial #(EXTRA_DATA_WIDTH, STARTING_CONNECT_COUNT_LAG) combinatorialComponent (
    clk,
    rst,
    
    // input side
    graphIn,
    start,
    startingConnectCountIn_DELAYED,
    extraDataIn,
    
    // combinatorial state loop
    runEndIn,
    extendedIn_HASBIT,
    selectedLeftoverGraphIn,
    shouldGrabNewSeedIn,
    validIn,
    storedConnectionCountIn,
    storedExtraDataIn,
    
    requestOut_EXPL,
    extendedOut_EXPL,
    selectedLeftoverGraphOut_EXPL,
    shouldGrabNewSeedOut_EXPL,
    validOut,
    connectionCountOut_EXPL,
    storedExtraDataOut
);

// delay other wires for DATA_IN_LATENCY
shiftRegister #(.CYCLES(`TOTAL_PIPELINE_STAGES - `OFFSET_EXPL + DATA_IN_LATENCY + `NEW_SEED_HASBIT_OFFSET), .WIDTH(128)) loopBackPipeExtended (clk,
    extendedOut_EXPL,
    extendedIn_HASBIT
);

// delay other wires for DATA_IN_LATENCY
shiftRegister #(.CYCLES(`TOTAL_PIPELINE_STAGES - `OFFSET_EXPL + DATA_IN_LATENCY), .WIDTH(128+1+1+6)) loopBackPipeSelectedLeftoverGraph(clk,
    {selectedLeftoverGraphOut_EXPL, requestOut_EXPL, shouldGrabNewSeedOut_EXPL, connectionCountOut_EXPL},
    {selectedLeftoverGraphIn, runEndIn, shouldGrabNewSeedIn, storedConnectionCountIn}
);

// delay other wires for DATA_IN_LATENCY
shiftRegister #(.CYCLES(`TOTAL_PIPELINE_STAGES + DATA_IN_LATENCY), .WIDTH(1+EXTRA_DATA_WIDTH)) loopBackPipeValidAndExtraData (clk,
    {validOut, storedExtraDataOut},
    {validIn, storedExtraDataIn}
);

endmodule
