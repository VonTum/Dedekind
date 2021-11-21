`timescale 1ns / 1ps
`include "bramProperties.vh"

`ifdef ALTERA_RESERVED_QIS
`define USE_QUADPORTRAM_IP
`endif

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

`ifdef USE_QUADPORTRAM_IP
collectorM20K memBlock01 (
    .clock(clk),
	 
    .wren(blockWrite),
    .wraddress(blockWriteAddr),
    .byteena_a(writeMasks[1:0]),
    .data({blockWriteData,blockWriteData}),
	 
    .rden(readEnable),
    .rdaddress(readAddr),
    .q(readData[19:00])
);
collectorM20K memBlock23 (
    .clock(clk),
	 
    .wren(blockWrite),
    .wraddress(blockWriteAddr),
    .byteena_a(writeMasks[3:2]),
    .data({blockWriteData,blockWriteData}),
	 
    .rden(readEnable),
    .rdaddress(readAddr),
    .q(readData[39:20])
);
collectorM20K memBlock45 (
    .clock(clk),
	 
    .wren(blockWrite),
    .wraddress(blockWriteAddr),
    .byteena_a(writeMasks[5:4]),
    .data({blockWriteData,blockWriteData}),
	 
    .rden(readEnable),
    .rdaddress(readAddr),
    .q(readData[59:40])
);
`else
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
`endif


endmodule

module collectionModule(
    input clk,
    
    // input side
    input write,
    input[`ADDR_WIDTH-1:0] dataInAddr,
    input[2:0] dataInSubAddr, 
    input[5:0] addBit,
    
    // output side
    input readAddrValid,
    input[`ADDR_WIDTH-1:0] readAddr,
    output[37:0] summedDataOut,
    output[2:0] pcoeffCount
);

wire[5:0] subAddrMask = write ? (6'b000001 << dataInSubAddr) : 6'b000000;

localparam UPPER_ADDR_BITS = 2; // 4 sections

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
endgenerate

wire[59:0] totalReadData = (blocksReadData[0] | blocksReadData[1]) | (blocksReadData[2] | blocksReadData[3]);

wire[5:0] counts[5:0];
wire countValids[5:0];
wire[3:0] eccValues[5:0];

wire[35:0] pcoeffs[5:0];

generate
for(i = 0; i < 6; i = i + 1) begin
    assign counts[i][3:0] = totalReadData[10*i+3 : 10*i];
    assign counts[i][5:4] = totalReadData[10*i+5 : 10*i+4] - 1;
    assign countValids[i] = totalReadData[10*i+5 : 10*i+4] != 2'b00;
    assign eccValues[i] = totalReadData[10*i+9 : 10*i+6];
    assign pcoeffs[i] = countValids[i] ? (35'b1 << counts[i]) : 35'b0;
end
endgenerate

`define SUM_CYCLES 8

reg[36:0] pcoeffSums[2:0];
reg[37:0] bigSum1;
reg[36:0] bigSum2;
reg[37:0] summedDataPipeStart;
always @(posedge clk) begin
	pcoeffSums[0] <= pcoeffs[0] + pcoeffs[1];
	pcoeffSums[1] <= pcoeffs[2] + pcoeffs[3];
	pcoeffSums[2] <= pcoeffs[4] + pcoeffs[5];
	bigSum1 <= pcoeffSums[0] + pcoeffSums[1];
	bigSum2 <= pcoeffSums[2];
	summedDataPipeStart <= bigSum1 + bigSum2;
end
hyperpipe #(.CYCLES(`SUM_CYCLES - 3), .WIDTH(38)) summedDataPipe(clk, summedDataPipeStart, summedDataOut);

wire[2:0] pcoeffCountPrePipe = (countValids[0] + countValids[1])
       + (countValids[2] + countValids[3])
       + (countValids[4] + countValids[5]);
hyperpipe #(.CYCLES(`SUM_CYCLES), .WIDTH(3)) pcoeffCountPipe(
    clk, pcoeffCountPrePipe, pcoeffCount);

/*wire[2:0] upperWriteAddr = dataInAddr[11:9];
wire[8:0] lowerWriteAddr = dataInAddr[8:0];
wire[2:0] upperReadAddr = readAddr[11:9];
wire[8:0] lowerReadAddr = readAddr[8:0];

generate
for(genvar i = 0; i < 8; i=i+1) begin
    collectorBlock thisBlock(
        .clk(clk),
        
        .writeEnable(write & (upperWriteAddr == i)),
        .writeAddr(lowerWriteAddr),
        .writeSubAddr(dataInSubAddr),
        .writeData(addBit),
        
        // output side
        .readEnable(readAddrValid & (upperReadAddr == (i+1) % 8)), // 
        .readAddr(lowerReadAddr),
        .readData()
    );
end
endgenerate*/


/*wire dataInEnablePostLatency;
wire[5:0] addBitPostLatency;
wire[`ADDR_WIDTH-1:0] addrPostLatency;
wire[`DATA_WIDTH-1:0] readSumPostLatency;
wire[2:0] readCountPostLatency;
wire[`DATA_WIDTH-1:0] updatedSum = readSumPostLatency + (36'b1 << addBitPostLatency);
wire[2:0] updatedCount = readCountPostLatency + 1;

registerPipe #(.WIDTH(1), .DEPTH(`READ_LATENCY)) enablePipeline(
    .clk(clk), 
    .dataIn(write), 
    .dataOut(dataInEnablePostLatency)
);

registerPipe #(.WIDTH(6), .DEPTH(`READ_LATENCY)) addBitPipeline(
    .clk(clk), 
    .dataIn(addBit), 
    .dataOut(addBitPostLatency)
);

registerPipe #(.WIDTH(`ADDR_WIDTH), .DEPTH(`READ_LATENCY)) addrPipeline(
    .clk(clk), 
    .dataIn(dataInAddr), 
    .dataOut(addrPostLatency)
);

`ifdef USE_QUADPORTRAM_IP
pipelineResultsRAM memBlock(
    .clock(clk),
    
    .read_address_a(dataInAddr),
    .q_a({readSumPostLatency, readCountPostLatency}),
    .write_address_a(addrPostLatency),
    .data_a({updatedSum, updatedCount}), // addBit can only go up to 35
    .wren_a(dataInEnablePostLatency),
    
    .read_address_b(readAddr),
    .q_b({summedDataOut, pcoeffCount}),
    .write_address_b(readAddr),
    .data_b(43'b0),
    .wren_b(readAddrValid)
);
`else
quadPortBRAM #(.DATA_WIDTH(`DATA_WIDTH + 3), .ADDR_WIDTH(`ADDR_WIDTH), .READ_LATENCY(`READ_LATENCY)) memBlock(
    .clk(clk),
    
    .rAddrA(dataInAddr),
    .rDataA({readSumPostLatency, readCountPostLatency}),
    .wAddrA(addrPostLatency),
    .wDataA({updatedSum, updatedCount}), // addBit can only go up to 35
    .wEnableA(dataInEnablePostLatency),
    
    .rAddrB(readAddr),
    .rDataB({summedDataOut, pcoeffCount}),
    .wAddrB(readAddr),
    .wDataB(43'b0),
    .wEnableB(readAddrValid)
);
`endif*/

endmodule
