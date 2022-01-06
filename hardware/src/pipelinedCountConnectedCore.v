`timescale 1ns / 1ps

module hasFirstBitAnalysis(
    input clk,
    
    input[127:0] graphIn,
    output[1:0] hasBit64,
    // grouped in sets of 4, an element is marked '1' if its 4 bits contain the first bit of the 16 bits of the group
    output reg[4*8-1:0] hasFirstBit4,
    // grouped in sets of 4, an element is marked '1' if its 16 bits contain the first bit of the 64 bits of the group
    output[4*2-1:0] hasFirstBit16
);

wire[31:0] hasBit4;
reg[7:0] hasBit16;
genvar i;
generate
for(i = 0; i < 32; i = i + 1) begin assign hasBit4[i] = |graphIn[i*4+:4]; end
for(i = 0; i < 8; i = i + 1) begin always @(posedge clk) hasBit16[i] <= |hasBit4[i*4+:4]; end
for(i = 0; i < 2; i = i + 1) begin assign hasBit64[i] = |hasBit16[i*4+:4]; end
endgenerate

generate
for(i = 0; i < 8; i = i + 1) begin
    always @(posedge clk) begin
        hasFirstBit4[4*i+0] <= hasBit4[4*i];
        hasFirstBit4[4*i+1] <= !hasBit4[4*i] & hasBit4[4*i+1];
        hasFirstBit4[4*i+2] <= !hasBit4[4*i] & !hasBit4[4*i+1] & hasBit4[4*i+2];
        hasFirstBit4[4*i+3] <= !hasBit4[4*i] & !hasBit4[4*i+1] & !hasBit4[4*i+2] & hasBit4[4*i+3];
    end
end
endgenerate

generate
for(i = 0; i < 2; i = i + 1) begin
    assign hasFirstBit16[4*i+0] =  hasBit16[4*i];
    assign hasFirstBit16[4*i+1] = !hasBit16[4*i] &  hasBit16[4*i+1];
    assign hasFirstBit16[4*i+2] = !hasBit16[4*i] & !hasBit16[4*i+1] &  hasBit16[4*i+2];
    assign hasFirstBit16[4*i+3] = !hasBit16[4*i] & !hasBit16[4*i+1] & !hasBit16[4*i+2] & hasBit16[4*i+3];
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
    input[127:0] extended,
    input shouldGrabNewSeed,
    
    output shouldIncrementConnectionCount,
    output[127:0] newCurExtendingOut
);

reg[127:0] graphIn_START;
reg[127:0] extended_START;
reg shouldGrabNewSeed_START;

always @(posedge clk) begin
    graphIn_START <= graphIn;
    extended_START <= extended;
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
reg[127:0] graphIn_HASBIT;
reg[127:0] extended_HASBIT;
reg shouldGrabNewSeed_HASBIT;
always @(posedge clk) begin
    graphIn_HASBIT <= graphIn_START;
    extended_HASBIT <= extended_START;
    shouldGrabNewSeed_HASBIT <= shouldGrabNewSeed_START;
end

// PIPELINE STAGE
reg[1:0] hasBit64_RESULT;
reg[4*8-1:0] hasFirstBit4_RESULT;
reg[4*2-1:0] hasFirstBit16_RESULT;
reg[127:0] graphIn_RESULT;
reg[127:0] extended_RESULT;
reg shouldGrabNewSeed_RESULT;

always @(posedge clk) begin
    hasBit64_RESULT <= hasBit64_HASBIT;
    hasFirstBit4_RESULT <= hasFirstBit4_HASBIT;
    hasFirstBit16_RESULT <= hasFirstBit16_HASBIT;
    graphIn_RESULT <= graphIn_HASBIT;
    extended_RESULT <= extended_HASBIT;
    shouldGrabNewSeed_RESULT <= shouldGrabNewSeed_HASBIT;
end

// PIPELINE STEP 3
newExtendingProducer resultProducer(
    graphIn_RESULT,
    extended_RESULT,
    shouldGrabNewSeed_RESULT,
    hasBit64_RESULT,
    hasFirstBit4_RESULT,
    hasFirstBit16_RESULT,
    newCurExtendingOut
);

// Now we can finally produce the resulting connectionCount
assign shouldIncrementConnectionCount = shouldGrabNewSeed_RESULT & (hasBit64_RESULT[0] | hasBit64_RESULT[1]);

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

`define OTHER_DATA_WIDTH (128+128+1+1+1+6+EXTRA_DATA_WIDTH)

