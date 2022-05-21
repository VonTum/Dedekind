`timescale 1ns / 1ps

`define NEW_SEED_HASBIT_DEPTH 3
`define NEW_SEED_HASBIT_OFFSET (1+`NEW_SEED_HASBIT_DEPTH)
`define NEW_SEED_DEPTH (`NEW_SEED_HASBIT_DEPTH+1)
`define EXPLORATION_DOWN_OFFSET 5
`define EXPLORATION_DEPTH 7

`define OFFSET_NSD `NEW_SEED_DEPTH
`define OFFSET_MID (`OFFSET_NSD+2)
`define OFFSET_DOWN (`OFFSET_MID+`EXPLORATION_DOWN_OFFSET)
`define OFFSET_EXPL (`OFFSET_MID+`EXPLORATION_DEPTH)
`define TOTAL_PIPELINE_STAGES `OFFSET_EXPL

module hasFirstBitAnalysis (
    input clk,
    
    input[127:0] graphIn,
    // Grouped by 4, denotes if this set of 4 bits contains the first bit
    output reg[31:0] firstBit4,
    output reg isEmpty
);

reg[31:0] hasBit4;
reg[31:0] hasBit4D; always @(posedge clk) hasBit4D <= hasBit4;
reg[7:0] hasBit16;
wire[8:0] firstBit16;
genvar i;
generate
for(i = 0; i < 32; i = i + 1) begin always @(posedge clk) hasBit4[i] <= |graphIn[i*4+:4]; end
for(i = 0; i < 8; i = i + 1) begin always @(posedge clk) hasBit16[i] <= |hasBit4[i*4+:4]; end
assign firstBit16[0] = 1;
assign firstBit16[1] = !hasBit16[0];
for(i = 2; i < 9; i = i + 1) begin assign firstBit16[i] = firstBit16[i-1] && !hasBit16[i-1]; end
for(i = 0; i < 8; i = i + 1) begin
    always @(posedge clk) begin
        firstBit4[4*i] <= firstBit16[i];
        firstBit4[4*i+1] <= firstBit16[i] && !hasBit4D[4*i];
        firstBit4[4*i+2] <= firstBit16[i] && !hasBit4D[4*i] && !hasBit4D[4*i+1];
        firstBit4[4*i+3] <= firstBit16[i] && !hasBit4D[4*i] && !hasBit4D[4*i+1] && !hasBit4D[4*i+2];
    end
end
endgenerate
always @(posedge clk) isEmpty <= firstBit16[8];

endmodule

module newSeedProductionPipeline (
    input clk,
    
    input[127:0] graphIn_START,
    output[127:0] graphIn_HASBIT,
    input[127:0] extended_HASBIT,
    input shouldGrabNewSeed_HASBIT,
    
    output shouldIncrementConnectionCount,
    output reg[127:0] newCurExtendingOut_D
);

// PIPELINE STEP 1 and 2
wire[4*8-1:0] firstBit4_HASBIT;
wire isEmpty_HASBIT;
hasFirstBitAnalysis firstBitAnalysis(
    clk,
    graphIn_START,
    firstBit4_HASBIT,
    isEmpty_HASBIT
);

// delays
hyperpipe #(.CYCLES(`NEW_SEED_HASBIT_DEPTH), .WIDTH(128)) graphIn_START_TO_HASBIT_DELAY (clk, graphIn_START, graphIn_HASBIT);

wire[127:0] newSeed_HASBIT;
generate
// synthesizes to 1 ALM module a piece
for(genvar i = 0; i < 32; i = i + 1) begin
    assign newSeed_HASBIT[4*i]   = firstBit4_HASBIT[i] && graphIn_HASBIT[i*4];
    assign newSeed_HASBIT[4*i+1] = firstBit4_HASBIT[i] && graphIn_HASBIT[i*4+1]; // A is allowed even if X is active. Guaranteed single connected component
    assign newSeed_HASBIT[4*i+2] = firstBit4_HASBIT[i] && graphIn_HASBIT[i*4+2] && !graphIn_HASBIT[i*4+1]; // Only B requires that A is not active. 
    assign newSeed_HASBIT[4*i+3] = firstBit4_HASBIT[i] && graphIn_HASBIT[i*4+3]; // AB is allowed even if A or B or X are active. Guaranteed single connected component
