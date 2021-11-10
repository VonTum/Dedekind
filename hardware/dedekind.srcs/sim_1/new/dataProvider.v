`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 08/30/2021 05:00:14 PM
// Design Name: 
// Module Name: dataProvider
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


module indexProvider #(parameter ADDR_DEPTH = 4096, parameter MIN_OUTPUT_DELAY = 0) (
    input clk,
    input rst,
    output reg[$clog2(ADDR_DEPTH)-1:0] index,
    output dataAvailable,
    input requestData
);

initial index = 0;

reg hasStarted = 0;
reg isDone = 0;

generate
    if(MIN_OUTPUT_DELAY == 0) 
        assign dataAvailable = !rst & !isDone;
    else begin
        integer delayTillNext = MIN_OUTPUT_DELAY;
        assign dataAvailable = !rst & !isDone & (delayTillNext == 0);
        always @(posedge clk) begin
            if(dataAvailable & requestData)
                delayTillNext <= MIN_OUTPUT_DELAY;
            else begin
                if(delayTillNext > 0)
                    delayTillNext <= delayTillNext - 1;
            end
        end
    end
endgenerate

always @(posedge clk) begin
    if(rst) begin
        index <= 0;
        hasStarted <= 0;
        isDone <= 0;
    end else begin
        if(hasStarted & (index == ADDR_DEPTH - 1)) isDone <= 1;
        
        if(dataAvailable & requestData) begin
            index <= index + 1;
            hasStarted <= 1;
        end
    end
end

endmodule


module dataProvider #(parameter FILE_NAME = "testData.mem", parameter DATA_WIDTH = 256, parameter ADDR_DEPTH = 4096, parameter MIN_OUTPUT_DELAY = 0) (
    input clk,
    input rst,
    output [$clog2(ADDR_DEPTH)-1:0] index,
    output dataAvailable,
    input requestData,
    output[DATA_WIDTH-1:0] data
);

indexProvider #(ADDR_DEPTH, MIN_OUTPUT_DELAY) ip(clk, rst, index, dataAvailable, requestData);

reg[DATA_WIDTH-1:0] dataTable[ADDR_DEPTH-1:0];
initial $readmemb(FILE_NAME, dataTable);
assign data = dataTable[index];

endmodule
