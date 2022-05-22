`timescale 1ns / 1ps

`define NEW_SEED_HASBIT_DEPTH 3
`define NEW_SEED_HASBIT_OFFSET (1+`NEW_SEED_HASBIT_DEPTH)
`define NEW_SEED_DEPTH (`NEW_SEED_HASBIT_DEPTH+1)
`define EXPLORATION_DOWN_OFFSET 7
`define EXPLORATION_DEPTH 10

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
    input[127:0] extended_HASBIT,
    input shouldGrabNewSeed_HASBIT,
    
    output shouldIncrementConnectionCount_HASBIT,
    output reg[127:0] graphIn_NSD,
    output reg[127:0] newCurExtendingOut_NSD
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
wire[127:0] graphIn_HASBIT;
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

always @(posedge clk) begin
    graphIn_NSD <= graphIn_HASBIT;
    newCurExtendingOut_NSD <= shouldGrabNewSeed_HASBIT ? newSeed_HASBIT : extended_HASBIT;
end

// Now we can finally produce the resulting connectionCount
assign shouldIncrementConnectionCount_HASBIT = shouldGrabNewSeed_HASBIT && !isEmpty_HASBIT;

endmodule

module explorationPipeline(
    input clk,
    
    input[127:0] leftoverGraph,
    input[127:0] curExtendingIn,
    
    output reg[127:0] reducedGraphOut_DOWN,
    output reg[127:0] extendedOut_DOWN,
    output runEnd,
    output reg shouldGrabNewSeedOut_D
);

wire[127:0] leftoverGraph_A;
wire[127:0] leftoverGraph_B;
wire[127:0] leftoverGraph_C;
wire[127:0] leftoverGraph_PRE_DOWN;
hyperpipe #(.CYCLES(1), .WIDTH(128)) leftoverGraphPipeA (clk, leftoverGraph, leftoverGraph_A);
hyperpipe #(.CYCLES(2), .WIDTH(128)) leftoverGraphPipeB (clk, leftoverGraph_A, leftoverGraph_B);
hyperpipe #(.CYCLES(2), .WIDTH(128)) leftoverGraphPipeC (clk, leftoverGraph_B, leftoverGraph_C);
hyperpipe #(.CYCLES(2), .WIDTH(128)) leftoverGraphPipeD (clk, leftoverGraph_C, leftoverGraph_PRE_DOWN);

wire[127:0] monotonizedUp_A; pipelinedMonotonizeUp #(0,0,0,1,0,0,0) mUpA(clk, curExtendingIn, monotonizedUp_A); reg[127:0] mUp_A_R; always @(posedge clk) mUp_A_R <= monotonizedUp_A & leftoverGraph_A;
wire[127:0] monotonizedDown_B; pipelinedMonotonizeDown #(0,0,0,1,0,0,0) mDownB(clk, mUp_A_R, monotonizedDown_B); reg[127:0] mDown_B_R; always @(posedge clk) mDown_B_R <= monotonizedDown_B & leftoverGraph_B;
wire[127:0] monotonizedUp_C; pipelinedMonotonizeUp #(0,0,0,1,0,0,0) mUpC(clk, mDown_B_R, monotonizedUp_C); reg[127:0] midPoint_MID; always @(posedge clk) midPoint_MID <= monotonizedUp_C & leftoverGraph_C;
wire[127:0] monotonizedDown_PRE_DOWN; pipelinedMonotonizeDown #(0,0,0,1,0,0,0) mDownD(clk, midPoint_MID, monotonizedDown_PRE_DOWN);

// Instead of this
//reg[127:0] midPoint_MID; always @(posedge clk) midPoint_MID <= monotonizedUp_PRE_MID & leftoverGraphIn_PRE_MID;
// Use top, that way we can reduce resource usage by moving leftoverGraphIn to a longer shift register
//wire[127:0] top;
//topReceiver receiver(clk, topChannel, top);
//reg[127:0] midPoint_MID; always @(posedge clk) midPoint_MID <= monotonizedUp_PRE_MID & leftoverGraph_PRE_MID;

reg[127:0] midPoint_PRE_DOWN;
reg[127:0] midPoint_DOWN;
always @(posedge clk) begin
    midPoint_PRE_DOWN <= midPoint_MID;
    midPoint_DOWN <= midPoint_PRE_DOWN;
    reducedGraphOut_DOWN <= leftoverGraph_PRE_DOWN & ~monotonizedDown_PRE_DOWN;
    extendedOut_DOWN <= leftoverGraph_PRE_DOWN & monotonizedDown_PRE_DOWN;
end

// PIPELINE STEP 5
reg[31:0] reducedGraphIsZeroIntermediates_SUMMARIZE;
reg[7:0] reducedGraphIsZeroIntermediates_SUMMARIZE_D;
reg[63:0] extentionFinishedIntermediates_SUMMARIZE;
reg[15:0] extentionFinishedIntermediates_SUMMARIZE_D;
reg[3:0] extentionFinishedIntermediates_SUMMARIZE_DD;