end
endgenerate

always @(posedge clk) newCurExtendingOut_D <= shouldGrabNewSeed_HASBIT ? newSeed_HASBIT : extended_HASBIT;

// Now we can finally produce the resulting connectionCount
assign shouldIncrementConnectionCount = shouldGrabNewSeed_HASBIT && !isEmpty_HASBIT;

endmodule

module explorationPipeline(
    input clk,
    input[1:0] topChannel,
    
    input[127:0] leftoverGraph_PRE_DOWN,
    input[127:0] curExtendingIn,
    
    output reg[127:0] reducedGraphOut_DOWN,
    output reg[127:0] extendedOut_DOWN,
    output reg runEnd,
    output reg shouldGrabNewSeedOut_DD
);

// PIPELINE STEP 1, 2, 3
wire[127:0] monotonizedUp_PRE_MID; pipelinedMonotonizeUp #(0,0,1,0,0,1,0) mUp(clk, curExtendingIn, monotonizedUp_PRE_MID);

// Instead of this
//reg[127:0] midPoint_MID; always @(posedge clk) midPoint_MID <= monotonizedUp_PRE_MID & leftoverGraphIn_PRE_MID;
// Use top, that way we can reduce resource usage by moving leftoverGraphIn to a longer shift register
wire[127:0] top;
topReceiver receiver(clk, topChannel, top);
reg[127:0] midPoint_MID; always @(posedge clk) midPoint_MID <= monotonizedUp_PRE_MID & top;

// PIPELINE STEP 3, 4
wire[127:0] monotonizedDown_PRE_DOWN; pipelinedMonotonizeDown #(0,0,1,0,0,1,0) mDown(clk, midPoint_MID, monotonizedDown_PRE_DOWN);
reg[127:0] midPoint_POST_MID;
reg[127:0] midPoint_PRE_DOWN;
always @(posedge clk) begin
    midPoint_POST_MID <= midPoint_MID;
    midPoint_PRE_DOWN <= midPoint_POST_MID;
end

reg[127:0] midPoint_DOWN;
always @(posedge clk) begin
    midPoint_DOWN <= midPoint_PRE_DOWN;
    reducedGraphOut_DOWN <= leftoverGraph_PRE_DOWN & ~monotonizedDown_PRE_DOWN;
    extendedOut_DOWN <= leftoverGraph_PRE_DOWN & monotonizedDown_PRE_DOWN;
end

// PIPELINE STEP 5
reg[7:0] reducedGraphIsZeroIntermediates_SUMMARIZE;
reg[63:0] extentionFinishedIntermediates_SUMMARIZE;
reg[15:0] extentionFinishedIntermediates_SUMMARIZE_D;
reg[3:0] extentionFinishedIntermediates_SUMMARIZE_DD;

genvar i;
generate
    for(i = 0; i < 8; i = i + 1) begin always @(posedge clk) reducedGraphIsZeroIntermediates_SUMMARIZE[i] <= |reducedGraphOut_DOWN[16*i +: 16]; end
    for(i = 0; i < 64; i = i + 1) begin always @(posedge clk) extentionFinishedIntermediates_SUMMARIZE[i] <= (extendedOut_DOWN[2*i +: 2] == midPoint_DOWN[2*i +: 2]); end
    for(i = 0; i < 16; i = i + 1) begin always @(posedge clk) extentionFinishedIntermediates_SUMMARIZE_D[i] <= &extentionFinishedIntermediates_SUMMARIZE[4*i +: 4]; end
    for(i = 0; i < 4; i = i + 1) begin always @(posedge clk) extentionFinishedIntermediates_SUMMARIZE_DD[i] <= &extentionFinishedIntermediates_SUMMARIZE_D[4*i +: 4]; end
