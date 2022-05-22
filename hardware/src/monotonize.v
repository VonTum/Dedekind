`timescale 1ns / 1ps

module monotonizeUp (
    input[127:0] vIn,
    output[127:0] vOut
);

wire[127:0] addA;
wire[127:0] addB;
wire[127:0] addC;
wire[127:0] addD;
wire[127:0] addE;
wire[127:0] addF;
wire[127:0] addG;

genvar i;
generate
    // add A
    for(i = 0; i < 128; i = i + 1) begin
        assign addA[i] = (i % 2 >= 1) ? vIn[i] | vIn[i-1] : vIn[i];
    end
    // add B
    for(i = 0; i < 128; i = i + 1) begin
        assign addB[i] = (i % 4 >= 2) ? addA[i] | addA[i-2] : addA[i];
    end
    // add C
    for(i = 0; i < 128; i = i + 1) begin
        assign addC[i] = (i % 8 >= 4) ? addB[i] | addB[i-4] : addB[i];
    end
    // add D
    for(i = 0; i < 128; i = i + 1) begin
        assign addD[i] = (i % 16 >= 8) ? addC[i] | addC[i-8] : addC[i];
    end
    // add E
    for(i = 0; i < 128; i = i + 1) begin
        assign addE[i] = (i % 32 >= 16) ? addD[i] | addD[i-16] : addD[i];
    end
    // add F
    for(i = 0; i < 128; i = i + 1) begin
        assign addF[i] = (i % 64 >= 32) ? addE[i] | addE[i-32] : addE[i];
    end
    // add G
    for(i = 0; i < 128; i = i + 1) begin
        assign addG[i] = (i >= 64) ? addF[i] | addF[i-64] : addF[i];
    end
endgenerate

assign vOut = addG;

endmodule


module monotonizeDown (
    input[127:0] vIn,
    output[127:0] vOut
);

wire[127:0] removeG;
wire[127:0] removeF;
wire[127:0] removeE;
wire[127:0] removeD;
wire[127:0] removeC;
wire[127:0] removeB;
wire[127:0] removeA;

genvar i;
generate
    // remove G
    for(i = 0; i < 128; i = i + 1) begin
        assign removeG[i] = (i < 64) ? vIn[i] | vIn[i+64] : vIn[i];
    end
    // remove F
    for(i = 0; i < 128; i = i + 1) begin
        assign removeF[i] = (i % 64 < 32) ? removeG[i] | removeG[i+32] : removeG[i];
    end
    // remove E
    for(i = 0; i < 128; i = i + 1) begin
        assign removeE[i] = (i % 32 < 16) ? removeF[i] | removeF[i+16] : removeF[i];
    end
    // remove D
    for(i = 0; i < 128; i = i + 1) begin
        assign removeD[i] = (i % 16 < 8) ? removeE[i] | removeE[i+8] : removeE[i];
    end
    // remove C
    for(i = 0; i < 128; i = i + 1) begin
        assign removeC[i] = (i % 8 < 4) ? removeD[i] | removeD[i+4] : removeD[i];
    end
    // remove B
    for(i = 0; i < 128; i = i + 1) begin
        assign removeB[i] = (i % 4 < 2) ? removeC[i] | removeC[i+2] : removeC[i];
    end
    // remove A
    for(i = 0; i < 128; i = i + 1) begin
        assign removeA[i] = (i % 2 < 1) ? removeB[i] | removeB[i+1] : removeB[i];
    end
endgenerate

assign vOut = removeA;

endmodule


module monotonizePart16 (
    input clk,
    input[15:0] din,
    output[15:0] dout/*
        1111,
        1110,
        1101,
        1100,
        1011,
        1010,
        1001,
        1000,
        0111,
        0110,
        0101,
        0100,
        0011,
        0010,
        0001,
        0000
    */
);

reg[15:0] dinD;

wire[15:0] groups4;
reg[15:0] groups4D;
wire[15:0] groups8;
reg[15:0] groups8D;
always @(posedge clk) begin
    dinD <= din;
    groups4D <= groups4;
    groups8D <= groups8;
end

assign groups4[4'b0011] = din[4'b0011] || din[4'b0010] || din[4'b0001] || din[4'b0000];
assign groups4[4'b0101] = din[4'b0101] || din[4'b0100] || din[4'b0001] || din[4'b0000];
assign groups4[4'b0110] = din[4'b0110] || din[4'b0100] || din[4'b0010] || din[4'b0000];
assign groups4[4'b1001] = din[4'b1001] || din[4'b1000] || din[4'b0001] || din[4'b0000];
assign groups4[4'b1010] = din[4'b1010] || din[4'b1000] || din[4'b0010] || din[4'b0000];
assign groups4[4'b1100] = din[4'b1100] || din[4'b1000] || din[4'b0100] || din[4'b0000];

wire groups4_01XX = din[4'b0111] || din[4'b0110] || din[4'b0101] || din[4'b0100];
wire groups4_10XX = din[4'b1011] || din[4'b1010] || din[4'b1001] || din[4'b1000];

assign groups8[4'b0111] = groups4[4'b0011] || groups4_01XX;
assign groups8[4'b1011] = groups4[4'b0011] || groups4_10XX;
assign groups8[4'b1101] = groups4[4'b1100] || (din[4'b1101] || din[4'b1001] || din[4'b0101] || din[4'b0001]);
assign groups8[4'b1110] = groups4[4'b1100] || (din[4'b1110] || din[4'b1010] || din[4'b0110] || din[4'b0010]);

// 16
reg group16;
wire group16LastSet = din[4'b1111] || din[4'b1110] || din[4'b1101] || din[4'b1100];
always @(posedge clk) group16 <= groups4[4'b0011] || groups4_01XX || groups4_10XX || group16LastSet;
assign dout[4'b1111] = group16;

// 8
assign dout[4'b0111] = groups8D[4'b0111];
assign dout[4'b1011] = groups8D[4'b1011];
assign dout[4'b1101] = groups8D[4'b1101];
assign dout[4'b1110] = groups8D[4'b1110];

// 4
assign dout[4'b0011] = groups4D[4'b0011];
assign dout[4'b0101] = groups4D[4'b0101];
assign dout[4'b0110] = groups4D[4'b0110];
assign dout[4'b1001] = groups4D[4'b1001];
assign dout[4'b1010] = groups4D[4'b1010];
assign dout[4'b1100] = groups4D[4'b1100];

// 2
assign dout[4'b0001] = dinD[4'b0001] || dinD[4'b0000];
assign dout[4'b0010] = dinD[4'b0010] || dinD[4'b0000];
assign dout[4'b0100] = dinD[4'b0100] || dinD[4'b0000];
assign dout[4'b1000] = dinD[4'b1000] || dinD[4'b0000];

// 1
assign dout[4'b0000] = dinD[4'b0000];

endmodule

// 1 pipeline stage
module pipelinedMonotonizeUp (
    input clk,
    input[127:0] vIn,
    output[127:0] vOut
);

/*wire[127:0] addA;
wire[127:0] addB;
wire[127:0] addC;*/
wire[127:0] addD;
wire[127:0] addE;
wire[127:0] addF;
wire[127:0] addG;

genvar i;
generate
    /*
    // add A
    for(i = 0; i < 128; i = i + 1) begin
        assign addA[i] = (i % 2 >= 1) ? vIn[i] | vIn[i-1] : vIn[i];
    end
    
    // add B
    for(i = 0; i < 128; i = i + 1) begin
        assign addB[i] = (i % 4 >= 2) ? addA[i] | addA[i-2] : addA[i];
    end
    
    // add C
    for(i = 0; i < 128; i = i + 1) begin
        assign addC[i] = (i % 8 >= 4) ? addB[i] | addB[i-4] : addB[i];
    end
    
    // add D
    for(i = 0; i < 128; i = i + 1) begin
        always @(posedge clk) addD[i] <= (i % 16 >= 8) ? addC[i] | addC[i-8] : addC[i];
    end*/
    
    // ALM Optimization
    for(i = 0; i < 8; i=i+1) begin
        monotonizePart16 mPart(clk, vIn[i*16 +: 16], addD[i*16 +: 16]);
    end
    
    // add E
    for(i = 0; i < 128; i = i + 1) begin
        assign addE[i] = (i % 32 >= 16) ? addD[i] | addD[i-16] : addD[i];
    end
    
    // add F
    for(i = 0; i < 128; i = i + 1) begin
        assign addF[i] = (i % 64 >= 32) ? addE[i] | addE[i-32] : addE[i];
    end
    
    // add G
    for(i = 0; i < 128; i = i + 1) begin
        assign addG[i] = (i >= 64) ? addF[i] | addF[i-64] : addF[i];
    end
    
endgenerate

assign vOut = addG;

endmodule

// 1 pipeline stage
module pipelinedMonotonizeDown (
    input clk,
    input[127:0] vIn,
    output[127:0] vOut
);

/*wire[127:0] removeG;
wire[127:0] removeF;
wire[127:0] removeE;*/
wire[127:0] removeD;
wire[127:0] removeC;
wire[127:0] removeB;
wire[127:0] removeA;

genvar i;
generate
    
    // remove G
    /*for(i = 0; i < 128; i = i + 1) begin
        assign removeG[i] = (i < 64) ? vIn[i] | vIn[i+64] : vIn[i];
    end
    
    // remove F
    for(i = 0; i < 128; i = i + 1) begin
        assign removeF[i] = (i % 64 < 32) ? removeG[i] | removeG[i+32] : removeG[i];
    end
    
    // remove E
    for(i = 0; i < 128; i = i + 1) begin
        assign removeE[i] = (i % 32 < 16) ? removeF[i] | removeF[i+16] : removeF[i];
    end
    
    // remove D
    for(i = 0; i < 128; i = i + 1) begin
        always @(posedge clk) removeD[i] <= (i % 16 < 8) ? removeE[i] | removeE[i+8] : removeE[i];
    end*/
    
    
    // ALM Optimization
    for(i = 0; i < 8; i = i + 1) begin
        wire[15:0] inputWires;
        wire[15:0] outputWires;
        
        for(genvar j = 0; j < 16; j=j+1) begin
            assign inputWires[15-j] = vIn[8*j + i];
            assign removeD[8*j + i] = outputWires[15-j];
        end
        
        monotonizePart16 mPart(clk, inputWires, outputWires);
    end
    
    // remove C
    for(i = 0; i < 128; i = i + 1) begin
        assign removeC[i] = (i % 8 < 4) ? removeD[i] | removeD[i+4] : removeD[i];
    end
    
    // remove B
    for(i = 0; i < 128; i = i + 1) begin
        assign removeB[i] = (i % 4 < 2) ? removeC[i] | removeC[i+2] : removeC[i];
    end
    
    // remove A
    for(i = 0; i < 128; i = i + 1) begin
        assign removeA[i] = (i % 2 < 1) ? removeB[i] | removeB[i+1] : removeB[i];
    end
    
endgenerate

assign vOut = removeA;

endmodule




