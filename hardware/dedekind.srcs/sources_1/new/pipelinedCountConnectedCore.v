`timescale 1ns / 1ps

module pipelinedCountConnectedCore #(parameter EXTRA_DATA_WIDTH = 10) (
	 input clk,
	 input[127:0] top, // top wire remains constant for the duration of a run, no need to factor it into pipeline
	 
	 // input side
	 output request,
	 input[127:0] graphIn,
	 input graphInAvailable,
	 input graphInIsZero,
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
wire[127:0] selectedGraph = shouldGrabNewSeed ? reducedGraph : leftoverGraph;
wire[127:0] finalLeftoverGraph = start ? graphIn : selectedGraph; // synthesizes this and the above to 128 5-in 1-out LUTs
wire[EXTRA_DATA_WIDTH-1:0] finalExtraData = runEnd ? extraDataIn : extraData;
wire finalValid = (valid & !runEnd) | start;
wire[5:0] selectedConnectCount = start ? connectCountIn : connectionCount;

// PIPELINE STEP 6
// Start generation of new seed, find index and test if graph is 0
wire[6:0] newSeedIndex;
wire newSeedIsZero;
bitScanForward128 firstBitIndex(finalLeftoverGraph, newSeedIndex, newSeedIsZero);

// PIPELINE STEP 7
// Produce new curExtending
// This should synthesize to one 128-bit multiplexer, or 64+16 alm modules. Because it can use the synclr input on alm modules
wire[127:0] newSeed = 1 << newSeedIndex;
// The newSeedIsZero set to zero can be done here instead of modifying newSeed since the graph won't ever become zero when not inputted as zero
wire[127:0] finalCurExtending = newSeedIsZero ? 0 : (shouldGrabNewSeed ? newSeed : extended);

// Now we can finally produce the resulting connectionCount
wire shouldIncrementConnectionCount = shouldGrabNewSeed & !newSeedIsZero;

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
