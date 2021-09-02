`timescale 1ns / 1ps
`include "bramProperties.vh"

//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 08/17/2021 03:39:47 PM
// Design Name: 
// Module Name: queuedAdderTable
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



module queuedAdderTable(
    input clk,
    
    // input side
    input[5:0] countIn,
    input[`ADDR_WIDTH-1:0] inIdx,
    input dataInAvailable,
    output fifoAlmostFull,
    output fifoFull,
    
    // output side
    input[`ADDR_WIDTH-1:0] outIdx,
    input isSelected, // should perform a read/reset cycle, if not, data can be written
    output[39:0] currentSum
);


wire fifoEmpty;
wire[5:0] fifoOutCount;
wire[`ADDR_WIDTH-1:0] fifoOutIdx;
wire fifoReadEnable = !fifoEmpty & !isSelected;

fifo_32x18b tableQueue(
    .clk(clk),
    .din({countIn, inIdx}), 
    .wr_en(dataInAvailable),
    .full(fifoFull),
    .almost_full(fifoAlmostFull),
    .empty(fifoEmpty),
    .dout({fifoOutCount, fifoOutIdx}),
    .rd_en(fifoReadEnable)
);

wire[`ADDR_WIDTH-1:0] readAddr = isSelected ? outIdx : fifoOutIdx;
wire[`ADDR_WIDTH-1:0] readAddrOut;
wire[`ADDR_WIDTH-1:0] writeAddr = readAddrOut;
wire wasFromFifo;
wire wasValid;
wire[5:0] bitToAdd;

wire writeEnable = wasValid;

wire[39:0] dataAdded = currentSum + (40'b1 << bitToAdd);

wire[39:0] dataIn = wasFromFifo ? dataAdded : 0; // write zero if selected

streamingMemory_512x40b #(8) dataTable (
    .clk(clk),
    
    // read side
    .readAddr(readAddr),
    .dout(currentSum),
    .readAddrOut(readAddrOut), // requested address and any extra given data is returned with the data
    .extraDataIn({fifoReadEnable, isSelected | !fifoEmpty, fifoOutCount}),
    .extraDataOut({wasFromFifo, wasValid, bitToAdd}),
    
    // write side
    .writeAddr(writeAddr),
    .din(dataIn),
    .writeEnable(writeEnable)
);




endmodule

