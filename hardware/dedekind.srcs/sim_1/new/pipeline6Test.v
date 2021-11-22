`timescale 1ns / 1ps

`include "pipelineGlobals.vh"

module pipeline6Test();

reg clk;
initial begin
    clk = 0;
    forever #1 clk = ~clk;
end
reg rst;
initial begin
    rst = 1;
    #200 rst = 0;
end

parameter MEMSIZE = 16384;

wire[$clog2(MEMSIZE)-1:0] index;
wire[127:0] top, bot;
wire isBotValid;
wire[4:0] fifoFullness;
wire[37:0] summedData;
wire[2:0] pcoeffCount;
wire[5:0] validBotPermutations; // == {vABC, vACB, vBAC, vBCA, vCAB, vCBA}

permuteCheck6 permutChecker (
    .top(top),
    .bot(bot),
    .isBotValid(isBotValid),
    .validBotPermutations(validBotPermutations)
);

fullPipeline elementUnderTest (
    .clk(clk),
    .rst(rst),
    .top(top),
    
    .bot(bot),
    .botIndex(index[`ADDR_WIDTH-1:0]),
    .isBotValid(isBotValid),
    .validBotPermutations(validBotPermutations),
    .fifoFullness(fifoFullness),
    .summedDataOut(summedData),
    .pcoeffCountOut(pcoeffCount)
);

indexProvider #(MEMSIZE) dataProvider (
    .clk(clk),
    .rst(rst),
    .index(index),
    .requestData(fifoFullness <= 30),
    .dataAvailable(isBotValid)
);

reg[128*2+64+8-1:0] dataTable[MEMSIZE-1:0];
initial $readmemb("pipeline6PackTestSet7.mem", dataTable);
wire[37:0] offsetSum;
wire[2:0] offsetCount;
assign {top, bot} = dataTable[index][128*2+64+8-1 : 64+8];

localparam OUTPUT_LAG = (1 << `ADDR_WIDTH) - `OUTPUT_INDEX_OFFSET + `OUTPUT_READ_LATENCY;

assign offsetSum = dataTable[index-OUTPUT_LAG][37+8-1 : 8];
assign offsetCount = dataTable[index-OUTPUT_LAG][2 : 0];

wire CORRECT_SUM = summedData == offsetSum;
wire CORRECT_COUNT = pcoeffCount == offsetCount;

endmodule
