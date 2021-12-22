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




module pipelinedMonotonizeUp (
    input clk,
    input[127:0] vIn,
    output[127:0] vOut
);

wire[127:0] addA;
wire[127:0] addB;
wire[127:0] addC;
reg[127:0] addD;
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
        always @(posedge clk) addD[i] <= (i % 16 >= 8) ? addC[i] | addC[i-8] : addC[i];
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


module pipelinedMonotonizeDown (
    input clk,
    input[127:0] vIn,
    output[127:0] vOut
);

wire[127:0] removeG;
wire[127:0] removeF;
wire[127:0] removeE;
reg[127:0] removeD;
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
        always @(posedge clk) removeD[i] <= (i % 16 < 8) ? removeE[i] | removeE[i+8] : removeE[i];
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



