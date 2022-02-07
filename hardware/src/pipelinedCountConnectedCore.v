`timescale 1ns / 1ps

`define NEW_SEED_HASBIT_DEPTH 3
`define NEW_SEED_HASBIT_OFFSET 1+`NEW_SEED_HASBIT_DEPTH
`define NEW_SEED_DEPTH 4
`define EXPLORATION_DOWN_OFFSET 4
`define EXPLORATION_DEPTH 6

`define OFFSET_NSD `NEW_SEED_DEPTH
`define OFFSET_MID (`OFFSET_NSD+1)
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

hyperpipe #(.CYCLES(`NEW_SEED_HASBIT_DEPTH), .WIDTH(128)) graphIn_START_TO_HASBIT_DELAY (clk, graphIn_START, graphIn_HASBIT);
hyperpipe #(.CYCLES(`NEW_SEED_HASBIT_DEPTH), .WIDTH(1), .MAX_FAN(5)) shouldGrabNewSeed_START_TO_HASBIT_DELAY (clk, shouldGrabNewSeed_START, shouldGrabNewSeed_HASBIT);

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
    
    output[127:0] selectedLeftoverGraphOut,
    output reg[127:0] extendedOut_DOWN,
    output reg runEnd,
    output reg shouldGrabNewSeedOut
);

reg[127:0] leftoverGraphIn_PRE_MID;
always @(posedge clk) begin
    leftoverGraphIn_PRE_MID <= leftoverGraphIn;
end

// PIPELINE STEP 1, 2
wire[127:0] monotonizedUp_PRE_MID; pipelinedMonotonizeUp mUp(clk, curExtendingIn, monotonizedUp_PRE_MID);
reg[127:0] midPoint_MID; always @(posedge clk) midPoint_MID <= monotonizedUp_PRE_MID & leftoverGraphIn_PRE_MID;

// delays 
reg[127:0] leftoverGraphIn_MID;
always @(posedge clk) begin
    leftoverGraphIn_MID <= leftoverGraphIn_PRE_MID;
end

// PIPELINE STEP 3, 4
wire[127:0] monotonizedDown_POST_MID; pipelinedMonotonizeDown mDown(clk, midPoint_MID, monotonizedDown_POST_MID);
reg[127:0] midPoint_POST_MID;
reg[127:0] leftoverGraphIn_POST_MID;
always @(posedge clk) begin
    midPoint_POST_MID <= midPoint_MID;
    leftoverGraphIn_POST_MID <= leftoverGraphIn_MID;
end

reg[127:0] midPoint_DOWN;
reg[127:0] reducedGraph_DOWN;
reg[127:0] leftoverGraphIn_DOWN;
always @(posedge clk) begin
    midPoint_DOWN <= midPoint_POST_MID;
    reducedGraph_DOWN <= leftoverGraphIn_POST_MID & ~monotonizedDown_POST_MID;
    extendedOut_DOWN <= leftoverGraphIn_POST_MID & monotonizedDown_POST_MID;
    leftoverGraphIn_DOWN <= leftoverGraphIn_POST_MID;
end

reg[127:0] midPoint_SUMMARIZE;
reg[127:0] reducedGraph_SUMMARIZE;
reg[127:0] leftoverGraphIn_SUMMARIZE;
always @(posedge clk) begin
    midPoint_SUMMARIZE <= midPoint_DOWN;
    reducedGraph_SUMMARIZE <= reducedGraph_DOWN;
    leftoverGraphIn_SUMMARIZE <= leftoverGraphIn_DOWN;
end


// PIPELINE STEP 5
reg[7:0] reducedGraphIsZeroIntermediates_SUMMARIZE;
reg[15:0] extentionFinishedIntermediates_SUMMARIZE;

genvar i;
generate
    for(i = 0; i < 8; i = i + 1) begin always @(posedge clk) reducedGraphIsZeroIntermediates_SUMMARIZE[i] <= |reducedGraph_DOWN[16*i +: 16]; end
    for(i = 0; i < 16; i = i + 1) begin always @(posedge clk) extentionFinishedIntermediates_SUMMARIZE[i] <= (extendedOut_DOWN[8*i +: 8] == midPoint_DOWN[8*i +: 8]); end
endgenerate
// PIPELINE STEP 6
wire runEnd_SUMMARIZE = !(|reducedGraphIsZeroIntermediates_SUMMARIZE); // the new leftoverGraph is empty, request a new graph
// split
 // no change? Then grab the next seed to extend
wire extentionFinished_SUMMARIZE = &extentionFinishedIntermediates_SUMMARIZE;

reg[127:0] reducedGraph_EXPL;
reg[127:0] leftoverGraphIn_EXPL;
// a single OR gate, to define shouldGrabNewSeed. 
// VERY INTERESTING! This was also a horrible bug. The standard path is just shouldGrabNewSeed = extentionFinished. 
// But if the resulting left over graph is empty, then we must have reached the end of the exploration, so we can quit early!
// This saves a single cycle in rare cases, and it fixes the aforementioned horrible bug :P
wire shouldGrabNewSeedWire = runEnd_SUMMARIZE | extentionFinished_SUMMARIZE;
always @(posedge clk) begin
    shouldGrabNewSeedOut <= shouldGrabNewSeedWire;
    runEnd <= runEnd_SUMMARIZE;
    reducedGraph_EXPL <= reducedGraph_SUMMARIZE;
    leftoverGraphIn_EXPL <= leftoverGraphIn_SUMMARIZE;