genvar i;
generate
    for(i = 0; i < 32; i = i + 1) begin always @(posedge clk) reducedGraphIsZeroIntermediates_SUMMARIZE[i] <= |reducedGraphOut_DOWN[4*i +: 4]; end
    for(i = 0; i < 8; i = i + 1) begin always @(posedge clk) reducedGraphIsZeroIntermediates_SUMMARIZE_D[i] <= |reducedGraphIsZeroIntermediates_SUMMARIZE[4*i +: 4]; end
    for(i = 0; i < 64; i = i + 1) begin always @(posedge clk) extentionFinishedIntermediates_SUMMARIZE[i] <= (extendedOut_DOWN[2*i +: 2] == midPoint_DOWN[2*i +: 2]); end
    for(i = 0; i < 16; i = i + 1) begin always @(posedge clk) extentionFinishedIntermediates_SUMMARIZE_D[i] <= &extentionFinishedIntermediates_SUMMARIZE[4*i +: 4]; end
    for(i = 0; i < 4; i = i + 1) begin always @(posedge clk) extentionFinishedIntermediates_SUMMARIZE_DD[i] <= &extentionFinishedIntermediates_SUMMARIZE_D[4*i +: 4]; end
endgenerate
// PIPELINE STEP 6
reg reducedGraphIsZeroIntermediates_SUMMARIZE_DD_0; always @(posedge clk) reducedGraphIsZeroIntermediates_SUMMARIZE_DD_0 <= |reducedGraphIsZeroIntermediates_SUMMARIZE_D[3:0];
reg reducedGraphIsZeroIntermediates_SUMMARIZE_DD_1; always @(posedge clk) reducedGraphIsZeroIntermediates_SUMMARIZE_DD_1 <= |reducedGraphIsZeroIntermediates_SUMMARIZE_D[7:4];

assign runEnd = !(reducedGraphIsZeroIntermediates_SUMMARIZE_DD_0 || reducedGraphIsZeroIntermediates_SUMMARIZE_DD_1); // the new leftoverGraph is empty, request a new graph

// a single OR gate, to define shouldGrabNewSeed. 
// VERY INTERESTING! This was also a horrible bug. The standard path is just shouldGrabNewSeed = extentionFinished. 
// But if the resulting left over graph is empty, then we must have reached the end of the exploration, so we can quit early!
// This saves a single cycle in rare cases, and it fixes the aforementioned horrible bug :P
always @(posedge clk) shouldGrabNewSeedOut_D <= runEnd | &extentionFinishedIntermediates_SUMMARIZE_DD;

endmodule

// The combinatorial pipeline that does all the work. Loopback is done outside of this module through combinatorialStateIn/Out
// Pipeline stages are marked by wire_STAGE for clarity
// If graphInValid == 0, then graphIn must == 128'b0
module pipelinedCountConnectedCombinatorial #(parameter EXTRA_DATA_WIDTH = 10) (
    input clk,
    
    // input side
    input[127:0] graphIn,
    input graphInValid,
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
    output shouldGrabNewSeedOut_EXPL_D,
    output[127:0] extendedOut_DOWN,
    output[127:0] reducedGraphOut_DOWN,
    output reg validOut_NSD_D, 
    output reg[5:0] connectionCountOut_NSD_D, 
    output reg[EXTRA_DATA_WIDTH-1:0] storedExtraDataOut_NSD_D,
    
    // output side
    output reg done,
    output reg[5:0] connectCountOut,
    output reg[EXTRA_DATA_WIDTH-1:0] extraDataOut
);

// PIPELINE STEP 5
// PIPELINE STEP "NEW SEED PRODUCTION"
// Generation of new seed, find index and test if graph is 0 to increment connectCount

reg[127:0] leftoverGraph_START; always @(posedge clk) leftoverGraph_START <= runEndIn ? graphIn : reducedGraphIn;

wire shouldIncrementConnectionCount_NSD;
wire[127:0] curExtending_MID;
wire[127:0] leftoverGraph_MID; // Use later leftoverGraph for easier timing on leftoverGraph_START multiplexer
newSeedProductionPipeline newSeedProductionPipe(clk, leftoverGraph_START, extendedIn_HASBIT, shouldGrabNewSeedIn_HASBIT, shouldIncrementConnectionCount_NSD, leftoverGraph_MID, curExtending_MID);

wire runEndIn_NSD;
hyperpipe #(.CYCLES(`OFFSET_NSD), .WIDTH(1)) runEndInPipe(clk,
    runEndIn,
    runEndIn_NSD
);

always @(posedge clk) connectionCountOut_NSD_D <= (runEndIn_NSD ? 0 : storedConnectionCountIn_NSD) + shouldIncrementConnectionCount_NSD;


