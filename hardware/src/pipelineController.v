`timescale 1ns / 1ps

`include "pipelineGlobals.vh"


module pipelineController#(parameter BIG_ADDR_WIDTH = 32) (
    input clk,
    input rst,
    input[BIG_ADDR_WIDTH-1:0] upToAddr,
    input start,
    
    input queueFull,
    output reg[BIG_ADDR_WIDTH-1:0] pipelineInputAddr,
    output reg pipelineAddrValid,
    
    output[BIG_ADDR_WIDTH-1:0] resultSavingAddr,
    output resultAddrValid
);

`define WAITING 2'b00
`define INITIALIZING 2'b01
`define RUNNING 2'b10

reg[1:0] state;

reg isNewAddress;

reg[BIG_ADDR_WIDTH-1:0] upTo;

always @(posedge clk) begin
    if(rst) begin
        state <= `WAITING;
        pipelineAddrValid <= 1'b0;
        isNewAddress <= 1'b0;
    end else begin
        case(state)
        `WAITING: begin
            if(start) begin
                upTo <= upToAddr;
                pipelineInputAddr <= 0;
                pipelineAddrValid <= 1'b0;
                state <= `INITIALIZING;
            end
        end
        `INITIALIZING: begin
            if(pipelineInputAddr[`OUTPUT_INDEX_OFFSET_BITS-1:0] == (1 << `OUTPUT_INDEX_OFFSET_BITS)-1) begin
                pipelineInputAddr <= 0;
                pipelineAddrValid <= 1'b1;
                state <= `RUNNING;
            end else begin
                pipelineInputAddr <= pipelineInputAddr + 1;
            end
        end
        `RUNNING: begin
            if(pipelineInputAddr == upTo + `OUTPUT_INDEX_OFFSET) begin
                state <= `WAITING;
                pipelineAddrValid <= 1'b0;
            end else begin
                if(!queueFull) begin
                    pipelineInputAddr <= pipelineInputAddr + 1;
                    if(pipelineInputAddr <= upTo) begin
                        pipelineAddrValid <= 1'b1;
                    end
                end else begin
                    pipelineAddrValid <= 1'b0;
                end
            end
        end
        endcase
    end
end

wire[BIG_ADDR_WIDTH-1:0] resultSavingAddrPreDelay = pipelineInputAddr;

endmodule