`define NEW_SEED_DEPTH 3
`define EXPLORATION_DEPTH 3

`define OFFSET_NSD `NEW_SEED_DEPTH
`define OFFSET_MID (`OFFSET_NSD+1)
`define OFFSET_EXPL (`OFFSET_MID+`EXPLORATION_DEPTH)
`define TOTAL_PIPELINE_STEAGES (`OFFSET_EXPL+1)


// The combinatorial pipeline that does all the work. Loopback is done outside of this module through combinatorialStateIn/Out
// Pipeline stages are marked by wire_STAGE for clarity
module pipelinedCountConnectedCombinatorial #(parameter EXTRA_DATA_WIDTH = 10, parameter STARTING_CONNECT_COUNT_LAG = 3) (
    input clk,
    input rst,
    
    // input side
    output reg request,
    input[127:0] graphIn,
    input start,
    input[5:0] startingConnectCountIn_DELAYED,
    input[EXTRA_DATA_WIDTH-1:0] extraDataIn,
    
    // output side
    output reg done,
    output reg[5:0] connectionCount,
    output reg[EXTRA_DATA_WIDTH-1:0] extraData,
    
    // state loop
    input[`OTHER_DATA_WIDTH-1:0] combinatorialStateIn,
    output reg[`OTHER_DATA_WIDTH-1:0] combinatorialStateOut
);

wire[127:0] extendedIn;
wire shouldGrabNewSeedIn;
wire[127:0] selectedLeftoverGraphIn;
wire validIn;
wire runEndIn;
wire[5:0] storedConnectionCountIn;
wire[EXTRA_DATA_WIDTH-1:0] storedExtraDataIn;
assign {selectedLeftoverGraphIn, extendedIn, runEndIn, shouldGrabNewSeedIn, validIn, storedConnectionCountIn, storedExtraDataIn} = combinatorialStateIn;

wire[EXTRA_DATA_WIDTH-1:0] extraDataWire = runEndIn ? extraDataIn : storedExtraDataIn;
wire validWire = start ? 1 : (runEndIn ? 0 : validIn);

// PIPELINE STEP 5
// Inputs become available
wire[127:0] leftoverGraphWire = rst ? 0 : start ? graphIn : selectedLeftoverGraphIn;

// PIPELINE STEP "NEW SEED PRODUCTION"
// Generation of new seed, find index and test if graph is 0 to increment connectCount

wire shouldIncrementConnectionCount_NSD;
wire[127:0] curExtendingWire_NSD;
newSeedProductionPipeline newSeedProductionPipe(clk, leftoverGraphWire, extendedIn, shouldGrabNewSeedIn, shouldIncrementConnectionCount_NSD, curExtendingWire_NSD);
// delays of other wires

wire start_NSD;
wire[EXTRA_DATA_WIDTH-1:0] extraDataWire_NSD;
wire[127:0] leftoverGraphWire_NSD;
wire[5:0] storedConnectionCountIn_NSD;
wire validWire_NSD;
shiftRegister #(.CYCLES(`NEW_SEED_DEPTH), .WIDTH(1+EXTRA_DATA_WIDTH+128+6+1)) newSeedProductionPipeBypassDelay(clk,
    {start, extraDataWire, leftoverGraphWire, storedConnectionCountIn, validWire},
    {start_NSD, extraDataWire_NSD, leftoverGraphWire_NSD, storedConnectionCountIn_NSD, validWire_NSD}
);

// PIPELINE STAGE
reg[127:0] curExtending_MID;
reg[127:0] leftoverGraph_MID;
reg validOut_MID;
reg[EXTRA_DATA_WIDTH-1:0] extraDataOut_MID;
always @(posedge clk) begin
    curExtending_MID <= curExtendingWire_NSD;
    leftoverGraph_MID <= leftoverGraphWire_NSD;
    validOut_MID <= validWire_NSD;
    extraDataOut_MID <= extraDataWire_NSD;
