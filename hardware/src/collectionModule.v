`timescale 1ns / 1ps
`include "pipelineGlobals_header.v"

module collectorBlock(
    input clk,
    
    // input side
    input writeEnable,
    input[9:0] writeAddr,
    input[5:0] subAddrMask,
    input[9:0] writeData,
    
    // output side
    input readEnable, // this reads from the memory and also wipes the read entry. 
    input[9:0] readAddr,
    output[59:0] readData
);

// one cycle delay for writes so we don't read-and-write at the same time
reg[9:0] blockWriteData;
reg[5:0] writeMasks;
reg blockWrite;
reg[9:0] blockWriteAddr;
always @(posedge clk) begin
    blockWriteData <= readEnable ? 10'b0000000000 : writeData;
    writeMasks <= readEnable ? 6'b111111 : subAddrMask;
    blockWrite <= writeEnable | readEnable;
    blockWriteAddr <= readEnable ? readAddr : writeAddr;
end

simpleDualPortM20K_20b1024Registered memBlock01 (
    .clk(clk),
    
    .writeEnable(blockWrite),
    .writeAddr(blockWriteAddr),
    .writeMask(writeMasks[1:0]),
    .writeData({blockWriteData,blockWriteData}),
    
    .readEnable(readEnable),
    .readAddr(readAddr),
    .readData(readData[19:00])
);
simpleDualPortM20K_20b1024Registered memBlock23 (
    .clk(clk),
    
    .writeEnable(blockWrite),
    .writeAddr(blockWriteAddr),
    .writeMask(writeMasks[3:2]),
    .writeData({blockWriteData,blockWriteData}),
    
    .readEnable(readEnable),
    .readAddr(readAddr),
    .readData(readData[39:20])
);
simpleDualPortM20K_20b1024Registered memBlock45 (
    .clk(clk),
    
    .writeEnable(blockWrite),
    .writeAddr(blockWriteAddr),
    .writeMask(writeMasks[5:4]),
    .writeData({blockWriteData,blockWriteData}),
    
    .readEnable(readEnable),
    .readAddr(readAddr),
    .readData(readData[59:40])
);

endmodule

module collectionModule(
    input clk,
    
    // input side
    input write,
    input[`ADDR_WIDTH-1:0] dataInAddr,
    input[2:0] dataInSubAddr, 
    input[5:0] addBit,
    
    // output side
    input[`ADDR_WIDTH-1:0] readAddr,
    output[37:0] summedDataOut,
    output[2:0] pcoeffCount
);

wire[5:0] subAddrMask = write ? (6'b000001 << dataInSubAddr) : 6'b000000;

localparam UPPER_ADDR_BITS = `ADDR_WIDTH - 10;

wire[59:0] blocksReadData[(1 << UPPER_ADDR_BITS)-1:0];

wire[9:0] dataWithECC;
assign dataWithECC[3:0] = addBit[3:0];
assign dataWithECC[5:4] = addBit[5:4]+1;
assign dataWithECC[9:6] = 4'b0000; // TODO ECC

genvar i;
generate
for(i = 0; i < (1 << UPPER_ADDR_BITS); i = i + 1) begin
    collectorBlock cBlock(
        .clk(clk),
        
        // input side
        .writeEnable(write & (dataInAddr[(10+UPPER_ADDR_BITS)-1:10] == i)),
        .writeAddr(dataInAddr[9:0]),
        .subAddrMask(subAddrMask),
        .writeData(dataWithECC),
        
        // output side
        .readEnable(readAddr[(10+UPPER_ADDR_BITS)-1:10] == (i+(1 << UPPER_ADDR_BITS)-1) % (1 << UPPER_ADDR_BITS)), // this reads from the memory and also wipes the read entry. 
        .readAddr(readAddr[9:0]),
        .readData(blocksReadData[i])
    );
end
wire[59:0] totalReadData;
if(UPPER_ADDR_BITS == 2) 
    assign totalReadData = (blocksReadData[0] | blocksReadData[1]) | (blocksReadData[2] | blocksReadData[3]);
else if(UPPER_ADDR_BITS == 3)
    assign totalReadData = (blocksReadData[0] | blocksReadData[1]) | (blocksReadData[2] | blocksReadData[3]) | 
                           (blocksReadData[4] | blocksReadData[5]) | (blocksReadData[6] | blocksReadData[7]);
else assign totalReadData = 60'bX;
endgenerate

reg[5:0] counts[5:0];
reg countValids[5:0];
reg[3:0] eccValues[5:0];

reg[35:0] pcoeffs[5:0];

generate
for(i = 0; i < 6; i = i + 1) begin
    always @(posedge clk) begin
        counts[i][3:0] <= totalReadData[10*i+3 : 10*i];
        counts[i][5:4] <= totalReadData[10*i+5 : 10*i+4] - 1;
        countValids[i] <= totalReadData[10*i+5 : 10*i+4] != 2'b00;
        eccValues[i] <= totalReadData[10*i+9 : 10*i+6];
        pcoeffs[i] <= countValids[i] ? (35'b1 << counts[i]) : 35'b0;
    end
end
endgenerate

localparam RAM_CYCLES = 3;
localparam SUM_CYCLES = `OUTPUT_PIPELINE_READ_LATENCY - RAM_CYCLES;

reg[36:0] pcoeffs01; always @(posedge clk) pcoeffs01 <= pcoeffs[0] + pcoeffs[1];
reg[36:0] pcoeffs23; always @(posedge clk) pcoeffs23 <= pcoeffs[2] + pcoeffs[3];
reg[36:0] pcoeffs45; always @(posedge clk) pcoeffs45 <= pcoeffs[4] + pcoeffs[5];

wire[37:0] summedDataPipeStart = pcoeffs01 + pcoeffs23 + pcoeffs45;
// hyperpipe registers get divided across this big sum to improve FMax
hyperpipe #(.CYCLES(SUM_CYCLES - 2), .WIDTH(38)) summedDataPipe(clk, summedDataPipeStart, summedDataOut);

wire[2:0] pcoeffCountPrePipe = (countValids[0] + countValids[1])
       + (countValids[2] + countValids[3])
       + (countValids[4] + countValids[5]);
hyperpipe #(.CYCLES(SUM_CYCLES), .WIDTH(3)) pcoeffCountPipe(
    clk, pcoeffCountPrePipe, pcoeffCount);

endmodule
