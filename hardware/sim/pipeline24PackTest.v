`timescale 1ns / 1ps
`include "../src/pipelineGlobals_header.v"


module pipeline24PackTest();

reg clk;
initial begin
    clk = 0;
    forever #1 clk = ~clk;
end
reg rst;
initial begin
    rst = 1;
    #50 rst = 0;
end

parameter MEMSIZE = 16384;

wire[$clog2(MEMSIZE)-1:0] index;
wire[127:0] top, bot;
wire isBotValid;
wire[4:0] packFullness;
wire[39:0] summedData;
wire[4:0] pcoeffCount;

pipeline24Pack elementUnderTest (
    .clk(clk),
    .rst(rst),
    .top(top),
    .bot(bot),
    .botIndex(index[`ADDR_WIDTH-1:0]),
    .isBotValid(isBotValid),
    .maxFullness(packFullness),
    .summedData(summedData),
    .pcoeffCount(pcoeffCount)
);

indexProvider #(MEMSIZE) dataProvider (
    .clk(clk),
    .rst(rst),
    .index(index),
    .requestData(packFullness < 30),
    .dataAvailable(isBotValid)
);

reg[128*2+64+8-1:0] dataTable[MEMSIZE-1:0];
initial $readmemb("pipeline24PackTestSet7.mem", dataTable);

assign {top, bot} = dataTable[index][128*2+64+8-1 : 64+8];

localparam OUTPUT_LAG = (1 << `ADDR_WIDTH) - `OUTPUT_INDEX_OFFSET + `OUTPUT_READ_LATENCY;

wire[39:0] offsetSum = dataTable[index-OUTPUT_LAG][37+8-1 : 8];
wire[4:0] offsetCount = dataTable[index-OUTPUT_LAG][2 : 0];

wire CORRECT_SUM = summedData == offsetSum;
wire CORRECT_COUNT = pcoeffCount == offsetCount;

endmodule