endgenerate
// PIPELINE STEP 6
always @(posedge clk) runEnd <= !(|reducedGraphIsZeroIntermediates_SUMMARIZE); // the new leftoverGraph is empty, request a new graph
(* dont_merge *) reg runEnd_D; always @(posedge clk) runEnd_D <= runEnd;

// a single OR gate, to define shouldGrabNewSeed. 
// VERY INTERESTING! This was also a horrible bug. The standard path is just shouldGrabNewSeed = extentionFinished. 
// But if the resulting left over graph is empty, then we must have reached the end of the exploration, so we can quit early!
// This saves a single cycle in rare cases, and it fixes the aforementioned horrible bug :P
always @(posedge clk) shouldGrabNewSeedOut_DD <= runEnd_D | &extentionFinishedIntermediates_SUMMARIZE_DD;

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
    input[127:0] reducedGraphIn,
    input shouldGrabNewSeedIn_HASBIT,
    input validIn_NSD,
    input[5:0] storedConnectionCountIn_NSD,
    input[EXTRA_DATA_WIDTH-1:0] storedExtraDataIn_NSD,
    
    output request_EXPL,
    output[127:0] extendedOut_DOWN,
    output[127:0] reducedGraphOut_DOWN,
    output shouldGrabNewSeedOut_EXPL_DD,
    output reg validOut_NSD_D, 
    output reg[5:0] connectionCountOut_NSD_D, 
    output reg[EXTRA_DATA_WIDTH-1:0] storedExtraDataOut_NSD_D,
    
    // output side
    output reg done,
    output reg[5:0] connectCountOut,
    output reg[EXTRA_DATA_WIDTH-1:0] extraDataOut,
    output reg eccStatus
);

// PIPELINE STEP 5
// PIPELINE STEP "NEW SEED PRODUCTION"
// Generation of new seed, find index and test if graph is 0 to increment connectCount

(* dont_merge *) reg rstD; always @(posedge clk) rstD <= rst;
(* dont_merge *) reg rstDD; always @(posedge clk) rstDD <= rstD;

reg[127:0] leftoverGraph_START; always @(posedge clk) leftoverGraph_START <= rstDD ? 128'b0 : runEndIn ? graphIn : reducedGraphIn;

wire shouldIncrementConnectionCount_NSD;
wire[127:0] curExtending_MID;
wire[127:0] leftoverGraph_HASBIT; // Use later leftoverGraph for easier timing on leftoverGraph_START multiplexer
newSeedProductionPipeline newSeedProductionPipe(clk, leftoverGraph_START, leftoverGraph_HASBIT, extendedIn_HASBIT, shouldGrabNewSeedIn_HASBIT, shouldIncrementConnectionCount_NSD, curExtending_MID);

wire eccStatusWire;
wire[127:0] leftoverGraphOut_PRE_DOWN;
always @(posedge clk) eccStatus <= eccStatusWire;
shiftRegister_M20K #(.CYCLES(`OFFSET_DOWN - 2 - `NEW_SEED_HASBIT_DEPTH), .WIDTH(128)) newSeedProductionPipeBypassLeftoverGraphWireDelay(clk,
    leftoverGraph_HASBIT,
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
// output wire shouldGrabNewSeedOut_EXPL_DD;
// output wire request_EXPL;
explorationPipeline explorationPipe(clk, topChannel, leftoverGraphOut_PRE_DOWN, curExtending_MID, reducedGraphOut_DOWN, extendedOut_DOWN, request_EXPL, shouldGrabNewSeedOut_EXPL_DD);

// Outputs

wire[EXTRA_DATA_WIDTH-1:0] extraDataIn_NSD;
wire graphInAvailable_NSD;

