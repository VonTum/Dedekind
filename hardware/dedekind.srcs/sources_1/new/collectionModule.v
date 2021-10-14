`timescale 1ns / 1ps
`include "bramProperties.vh"
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 08/25/2021 01:20:22 AM
// Design Name: 
// Module Name: shiftSumTable
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: 
// 
// Dependencies: 
// 
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
// 
//////////////////////////////////////////////////////////////////////////////////


module collectorBlock(
    input clk,
    
    // input side
    input writeEnable,
    input[8:0] writeAddr,
    input[1:0] writeSubAddr,
    input[5:0] writeData,
    // reset
    input rstOnOutClk,
    input[8:0] rstAddr,
    
    // output side
    input readEnable,
    input[8:0] readAddr,
    output[27:0] readData
);

wire rst;
synchronizer sync(outClk, inClk, rstOnOutClk, rst);

wire[9:0] wideWriteData;
assign wideWriteData[9:7] = 3'b000;
assign wideWriteData[6] = !rst;
assign wideWriteData[5:0] = writeData;

wire[39:0] memOut;
assign readData[6:0] = memOut[6:0];
assign readData[13:7] = memOut[16:10];
assign readData[20:14] = memOut[26:20];
assign readData[27:21] = memOut[36:30];

simpleDualPortM20K memBlock(
    .writeAddr(rst ? rstAddr : writeAddr),
    .writeMask(rst? 4'b1111 : writeEnable ? (4'b0001 << writeSubAddr) : 4'b0000),
    .writeClk(clk),
    .writeData({wideWriteData,wideWriteData,wideWriteData,wideWriteData}),
    
    .readClk(clk),
    .readEnable(readEnable),
    .readAddr(readAddr),
    .readData(memOut)
);

endmodule

module collectionModule(
    input clk,
    
    // input side
    input write,
    input[`ADDR_WIDTH-1:0] dataInAddr,
    input[1:0] dataInSubAddr, 
    input[5:0] addBit,
    
    // output side
    input sumAddrValid,
    input[`ADDR_WIDTH-1:0] sumAddr,
    output[`DATA_WIDTH-1:0] summedDataOut,
    output[2:0] pcoeffCount
);

/*
wire[2:0] upperWriteAddr = dataInAddr[11:9];
wire[8:0] lowerWriteAddr = dataInAddr[8:0];
wire[2:0] upperReadAddr = sumAddr[11:9];
wire[8:0] lowerReadAddr = sumAddr[8:0];

reg[8:0] rstAddr = 0; always @(posedge dataInClk) rstAddr = rstAdr+1;

generate
for(genvar i = 0; i < 8; i=i+1) begin
    collectorBlock thisBlock(
        .clk(clk),
        
        .writeEnable(write & (upperWriteAddr == i)),
        .writeAddr(lowerWriteAddr),
        .writeSubAddr(dataInSubAddr),
        .writeData(addBit),
        
        .rstOnOutClk(),
        .rstAddr(),
        // output side
        .readEnable(),
        .readAddr(),
        .readData()
    );
end
endgenerate*/

wire dataInEnablePostLatency;
wire[5:0] addBitPostLatency;
wire[`ADDR_WIDTH-1:0] addrPostLatency;
wire[`DATA_WIDTH-1:0] readDataPostLatency;

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


quadPortBRAM #(.DATA_WIDTH(`DATA_WIDTH + 3), .ADDR_WIDTH(`ADDR_WIDTH), .READ_LATENCY(`READ_LATENCY)) memBlock(
    .clk(clk),
    
    .rAddrA(dataInAddr),
    .rDataA(readDataPostLatency),
    .wAddrA(addrPostLatency),
    .wDataA(readDataPostLatency + (35'b1 << addBitPostLatency)), // addBit can only go up to 35
    .wEnableA(dataInEnablePostLatency),
    
    .rAddrB(sumAddr),
    .rDataB(summedDataOut),
    .wAddrB(sumAddr),
    .wDataB(0),
    .wEnableB(sumAddrValid)
);

endmodule
