`timescale 1ns / 1ps

module popcnt #(parameter ORDER = 7) (
    input[(1<<ORDER)-1:0] bitset,
    output[ORDER:0] count
);

generate
    if(ORDER <= 0)
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


module popcntNaive #(parameter WIDTH = 5) (
    input[WIDTH-1:0] bits,
    output[$clog2(WIDTH+1)-1:0] count
);

wire[$clog2(WIDTH)-1:0] subCounts[WIDTH-1:0];
assign subCounts[0] = bits[0];

generate
for(genvar i = 1; i < WIDTH; i=i+1) begin
    assign subCounts[i] = subCounts[i-1] + bits[i];
end
endgenerate

assign count = subCounts[WIDTH-1];

endmodule
