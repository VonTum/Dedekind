`timescale 1ns / 1ps

module fastFIFOTest();

reg clk;
initial begin
    clk = 0;
    forever #1 clk = ~clk;
end
reg rst;


reg writeEnable = 0;
reg readRequest = 0;

initial begin
    forever begin
        #10 readRequest = 1;
        #26 readRequest = 0;
    end
end
initial begin
    rst = 0;
    #30
    rst = 1;
    #10
    rst = 0;
    #10
    writeEnable = 1;
    #2000
    writeEnable = 0;
    
end


localparam WIDTH = 20;
localparam DEPTH_LOG2 = 9;

reg[WIDTH-1:0] dataIn = 0; always @(posedge clk) if(writeEnable) dataIn <= dataIn + 1;
wire[DEPTH_LOG2-1:0] usedw;

wire[WIDTH-1:0] dataOut;
wire dataOutValid;

FastFIFO #(.WIDTH(WIDTH), .DEPTH_LOG2(DEPTH_LOG2)) fifoUnderTest (
    clk,
    rst,
    
    // input side
    writeEnable,
    dataIn,
    usedw,
    
    // output side
    readRequest,
    dataOut,
    dataOutValid
);

endmodule
