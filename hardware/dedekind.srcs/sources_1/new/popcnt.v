`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 07/23/2021 03:37:51 PM
// Design Name: 
// Module Name: popcnt
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


module popcnt(bitset,count);

parameter ORDER = 7;

input [(1<<ORDER)-1:0] bitset;
output [ORDER:0] count;

generate
    if(ORDER == 0)
        assign count = bitset;
    else begin
        wire[ORDER-1:0] countA;
        wire[ORDER-1:0] countB;
        
        popcnt #(ORDER-1) subCountA(bitset[(1<<(ORDER-1))-1:0], countA);
        popcnt #(ORDER-1) subCountB(bitset[(1<<(ORDER))-1:(1<<(ORDER-1))], countB);
        
        assign count = countA + countB;
    end
endgenerate

endmodule
