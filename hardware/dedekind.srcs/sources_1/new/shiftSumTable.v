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


module collectionModule(
    // input side
    input dataInClk,
    input write,
    input[`ADDR_WIDTH-1:0] dataInAddr,
    input[5:0] addBit,
    
    // output side
    input dataOutClk,
    input sumAddrValid,
    input[`ADDR_WIDTH-1:0] sumAddr,
    output[`DATA_WIDTH-1:0] summedDataOut
);

wire dataInEnablePostLatency;
wire[5:0] addBitPostLatency;
wire[`ADDR_WIDTH-1:0] addrPostLatency;
wire[`DATA_WIDTH-1:0] readDataPostLatency;

registerPipe #(.WIDTH(1), .DEPTH(`READ_LATENCY)) enablePipeline(.clk(dataInClk), 
    .dataIn(write), 
    .dataOut(dataInEnablePostLatency)
);

registerPipe #(.WIDTH(6), .DEPTH(`READ_LATENCY)) addBitPipeline(.clk(dataInClk), 
    .dataIn(addBit), 
    .dataOut(addBitPostLatency)
);

registerPipe #(.WIDTH(`ADDR_WIDTH), .DEPTH(`READ_LATENCY)) addrPipeline(.clk(dataInClk), 
    .dataIn(dataInAddr), 
    .dataOut(addrPostLatency)
);


quadPortBRAM #(.DATA_WIDTH(`DATA_WIDTH), .ADDR_WIDTH(`ADDR_WIDTH), .READ_LATENCY(`READ_LATENCY)) memBlock(
    .clkA(dataInClk),
    .rAddrA(dataInAddr),
    .rDataA(readDataPostLatency),
    .wAddrA(addrPostLatency),
    .wDataA(readDataPostLatency + (35'b1 << addBitPostLatency)), // addBit can only go up to 35
    .wEnableA(dataInEnablePostLatency),
    
    .clkB(dataOutClk),
    .rAddrB(sumAddr),
    .rDataB(summedDataOut),
    .wAddrB(sumAddr),
    .wDataB(0),
    .wEnableB(sumAddrValid)
);


endmodule
