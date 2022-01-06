`timescale 1ns / 1ps

module hasNeighbor(
    input[127:0] graphIn,
    output[127:0] hasNeighboring
);

generate
    genvar outI;
    genvar v;
    for(outI = 0; outI < 128; outI = outI + 1) begin
        wire[6:0] inputWires;
        for(v = 0; v < 7; v = v + 1) begin
            assign inputWires[v] = graphIn[((outI & (1 << v)) != 0) ? outI - (1 << v) : outI + (1 << v)];
        end
        assign hasNeighboring[outI] = |inputWires;
    end
endgenerate

endmodule

module singletonPopcnt(
    input clk,
    input[127:0] singletons,
    output reg[5:0] singletonCount
);

// singletons can't be next to each other
reg[63:0] halved;
genvar i;
generate
for(i = 0; i < 64; i = i + 1) begin
    always @(posedge clk) halved[i] <= singletons[2*i] | singletons[2*i+1];
end
endgenerate

// can use 2 bits per subsum, since at most 3 of halved[4*i+:4] can be 1 at the same time due to singleton constraint
/*
    Example of a 3 max bit possible singleton configuration. None of the bits can dominate or be dominated by another. 
    0
   /1\
  1/ \1
  0\ /0
   \0/
    0
*/
reg[1:0] sumsA[15:0];
generate
for(i = 0; i < 16; i = i + 1) begin
    //always @(posedge clk) sumsA[i] <= halved[4*i] + halved[4*i+1] + halved[4*i+2] + halved[4*i+3];
    // because the compiler is a big dum dum it way overengineers these sums. We have to use a lookup table. 
    always @(posedge clk) begin
        case(halved[4*i+:4])
        4'b0000: sumsA[i] <= 0;   4'b0001: sumsA[i] <= 1;   4'b0010: sumsA[i] <= 1;   4'b0011: sumsA[i] <= 2;
        4'b0100: sumsA[i] <= 1;   4'b0101: sumsA[i] <= 2;   4'b0110: sumsA[i] <= 2;   4'b0111: sumsA[i] <= 3;
        4'b1000: sumsA[i] <= 1;   4'b1001: sumsA[i] <= 2;   4'b1010: sumsA[i] <= 2;   4'b1011: sumsA[i] <= 3;
        4'b1100: sumsA[i] <= 2;   4'b1101: sumsA[i] <= 3;   4'b1110: sumsA[i] <= 3;   4'b1111: sumsA[i] <= 2'bXX;
        endcase
    end
end
endgenerate

// Finally crunsh all subsums down to our result
reg[2:0] sumsB[7:0];
reg[3:0] sumsC[3:0];
reg[4:0] sumsD[1:0];
generate
for(i = 0; i < 8; i = i + 1) begin always @(posedge clk) sumsB[i] <= sumsA[2*i] + sumsA[2*i+1]; end
for(i = 0; i < 4; i = i + 1) begin always @(posedge clk) sumsC[i] <= sumsB[2*i] + sumsB[2*i+1]; end
for(i = 0; i < 2; i = i + 1) begin always @(posedge clk) sumsD[i] <= sumsC[2*i] + sumsC[2*i+1]; end
endgenerate

always @(posedge clk) singletonCount <= sumsD[0] + sumsD[1];

endmodule

// produces singletonCount after a delay of 5 clock cycles (see PipelinedCountConnectedCore::CONNECT_COUNT_IN_LAG)
module singletonElimination(
    input clk,
    input[127:0] graphIn,
    output reg[127:0] nonSingletons,
    output[5:0] singletonCount
);

wire[127:0] hasNeighboring;
hasNeighbor neighborChecker(graphIn, hasNeighboring);

reg[127:0] graphInD; always @(posedge clk) graphInD <= graphIn;
reg[127:0] hasNeighboringD; always @(posedge clk) hasNeighboringD <= hasNeighboring;

wire[127:0] singletons = graphInD & ~hasNeighboringD;
always @(posedge clk) nonSingletons <= graphInD & hasNeighboringD;

singletonPopcnt singletonCounter(clk, singletons, singletonCount);

endmodule