end

assign selectedLeftoverGraphOut = shouldGrabNewSeedOut ? reducedGraph_EXPL : leftoverGraphIn_EXPL;

endmodule

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
    output[127:0] extendedOut_DOWN,
    output[127:0] selectedLeftoverGraphOut_EXPL,
    output shouldGrabNewSeedOut_EXPL,
    output validOut, 
    output[5:0] connectionCountOut_EXPL, 
    output[EXTRA_DATA_WIDTH-1:0] storedExtraDataOut
);


wire rstLocalWire; // Manual reset tree, can't use constraints to have it generate it for me. 
hyperpipe #(.CYCLES(2), .MAX_FAN(20)) rstPipe(clk, rst, rstLocalWire);
(* max_fan = 1 *) reg rstLocal; always @(posedge clk) rstLocal <= rstLocalWire;


assign storedExtraDataOut = runEndIn ? extraDataIn : storedExtraDataIn;
assign validOut = start ? 1 : (runEndIn ? 0 : validIn);

// PIPELINE STEP 5
// Inputs become available
wire[127:0] leftoverGraphWire = rstLocal ? 0 : start ? graphIn : selectedLeftoverGraphIn;

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
hyperpipe #(.CYCLES(STARTING_CONNECT_COUNT_LAG - `OFFSET_NSD), .WIDTH(1+1+6), .MAX_FAN(5)) startingConnectCountSyncPipe(clk,
    {start_NSD, shouldIncrementConnectionCount_NSD, storedConnectionCountIn_NSD},
    {start_DELAYED, shouldIncrementConnectionCount_DELAYED, storedConnectionCountIn_DELAYED}
);

wire[5:0] connectionCountOut_DELAYED = (start_DELAYED ? startingConnectCountIn_DELAYED : storedConnectionCountIn_DELAYED) + shouldIncrementConnectionCount_DELAYED;


// PIPELINE STEP "EXPLORATION"
// output wire[127:0] selectedLeftoverGraphOut_EXPL;
// output wire[127:0] extendedOut_DOWN;
// output wire shouldGrabNewSeedOut_EXPL;
// output wire request_EXPL;
explorationPipeline explorationPipe(clk, leftoverGraph_MID, curExtending_MID, selectedLeftoverGraphOut_EXPL, extendedOut_DOWN, request_EXPL, shouldGrabNewSeedOut_EXPL);


// delays
// output wire[5:0] connectionCountOut_EXPL;
hyperpipe #(.CYCLES(`OFFSET_EXPL - STARTING_CONNECT_COUNT_LAG), .WIDTH(6)) connectCountOutSyncPipe(clk,
    connectionCountOut_DELAYED,
    connectionCountOut_EXPL
);
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

// Some extra register slack towards the collector to improve its fitting
hyperpipe #(.CYCLES(3), .WIDTH(1+6+EXTRA_DATA_WIDTH)) outputPipe (clk,
    {runEndIn & validIn, storedConnectionCountIn, storedExtraDataIn},
    {done, connectCount, extraDataOut}
);

wire requestOut_EXPL;
wire[127:0] extendedOut_DOWN;
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
    extendedOut_DOWN,
    selectedLeftoverGraphOut_EXPL,
    shouldGrabNewSeedOut_EXPL,
    validOut,
    connectionCountOut_EXPL,
    storedExtraDataOut
);

// delay other wires for DATA_IN_LATENCY
shiftRegister #(.CYCLES(`TOTAL_PIPELINE_STAGES - `OFFSET_DOWN + DATA_IN_LATENCY + `NEW_SEED_HASBIT_OFFSET), .WIDTH(128)) loopBackPipeExtended (clk,
    extendedOut_DOWN,
    extendedIn_HASBIT
);

// delay other wires for DATA_IN_LATENCY
hyperpipe #(.CYCLES(`TOTAL_PIPELINE_STAGES - `OFFSET_EXPL + DATA_IN_LATENCY), .WIDTH(128+1+1+6)) loopBackPipeSelectedLeftoverGraph(clk,
    {selectedLeftoverGraphOut_EXPL, requestOut_EXPL, shouldGrabNewSeedOut_EXPL, connectionCountOut_EXPL},
    {selectedLeftoverGraphIn, runEndIn, shouldGrabNewSeedIn, storedConnectionCountIn}
);

// delay other wires for DATA_IN_LATENCY
shiftRegister #(.CYCLES(`TOTAL_PIPELINE_STAGES + DATA_IN_LATENCY), .WIDTH(1+EXTRA_DATA_WIDTH)) loopBackPipeValidAndExtraData (clk,
    {validOut, storedExtraDataOut},
    {validIn, storedExtraDataIn}
);

endmodule