hyperpipe #(.CYCLES(`NEW_SEED_DEPTH), .WIDTH(EXTRA_DATA_WIDTH+1)) extraDataInGraphInAvailablePipe(clk,
    {extraDataIn, graphInAvailable},
    {extraDataIn_NSD, graphInAvailable_NSD}
);

always @(posedge clk) storedExtraDataOut_NSD_D <= runEndIn_NSD ? extraDataIn_NSD : storedExtraDataIn_NSD;
always @(posedge clk) validOut_NSD_D <= runEndIn_NSD ? graphInAvailable_NSD : validIn_NSD;

always @(posedge clk) begin
    done <= runEndIn_NSD && validIn_NSD;
    connectCountOut <= storedConnectionCountIn_NSD;
    extraDataOut <= storedExtraDataIn_NSD;
end

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
wire[127:0] reducedGraphIn;
wire shouldGrabNewSeedIn_HASBIT;
wire validIn_NSD;
wire[5:0] storedConnectionCountIn_NSD;
wire[EXTRA_DATA_WIDTH-1:0] storedExtraDataIn_NSD;

wire requestOut_EXPL;
wire[127:0] extendedOut_DOWN;
wire[127:0] reducedGraphOut_DOWN;
wire shouldGrabNewSeedOut_EXPL_DD;
wire validOut_NSD_D;
wire[5:0] connectionCountOut_NSD_D;
wire[EXTRA_DATA_WIDTH-1:0] storedExtraDataOut_NSD_D;

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
    reducedGraphIn,
    shouldGrabNewSeedIn_HASBIT,
    validIn_NSD,
    storedConnectionCountIn_NSD,
    storedExtraDataIn_NSD,
    
    requestOut_EXPL,
    extendedOut_DOWN,
    reducedGraphOut_DOWN,
    shouldGrabNewSeedOut_EXPL_DD,
    validOut_NSD_D,
    connectionCountOut_NSD_D,
    storedExtraDataOut_NSD_D,
    
    done,
    connectCountOut,
    extraDataOut,
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
hyperpipe #(.CYCLES(`TOTAL_PIPELINE_STAGES - `OFFSET_EXPL + DATA_IN_LATENCY)) loopBackRequestPipe(clk, requestOut_EXPL, runEndIn);
hyperpipe #(.CYCLES(`TOTAL_PIPELINE_STAGES - (`OFFSET_EXPL + 2) + DATA_IN_LATENCY + `NEW_SEED_HASBIT_OFFSET)) loopBackShouldGrabNewSeedPipe(clk, shouldGrabNewSeedOut_EXPL_DD, shouldGrabNewSeedIn_HASBIT);

hyperpipe #(.CYCLES(`TOTAL_PIPELINE_STAGES - `OFFSET_DOWN + DATA_IN_LATENCY), .WIDTH(128)) loopBackPipeReducedGraph(clk,
    reducedGraphOut_DOWN,
    reducedGraphIn
);

wire loopBackPipeValidAndExtraDataECCWire;
reg loopBackPipeValidAndExtraDataECC; always @(posedge clk) loopBackPipeValidAndExtraDataECC <= loopBackPipeValidAndExtraDataECCWire;
assign eccStatusWOR = loopBackPipeValidAndExtraDataECC;

shiftRegister_M20K #(.CYCLES(`TOTAL_PIPELINE_STAGES - 1 + DATA_IN_LATENCY), .WIDTH(1 + EXTRA_DATA_WIDTH + 6)) loopBackPipeValidAndExtraData (clk,
    {validOut_NSD_D, storedExtraDataOut_NSD_D, connectionCountOut_NSD_D},
    {validIn_NSD, storedExtraDataIn_NSD, storedConnectionCountIn_NSD},
    loopBackPipeValidAndExtraDataECCWire
);

assign isActive = validIn_NSD;

endmodule
