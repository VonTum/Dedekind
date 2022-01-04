`timescale 1ns / 1ps

module hasFirstBitAnalysis(
    input[127:0] graphIn,
    output[1:0] hasBit64,
    // grouped in sets of 4, an element is marked '1' if its 4 bits contain the first bit of the 16 bits of the group
    output[4*8-1:0] hasFirstBit4,
    // grouped in sets of 4, an element is marked '1' if its 16 bits contain the first bit of the 64 bits of the group
    output[4*2-1:0] hasFirstBit16
);

wire[31:0] hasBit4;
wire[7:0] hasBit16;
genvar i;
generate
for(i = 0; i < 32; i = i + 1) begin assign hasBit4[i] = |graphIn[i*4+:4]; end
for(i = 0; i < 8; i = i + 1) begin assign hasBit16[i] = |hasBit4[i*4+:4]; end
for(i = 0; i < 2; i = i + 1) begin assign hasBit64[i] = |hasBit16[i*4+:4]; end
endgenerate

generate
for(i = 0; i < 8; i = i + 1) begin
    assign hasFirstBit4[4*i+0] = hasBit4[4*i];
    assign hasFirstBit4[4*i+1] = !hasBit4[4*i] & hasBit4[4*i+1];
    assign hasFirstBit4[4*i+2] = !hasBit4[4*i] & !hasBit4[4*i+1] & hasBit4[4*i+2];
    assign hasFirstBit4[4*i+3] = !hasBit4[4*i] & !hasBit4[4*i+1] & !hasBit4[4*i+2] & hasBit4[4*i+3];
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
    input[127:0] graphIn,
    input[127:0] extended,
    input shouldGrabNewSeed,
    
    output shouldIncrementConnectionCount,
    output[127:0] newCurExtendingOut
);

// PIPELINE STEP 1 and 2
wire[1:0] hasBit64;
wire[4*8-1:0] hasFirstBit4;
wire[4*2-1:0] hasFirstBit16;
hasFirstBitAnalysis firstBitAnalysis(
    graphIn,
    hasBit64,
    hasFirstBit4,
    hasFirstBit16
);

// PIPELINE STEP 3
newExtendingProducer resultProducer(
    graphIn,
    extended,
    shouldGrabNewSeed,
    hasBit64,
    hasFirstBit4,
    hasFirstBit16,
    newCurExtendingOut
);

// Now we can finally produce the resulting connectionCount
assign shouldIncrementConnectionCount = shouldGrabNewSeed & (hasBit64[0] | hasBit64[1]);

endmodule

module explorationPipeline(
    input[127:0] top,
    
    input[127:0] leftoverGraphInDelayed,
    input[127:0] curExtendingIn,
    
    output[127:0] reducedGraphOut,
    output[127:0] extendedOut,
    output runEnd,
    output shouldGrabNewSeedOut
);

// PIPELINE STEP 1, 2
wire[127:0] monotonizedUp; monotonizeUp mUp(curExtendingIn, monotonizedUp);
wire[127:0] midPoint = monotonizedUp & top;

// PIPELINE STEP 3, 4
wire[127:0] monotonizedDown; monotonizeDown mDown(midPoint, monotonizedDown);
assign extendedOut = leftoverGraphInDelayed & monotonizedDown;
assign reducedGraphOut = leftoverGraphInDelayed & ~monotonizedDown;

// PIPELINE STEP 5
wire[31:0] reducedGraphIsZeroIntermediates;
wire[63:0] extentionFinishedIntermediates;

genvar i;
generate
    for(i = 0; i < 32; i = i + 1) begin assign reducedGraphIsZeroIntermediates[i] = |reducedGraphOut[4*i +: 4]; end
    for(i = 0; i < 64; i = i + 1) begin assign extentionFinishedIntermediates[i] = (extendedOut[2*i +: 2] == midPoint[2*i +: 2]); end
endgenerate

// PIPELINE STEP 6
assign runEnd = !(|reducedGraphIsZeroIntermediates); // the new leftoverGraph is empty, request a new graph
// split
wire[3:0] extentionFinished; // no change? Then grab the next seed to extend

generate
for(i = 0; i < 4; i = i + 1) begin assign extentionFinished[i] = &extentionFinishedIntermediates[16*i +: 16]; end
endgenerate

// a single OR gate, to define shouldGrabNewSeed. 
// VERY INTERESTING! This was also a horrible bug. The standard path is just shouldGrabNewSeed = extentionFinished. 
// But if the resulting left over graph is empty, then we must have reached the end of the exploration, so we can quit early!
// This saves a single cycle in rare cases, and it fixes the aforementioned horrible bug :P
assign shouldGrabNewSeedOut = runEnd | &extentionFinished;

endmodule

// requires a reset signal of at least 2*PIPELINE_DEPTH cycles, or more!
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

localparam PIPELINE_DEPTH = 10;

wire[127:0] leftoverGraph;
wire[127:0] curExtending;
wire[5:0] connectionCount;
wire valid;
wire[EXTRA_DATA_WIDTH-1:0] extraData;


wire[127:0] reducedGraphPreDelay;
wire[127:0] extendedPreDelay;
wire shouldGrabNewSeedPreDelay;

wire requestPreDelay;
// PIPELINE STEP 1
explorationPipeline explorationPipe(/*top*/ leftoverGraph, leftoverGraph, curExtending, reducedGraphPreDelay, extendedPreDelay, requestPreDelay, shouldGrabNewSeedPreDelay);

// PIPELINE STEP 4
// Produce outputs from this run if runEnd
hyperpipe #(.CYCLES(PIPELINE_DEPTH - DATA_IN_LATENCY), .WIDTH(1+1+EXTRA_DATA_WIDTH+6)) outputsPipe(clk,
    {requestPreDelay, valid & requestPreDelay, extraData,    connectionCount},
    {request,         done,                    extraDataOut, connectCount}
);

// delay other wires for DATA_IN_LATENCY
wire[127:0] reducedGraph;
wire[127:0] extended;
wire shouldGrabNewSeed;
wire[127:0] leftoverGraphForSelection;
wire validPostDelay;
wire runEnd;
wire[5:0] connectionCountPostDelay;
wire[EXTRA_DATA_WIDTH-1:0] extraDataPostDelay;

hyperpipe #(.CYCLES(PIPELINE_DEPTH), .WIDTH(128+128+128+1+1+1+6+EXTRA_DATA_WIDTH)) inputLatencyPipe(clk,
    {leftoverGraph,             reducedGraphPreDelay, extendedPreDelay, requestPreDelay, shouldGrabNewSeedPreDelay, valid,          connectionCount,          extraData},
    {leftoverGraphForSelection, reducedGraph,         extended,         runEnd,          shouldGrabNewSeed,         validPostDelay, connectionCountPostDelay, extraDataPostDelay}
);

// PIPELINE STEP 5
// Inputs become available
assign leftoverGraph = rst ? 0 : start ? graphIn : (shouldGrabNewSeed ? reducedGraph : leftoverGraphForSelection);

assign extraData = runEnd ? extraDataIn : extraDataPostDelay;
assign valid = start ? 1 : (runEnd ? 0 : validPostDelay); 

// PIPELINE STEP 6
// Generation of new seed, find index and test if graph is 0 to increment connectCount

wire shouldIncrementConnectionCount;
newSeedProductionPipeline newSeedProductionPipe (leftoverGraph, extended, shouldGrabNewSeed, shouldIncrementConnectionCount, curExtending);

wire[5:0] selectedConnectCount = start ? connectCountIn : connectionCountPostDelay;
assign connectionCount = selectedConnectCount + shouldIncrementConnectionCount;

endmodule
