`timescale 1ns / 1ps

module floodFillStep(
    input[127:0] top,
    input[127:0] graphIn,
    input[127:0] exploredIn,
    output[127:0] graphOut,
    output[127:0] exploredOut,
    output noChange
);

wire[127:0] monotonizeUpOut;
monotonizeUp firstStep(exploredIn, monotonizeUpOut); 
// use top instead of graphIn as it is a false path and thus reduces timing strain. 
// WARNING, Will produce incorrect results if Leaf Elimination Up is used, because then the top of top does not match the top of graph. 
// Luckily only Leaf Elimination Down leaves improves performance, so this version is used. 
wire[127:0] midPoint = monotonizeUpOut & top;

wire[127:0] monotonizeDownOut;
monotonizeDown secondStep(midPoint, monotonizeDownOut);
assign exploredOut = graphIn & monotonizeDownOut;
assign graphOut = graphIn & ~monotonizeDownOut;

assign noChange = (midPoint == exploredOut);

endmodule


module countConnectedCore #(parameter EXTRA_DATA_WIDTH = 0) (
    input clk,
    input[127:0] top,
    input[127:0] graphIn,
    input graphInAvailable,
    input[5:0] connectCountIn,
    input[EXTRA_DATA_WIDTH-1:0] extraDataIn,
    output reg[EXTRA_DATA_WIDTH-1:0] extraDataOut,
    output reg[5:0] connectCount,
    output request,
    output done
); 

reg[127:0] graph;
reg[127:0] curExtending;
reg isNewSeed;

wire[127:0] newSeed;
firstBit firstBitOfGraph(graph, newSeed, request);
wire started = request & graphInAvailable;

wire[127:0] floodFillIn = isNewSeed ? newSeed : curExtending;
wire[127:0] floodFillOut;
wire[127:0] graphOut;
wire ffRunComplete;
floodFillStep ffStep(top, graph, floodFillIn, graphOut, floodFillOut, ffRunComplete);

initial graph = 0;
initial curExtending = 0;
initial isNewSeed = 0;

always @ (posedge clk) begin
    // no test needed for curExtending, just update even when not running, requires less hardware
    curExtending <= floodFillOut;
    // isNewSeed will be 1 by default when starting out. Therefore also no test needed! Very elegant!
    isNewSeed <= ffRunComplete;
end

always @ (posedge clk) begin
    // first clock, load registers
    if(started) begin
        graph <= graphIn;
        connectCount <= connectCountIn;
        extraDataOut <= extraDataIn;
    end
    
    // new component discovered, explore!
    if(!request & ffRunComplete) begin
        graph <= graphOut;//graph & ~floodFillOut; // remove current component to start new component
        connectCount <= connectCount + 1;
    end
end

reg prevRequest = 1;
reg prevGraphInAvailable = 0;
always @ (posedge clk) begin
    prevRequest <= request;
    prevGraphInAvailable <= graphInAvailable;
end
assign done = request & (!prevRequest | prevGraphInAvailable); // outputs a 1-clock pulse for every processed graph
endmodule
