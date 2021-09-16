`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 07/24/2021 11:13:14 PM
// Design Name: 
// Module Name: graphGenerator
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



module makeGraph(
    input[127:0] top,
    input[127:0] bot,
    output[127:0] graph,
    output isValid
);

assign graph = top & ~bot;
assign isValid = ~(|(bot & ~top)); // no bot elements above top

endmodule


/*module graphGenerator(
    input[127:0] top,
    input[127:0] bot,
    input dataAvailable,
    output[127:0] graph,
    output graphAvailable,
    output reg done
);

// iter over all permutations of bot
// perhaps look at Heap's algorithm

reg[127:0] curBot;

initial begin
    done = 1;
end

always @ (posedge clk) begin
    if(done & graphAvailable) begin
        curBot = bot;
        done = 0;
    end
    
    if(!done) begin
        
    end
end






endmodule*/
