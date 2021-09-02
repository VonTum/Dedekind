`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 06/27/2021 11:08:40 PM
// Design Name: 
// Module Name: testFile
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

module testFile(
        input[127:0] testIn,
        output[127:0] testOut
    );
    
    reg clk;
    reg start;
    wire connectCount;
    wire done;
    
    //countConnectedCore core(.clk(clk),.graphIn(testIn),.start(start),.connectCountStart(0),.connectCount(connectCount),.done(done));
    
    // assign testOut[127:0] = testIn[127:0];
    
    //monotonizeDown mDown(testIn, testOut);
    
    
initial begin
    clk = 0;
    start = 0;
    #1000 forever #25 clk = ~clk;
    #1100 start = 1;
end
endmodule
