`timescale 1ns / 1ps

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
genvar j;
generate
for(i = 0; i < 32; i = i + 1) assign hasBit4[i] = |graphIn[i*4+:4];
for(i = 0; i < 8; i = i + 1) assign hasBit16[i] = |hasBit4[i*4+:4];
for(i = 0; i < 2; i = i + 1) assign hasBit64[i] = |hasBit16[i*4+:4];
endgenerate
assign isZero = !(|hasBit64);

wire[3:0] hasFirstBit4[7:0];
generate
for(i = 0; i < 8; i = i + 1) begin
    assign hasFirstBit4[i][0] = hasBit4[4*i];
    assign hasFirstBit4[i][1] = !hasBit4[4*i] & hasBit4[4*i+1];
    assign hasFirstBit4[i][2] = !hasBit4[4*i] & !hasBit4[4*i+1] & hasBit4[4*i+2];
    assign hasFirstBit4[i][3] = !hasBit4[4*i] & !hasBit4[4*i+1] & !hasBit4[4*i+2] & hasBit4[4*i+3];
end
endgenerate
wire[3:0] hasFirstBit16[1:0];
generate
for(i = 0; i < 2; i = i + 1) begin
    assign hasFirstBit16[i][0] =  hasBit16[4*i];
    assign hasFirstBit16[i][1] = !hasBit16[4*i] &  hasBit16[4*i+1];
    assign hasFirstBit16[i][2] = !hasBit16[4*i] & !hasBit16[4*i+1] &  hasBit16[4*i+2];
    assign hasFirstBit16[i][3] = !hasBit16[4*i] & !hasBit16[4*i+1] & !hasBit16[4*i+2] & hasBit16[4*i+3];
end
endgenerate

wire[63:0] isFirstBit2;
generate
for(i = 0; i < 4; i = i + 1) begin
    for(j = 0; j < 4; j = j + 1) begin
        assign isFirstBit2[i*8+j*2+0] = (|graphIn[16*i+4*j +: 2]) & hasFirstBit4[i][j] & hasFirstBit16[0][i];
        assign isFirstBit2[i*8+j*2+1] = !(|graphIn[16*i+4*j +: 2]) & hasFirstBit4[i][j] & hasFirstBit16[0][i];
    end
end
for(i = 0; i < 4; i = i + 1) begin
    for(j = 0; j < 4; j = j + 1) begin
        assign isFirstBit2[32+i*8+j*2+0] = (|graphIn[64+16*i+4*j +: 2]) & hasFirstBit4[4+i][j] & hasFirstBit16[1][i] & !hasBit64[0];
        assign isFirstBit2[32+i*8+j*2+1] = !(|graphIn[64+16*i+4*j +: 2]) & hasFirstBit4[4+i][j] & hasFirstBit16[1][i] & !hasBit64[0];
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

module pipelinedCountConnectedCore #(parameter EXTRA_DATA_WIDTH = 10) (
	 input clk,
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

// a single OR gate, to define shouldGrabNewSeed. 
// VERY INTERESTING! This was also a horrible bug. The standard path is just shouldGrabNewSeed = extentionFinished. 
// But if the resulting left over graph is empty, then we must have reached the end of the exploration, so we can quit early!
// This saves a single cycle in rare cases, and it fixes the aforementioned horrible bug :P
wire shouldGrabNewSeed = extentionFinished | runEnd;

// PIPELINE STEP 5
// Inputs become available
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
