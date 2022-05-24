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



// Selectable register positions
module pipelinedMonotonizeUp #(
    parameter RA = 0,
    parameter RB = 0,
    parameter RC = 0,
    parameter RD = 0,
    parameter RE = 0,
    parameter RF = 0,
    parameter RG = 0
) (
    input clk,
    input[127:0] vIn,
    output[127:0] vOut
);

wire[127:0] addA_P; reg[127:0] addA;
wire[127:0] addB_P; reg[127:0] addB;
wire[127:0] addC_P; reg[127:0] addC;
wire[127:0] addD_P; reg[127:0] addD;
wire[127:0] addE_P; reg[127:0] addE;
wire[127:0] addF_P; reg[127:0] addF;
wire[127:0] addG_P; reg[127:0] addG;

genvar i;
generate
    // add A
    for(i = 0; i < 128; i = i + 1) begin
        assign addA_P[i] = (i % 2 >= 1) ? vIn[i] | vIn[i-1] : vIn[i];
    end
    if(RA) always @(posedge clk) addA <= addA_P;
    else always @(*) addA = addA_P;
    
    // add B
    for(i = 0; i < 128; i = i + 1) begin
        assign addB_P[i] = (i % 4 >= 2) ? addA[i] | addA[i-2] : addA[i];
    end
    if(RB) always @(posedge clk) addB <= addB_P;
    else always @(*) addB = addB_P;
    
    // add C
    for(i = 0; i < 128; i = i + 1) begin
        assign addC_P[i] = (i % 8 >= 4) ? addB[i] | addB[i-4] : addB[i];
    end
    if(RC) always @(posedge clk) addC <= addC_P;
    else always @(*) addC = addC_P;
    
    // add D
    for(i = 0; i < 128; i = i + 1) begin
        assign addD_P[i] = (i % 16 >= 8) ? addC[i] | addC[i-8] : addC[i];
    end
    if(RD) always @(posedge clk) addD <= addD_P;
    else always @(*) addD = addD_P;
    
    // add E
    for(i = 0; i < 128; i = i + 1) begin
        assign addE_P[i] = (i % 32 >= 16) ? addD[i] | addD[i-16] : addD[i];
    end
    if(RE) always @(posedge clk) addE <= addE_P;
    else always @(*) addE = addE_P;
    
    // add F
    for(i = 0; i < 128; i = i + 1) begin
        assign addF_P[i] = (i % 64 >= 32) ? addE[i] | addE[i-32] : addE[i];
    end
    if(RF) always @(posedge clk) addF <= addF_P;
    else always @(*) addF = addF_P;
    
    // add G
    for(i = 0; i < 128; i = i + 1) begin
        assign addG_P[i] = (i >= 64) ? addF[i] | addF[i-64] : addF[i];
    end
    if(RG) always @(posedge clk) addG <= addG_P;
    else always @(*) addG = addG_P;
    
endgenerate

assign vOut = addG;

endmodule

// Selectable register positions
module pipelinedMonotonizeDown #(
    parameter RG = 0,
    parameter RF = 0,
    parameter RE = 0,
    parameter RD = 0,
    parameter RC = 0,
    parameter RB = 0,
    parameter RA = 0
) (
    input clk,
    input[127:0] vIn,
    output[127:0] vOut
);

wire[127:0] removeG_P; reg[127:0] removeG;
wire[127:0] removeF_P; reg[127:0] removeF;
wire[127:0] removeE_P; reg[127:0] removeE;
wire[127:0] removeD_P; reg[127:0] removeD;
wire[127:0] removeC_P; reg[127:0] removeC;
wire[127:0] removeB_P; reg[127:0] removeB;
wire[127:0] removeA_P; reg[127:0] removeA;

genvar i;
generate
    // remove G
    for(i = 0; i < 128; i = i + 1) begin
        assign removeG_P[i] = (i < 64) ? vIn[i] | vIn[i+64] : vIn[i];
    end
    if(RG) always @(posedge clk) removeG <= removeG_P;
    else always @(*) removeG = removeG_P;
    
    // remove F
    for(i = 0; i < 128; i = i + 1) begin
        assign removeF_P[i] = (i % 64 < 32) ? removeG[i] | removeG[i+32] : removeG[i];
    end
    if(RF) always @(posedge clk) removeF <= removeF_P;
    else always @(*) removeF = removeF_P;
    
    // remove E
    for(i = 0; i < 128; i = i + 1) begin
        assign removeE_P[i] = (i % 32 < 16) ? removeF[i] | removeF[i+16] : removeF[i];
    end
    if(RE) always @(posedge clk) removeE <= removeE_P;
    else always @(*) removeE = removeE_P;
    
    // remove D
    for(i = 0; i < 128; i = i + 1) begin
        assign removeD_P[i] = (i % 16 < 8) ? removeE[i] | removeE[i+8] : removeE[i];
    end
    if(RD) always @(posedge clk) removeD <= removeD_P;
    else always @(*) removeD = removeD_P;
    
    // remove C
    for(i = 0; i < 128; i = i + 1) begin
        assign removeC_P[i] = (i % 8 < 4) ? removeD[i] | removeD[i+4] : removeD[i];
    end
    if(RC) always @(posedge clk) removeC <= removeC_P;
    else always @(*) removeC = removeC_P;
    
    // remove B
    for(i = 0; i < 128; i = i + 1) begin
        assign removeB_P[i] = (i % 4 < 2) ? removeC[i] | removeC[i+2] : removeC[i];
    end
    if(RB) always @(posedge clk) removeB <= removeB_P;
    else always @(*) removeB = removeB_P;
    
    // remove A
    for(i = 0; i < 128; i = i + 1) begin
        assign removeA_P[i] = (i % 2 < 1) ? removeB[i] | removeB[i+1] : removeB[i];
    end
    if(RA) always @(posedge clk) removeA <= removeA_P;
    else always @(*) removeA = removeA_P;
    
endgenerate

assign vOut = removeA;

endmodule




