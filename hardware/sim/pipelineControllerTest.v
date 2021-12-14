`timescale 1ns / 1ps

module pipelineControllerTest();


reg clk = 0;
initial begin
    forever #1 clk = ~clk;
end
reg rst = 1;
initial begin
    #10 rst = 0;
end

reg upToAddr = 68461;
reg start = 0;
initial begin
    #10 start = 1;
    #2 start = 0;
end

reg queueFull = 0;

localparam BIG_ADDR_WIDTH = 32;

wire[BIG_ADDR_WIDTH-1:0] pipelineInputAddr;
wire pipelineAddrValid;

wire[BIG_ADDR_WIDTH-1:0] resultSavingAddr;
wire resultAddrValid;

pipelineController #(BIG_ADDR_WIDTH) controller(
    clk,
    rst,
    upToAddr,
    start,
    
    queueFull,
    pipelineInputAddr,
    pipelineAddrValid,
    
    resultSavingAddr,
    resultAddrValid
); 


endmodule