end

wire start_DELAYED;
wire shouldIncrementConnectionCount_DELAYED;
wire[5:0] storedConnectionCountIn_DELAYED;
shiftRegister #(.CYCLES(STARTING_CONNECT_COUNT_LAG - `OFFSET_NSD), .WIDTH(1+1+6)) startingConnectCountSyncPipe(clk,
    {start_NSD, shouldIncrementConnectionCount_NSD, storedConnectionCountIn_NSD},
    {start_DELAYED, shouldIncrementConnectionCount_DELAYED, storedConnectionCountIn_DELAYED}
);

wire[5:0] connectionCountOut_DELAYED = (start_DELAYED ? startingConnectCountIn_DELAYED : storedConnectionCountIn_DELAYED) + shouldIncrementConnectionCount_DELAYED;


// PIPELINE STEP "EXPLORATION"
wire[127:0] reducedGraph_EXPL;
wire[127:0] extendedOut_EXPL;
wire shouldGrabNewSeedOut_EXPL;
wire requestWire_EXPL;
explorationPipeline explorationPipe(clk, leftoverGraph_MID, curExtending_MID, reducedGraph_EXPL, extendedOut_EXPL, requestWire_EXPL, shouldGrabNewSeedOut_EXPL);


// delays
wire[127:0] leftoverGraph_EXPL;
wire validOut_EXPL;
wire[EXTRA_DATA_WIDTH-1:0] extraDataOut_EXPL;
shiftRegister #(.CYCLES(`EXPLORATION_DEPTH), .WIDTH(128+1+EXTRA_DATA_WIDTH)) explorationBypassDelay(clk,
    {leftoverGraph_MID , validOut_MID , extraDataOut_MID},
    {leftoverGraph_EXPL, validOut_EXPL, extraDataOut_EXPL}
);
wire[5:0] connectionCountOut_EXPL;
shiftRegister #(.CYCLES(`OFFSET_EXPL - STARTING_CONNECT_COUNT_LAG), .WIDTH(6)) connectCountOutSyncPipe(clk,
    connectionCountOut_DELAYED,
    connectionCountOut_EXPL
);

wire doneWire_EXPL = validOut_EXPL & requestWire_EXPL;
wire[127:0] selectedLeftoverGraphOut_EXPL = shouldGrabNewSeedOut_EXPL ? reducedGraph_EXPL : leftoverGraph_EXPL;


// OUTPUT PIPELINE STAGE
always @(posedge clk) combinatorialStateOut <= {selectedLeftoverGraphOut_EXPL, extendedOut_EXPL, requestWire_EXPL, shouldGrabNewSeedOut_EXPL, validOut_EXPL, connectionCountOut_EXPL, extraDataOut_EXPL};
always @(posedge clk) extraData <= extraDataOut_EXPL;
always @(posedge clk) connectionCount <= connectionCountOut_EXPL;
always @(posedge clk) done <= doneWire_EXPL;
always @(posedge clk) request <= requestWire_EXPL;
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

wire[`OTHER_DATA_WIDTH-1:0] combinatorialStateIn;
wire[`OTHER_DATA_WIDTH-1:0] combinatorialStateOut;

pipelinedCountConnectedCombinatorial #(EXTRA_DATA_WIDTH, STARTING_CONNECT_COUNT_LAG) combinatorialComponent (
    clk,
    rst,
    
    // input side
    request,
    graphIn,
    start,
    startingConnectCountIn_DELAYED,
    extraDataIn,
    
    // output side
    done,
    connectCount,
    extraDataOut,
    
    combinatorialStateIn,
    combinatorialStateOut
);

// delay other wires for DATA_IN_LATENCY
shiftRegister #(.CYCLES(DATA_IN_LATENCY), .WIDTH(`OTHER_DATA_WIDTH)) inputLatencyPipe(clk,
    combinatorialStateOut,
    combinatorialStateIn
);

endmodule
