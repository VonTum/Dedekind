`timescale 1ns / 1ps

// synthesizes to 2 ALM modules
module demuxElement (
    input graphIn0,
    input graphIn2,
    
    input lowerHalfIsFirstBit,
    input upperHalfIsFirstBit,
    input shouldGrabNewSeed,
    
    input[3:0] extendedIn,
    output[3:0] outputBits
);

wire[3:0] newSeed;
assign newSeed[1:0] = lowerHalfIsFirstBit ? (graphIn0 ? 2'b01 : 2'b10) : 2'b00;
assign newSeed[3:2] = upperHalfIsFirstBit ? (graphIn2 ? 2'b01 : 2'b10) : 2'b00;

assign outputBits = shouldGrabNewSeed ? newSeed : extendedIn;

endmodule

module newSeedProducer (
    input[127:0] graphIn,
    input[127:0] extendedIn,
    input shouldGrabNewSeedIn,
    output isZero,
    output[127:0] newExtendingOut
);

wire[31:0] hasBit4;
wire[7:0] hasBit16;
wire[1:0] hasBit64;
genvar i;
generate
for(i = 0; i < 32; i = i + 1) assign hasBit4[i] = (graphIn[i*4+:4] != 4'b0000);
for(i = 0; i < 8; i = i + 1) assign hasBit16[i] = |hasBit4[i*4+:4];
for(i = 0; i < 2; i = i + 1) assign hasBit64[i] = |hasBit16[i*4+:4];
endgenerate
assign isZero = !(|hasBit64);

wire[7:0] hasBitUpTo16;
assign hasBitUpTo16[0] = 0;
assign hasBitUpTo16[1] = hasBit16[0];
assign hasBitUpTo16[2] = hasBit16[0] | hasBit16[1];
assign hasBitUpTo16[3] = hasBit16[0] | hasBit16[1] | hasBit16[2];
assign hasBitUpTo16[4] = hasBit64[0];
assign hasBitUpTo16[5] = hasBit64[0] | hasBit16[4];
assign hasBitUpTo16[6] = hasBit64[0] | hasBit16[4] | hasBit16[5];
assign hasBitUpTo16[7] = hasBit64[0] | hasBit16[4] | hasBit16[5] | hasBit16[6];

wire[31:0] hasBitUpTo4;
generate
for(i = 0; i < 8; i = i + 1) begin
    assign hasBitUpTo4[i*4+0] = hasBitUpTo16[i];
    assign hasBitUpTo4[i*4+1] = hasBitUpTo16[i] | hasBit4[i*4];
    assign hasBitUpTo4[i*4+2] = hasBitUpTo16[i] | hasBit4[i*4] | hasBit4[i*4+1];
    assign hasBitUpTo4[i*4+3] = hasBitUpTo16[i] | hasBit4[i*4] | hasBit4[i*4+1] | hasBit4[i*4+2];
end
endgenerate

wire[31:0] isFirstBit4 = hasBit4 & ~hasBitUpTo4;

wire[63:0] isFirstBit2;
generate
for(i = 0; i < 32; i = i + 1) begin
    assign isFirstBit2[i*2+0] = isFirstBit4[i] & (graphIn[i*4+0] | graphIn[i*4+1]);
    assign isFirstBit2[i*2+1] = isFirstBit4[i] & !(graphIn[i*4+0] | graphIn[i*4+1]);
end
endgenerate

generate
for(i = 0; i < 32; i = i + 1) begin
    demuxElement demux(
        .graphIn0(graphIn[i*4+0]), 
        .graphIn2(graphIn[i*4+2]),
        .lowerHalfIsFirstBit(isFirstBit2[i*2]),
        .upperHalfIsFirstBit(isFirstBit2[i*2+1]),
        .shouldGrabNewSeed(shouldGrabNewSeedIn),
        .extendedIn(extendedIn[i*4+:4]),
        .outputBits(newExtendingOut[i*4+:4])
    );
end
endgenerate

endmodule

module pipelinedCountConnectedCore #(parameter EXTRA_DATA_WIDTH = 10) (
	 input clk,
	 input[127:0] top, // top wire remains constant for the duration of a run, no need to factor it into pipeline
	 
	 // input side
	 output request,
	 input[127:0] graphIn,
	 input graphInAvailable,
	 input[5:0] connectCountIn,
	 input[EXTRA_DATA_WIDTH-1:0] extraDataIn,
	 
	 // output side
	 output done,
	 output[5:0] connectCount,
	 output[EXTRA_DATA_WIDTH-1:0] extraDataOut
);

reg[127:0] leftoverGraph = 0;
reg[127:0] curExtending = 0;
reg[5:0] connectionCount;
reg valid = 0;
reg[EXTRA_DATA_WIDTH-1:0] extraData;

// PIPELINE STEP 1
// start with floodfill of the previous curExtending
wire[127:0] monotonizedUp; monotonizeUp mUp(curExtending, monotonizedUp);
wire[127:0] midPoint = monotonizedUp & top;

// PIPELINE STEP 2
// Floodfill monotonizeDown step
wire[127:0] monotonizedDown; monotonizeDown mDown(midPoint, monotonizedDown);
wire[127:0] extended = leftoverGraph & monotonizedDown;
wire[127:0] reducedGraph = leftoverGraph & ~monotonizedDown;

// PIPELINE STEP 3
wire extentionFinished = (extended == midPoint); // no change? Then grab the next seed to extend
wire runEnd = (reducedGraph == 0); // the new leftoverGraph is empty, request a new graph
assign request = runEnd;

// PIPELINE STEP 4
// Produce outputs from this run if runEnd
assign done = valid & runEnd;
assign extraDataOut = extraData;
assign connectCount = connectionCount;

// graphInAvailable Input becomes available
wire start = runEnd & graphInAvailable;

// a single OR gate, to define shouldGrabNewSeed. 
// VERY INTERESTING! This was also a horrible bug. The standard path is just shouldGrabNewSeed = extentionFinished. 
// But if the resulting left over graph is empty, then we must have reached the end of the exploration, so we can quit early!
// This saves a single cycle in rare cases, and it fixes the aforementioned horrible bug :P
wire shouldGrabNewSeed = extentionFinished | runEnd;

// PIPELINE STEP 5
// Other Inputs become available
wire[127:0] finalLeftoverGraph = start ? graphIn : (shouldGrabNewSeed ? reducedGraph : leftoverGraph); // synthesizes to 128 5-in 1-out LUTs
wire[EXTRA_DATA_WIDTH-1:0] finalExtraData = runEnd ? extraDataIn : extraData;
wire finalValid = (valid & !runEnd) | start;
wire[5:0] selectedConnectCount = start ? connectCountIn : connectionCount;

// PIPELINE STEP 6
// Start generation of new seed, find index and test if graph is 0
wire finalLeftoverGraphIsZero;
wire[127:0] finalCurExtending;
newSeedProducer newSeedProducer(
    .graphIn(finalLeftoverGraph),
    .isZero(finalLeftoverGraphIsZero),
    .shouldGrabNewSeedIn(shouldGrabNewSeed),
    .extendedIn(extended),
    .newExtendingOut(finalCurExtending)
);

// Now we can finally produce the resulting connectionCount
wire shouldIncrementConnectionCount = shouldGrabNewSeed & !finalLeftoverGraphIsZero;

wire[5:0] finalConnectionCount = selectedConnectCount + shouldIncrementConnectionCount;

// PIPELINE LOOPBACK
always @(posedge clk) begin
    leftoverGraph <= finalLeftoverGraph;
    curExtending <= finalCurExtending;
    connectionCount <= finalConnectionCount;
    valid <= finalValid;
    extraData <= finalExtraData;
end
endmodule