// PIPELINE STEP "EXPLORATION"
// output wire[127:0] selectedLeftoverGraphOut_EXPL;
// output wire[127:0] extendedOut_DOWN;
// output wire shouldGrabNewSeedOut_EXPL_D;
// output wire request_EXPL;
explorationPipeline explorationPipe(clk, leftoverGraph_MID, curExtending_MID, reducedGraphOut_DOWN, extendedOut_DOWN, request_EXPL, shouldGrabNewSeedOut_EXPL_D);
// Outputs

wire[EXTRA_DATA_WIDTH-1:0] extraDataIn_NSD;
wire graphInValid_NSD;

hyperpipe #(.CYCLES(`NEW_SEED_DEPTH), .WIDTH(EXTRA_DATA_WIDTH+1)) extraDataInGraphInValidPipe(clk,
    {extraDataIn, graphInValid},
    {extraDataIn_NSD, graphInValid_NSD}
);

always @(posedge clk) storedExtraDataOut_NSD_D <= runEndIn_NSD ? extraDataIn_NSD : storedExtraDataIn_NSD;
always @(posedge clk) validOut_NSD_D <= runEndIn_NSD ? graphInValid_NSD : validIn_NSD;

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
    output isActive, // Instrumentation wire for profiling
    
    // input side
    output request,
    input[127:0] graphIn,
    input graphInValid,
    input[EXTRA_DATA_WIDTH-1:0] extraDataIn,
    
    // output side
    output done,
    output[5:0] connectCountOut,
    output[EXTRA_DATA_WIDTH-1:0] extraDataOut,
    output reg eccStatus
);

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
wire shouldGrabNewSeedOut_EXPL_D;
wire validOut_NSD_D;
wire[5:0] connectionCountOut_NSD_D;
wire[EXTRA_DATA_WIDTH-1:0] storedExtraDataOut_NSD_D;

assign request = requestOut_EXPL;

pipelinedCountConnectedCombinatorial #(EXTRA_DATA_WIDTH) combinatorialComponent (
    clk,
    
    // input side
    graphIn,
    graphInValid,
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
    shouldGrabNewSeedOut_EXPL_D,
    extendedOut_DOWN,
    reducedGraphOut_DOWN,
    validOut_NSD_D,
    connectionCountOut_NSD_D,
    storedExtraDataOut_NSD_D,
    
    done,
    connectCountOut,
    extraDataOut
);

wire extraDataECCWire;
wire extendedECCWire;
wire reducedGraphECCWire;
reg extraDataECC; always @(posedge clk) extraDataECC <= extraDataECCWire;
reg extendedECC; always @(posedge clk) extendedECC <= extendedECCWire;
reg reducedGraphECC; always @(posedge clk) reducedGraphECC <= reducedGraphECCWire;

always @(posedge clk) eccStatus <= extraDataECC || extendedECC || reducedGraphECC;

shiftRegister_M20K #(.CYCLES(`TOTAL_PIPELINE_STAGES - `OFFSET_DOWN + DATA_IN_LATENCY + `NEW_SEED_HASBIT_OFFSET), .WIDTH(128)) loopBackPipeExtended (clk,
    extendedOut_DOWN,
    extendedIn_HASBIT,
    extendedECCWire
);

// delays
(* dont_merge *) reg rstD; always @(posedge clk) rstD <= rst;
hyperpipe #(.CYCLES(`TOTAL_PIPELINE_STAGES - `OFFSET_EXPL + DATA_IN_LATENCY)) loopBackRequestPipe(clk, requestOut_EXPL || rstD, runEndIn);
hyperpipe #(.CYCLES(`TOTAL_PIPELINE_STAGES - (`OFFSET_EXPL + 1) + DATA_IN_LATENCY + `NEW_SEED_HASBIT_OFFSET)) loopBackShouldGrabNewSeedPipe(clk, shouldGrabNewSeedOut_EXPL_D, shouldGrabNewSeedIn_HASBIT);

shiftRegister_M20K #(.CYCLES(`TOTAL_PIPELINE_STAGES - `OFFSET_DOWN + DATA_IN_LATENCY), .WIDTH(128)) loopBackPipeReducedGraph(clk,
    reducedGraphOut_DOWN,
    reducedGraphIn,
    reducedGraphECCWire
);

shiftRegister_M20K #(.CYCLES(`TOTAL_PIPELINE_STAGES - 1 + DATA_IN_LATENCY), .WIDTH(1 + EXTRA_DATA_WIDTH + 6)) loopBackPipeValidAndExtraData (clk,
    {validOut_NSD_D, storedExtraDataOut_NSD_D, connectionCountOut_NSD_D},
    {validIn_NSD, storedExtraDataIn_NSD, storedConnectionCountIn_NSD},
    extraDataECCWire
);

assign isActive = validIn_NSD;

endmodule
