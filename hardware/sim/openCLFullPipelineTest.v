`timescale 1ns / 1ps

`include "../src/pipelineGlobals_header.v"

module openCLFullPipelineTest();

reg clk;
initial begin
    clk = 0;
    forever #1 clk = ~clk;
end
reg rst;
reg inputBotValid;
initial begin
    rst = 0;
    #30
    rst = 1;
    #2
    rst = 0;
    /*rst = 1;
    inputBotValid = 0;
    #60
    rst = 0;*/
    #10
    inputBotValid = 1;
    #30000
    inputBotValid = 0;
    #6000
    inputBotValid = 1;
end

parameter MEMSIZE = 60000;
reg[1+128+64+8-1:0] dataTable[MEMSIZE-1:0];
initial $readmemb("pipeline6PackTestSetForOpenCL7.mem", dataTable);

reg[64+8-1:0] resultsTable[MEMSIZE-1:0];
//initial for(integer i = 0; i < MEMSIZE; i = i + 1) resultsTable[i] = 0;

always @(posedge clk) if(inputIndex >= MEMSIZE) inputBotValid <= 0;

reg[$clog2(MEMSIZE)-1:0] inputIndex = 0;
reg[$clog2(MEMSIZE)-1:0] outputIndex = 0;

wire[127:0] bot;

wire startNewTop;
wire isReadyForInput;
wire validOutput;
reg isReadyForOutput = 1; // controller is always ready for output

initial begin
    forever #10000 isReadyForOutput = !isReadyForOutput;
end

wire[63:0] summedDataPcoeffCountOut;
wire[37:0] summedData = summedDataPcoeffCountOut[37:0];
wire[2:0] pcoeffCount = summedDataPcoeffCountOut[40:38];

openCLFullPipeline openclfp (
    .clock(clk),
    .resetn(!rst),
	.ivalid(inputBotValid), 
	.iready(isReadyForOutput),
	.ovalid(validOutput),
	.oready(isReadyForInput), 
    
    .startNewTop(startNewTop),
    .botLower(bot[63:0]), // Represents all final 3 var swaps
    .botUpper(bot[127:64]), // Represents all final 3 var swaps
    .summedDataPcoeffCountOut(summedDataPcoeffCountOut)   // first 16 bits pcoeffCountOut, last 48 bits summedDataOut
);

wire isPassingInput = inputBotValid & isReadyForInput;
wire isPassingOutput = validOutput & isReadyForOutput;

always @(posedge clk) if(!rst) begin
    if(isPassingInput) begin
        inputIndex <= inputIndex + 1;
    end
    if(isPassingOutput) begin
        resultsTable[outputIndex] = {27'b0, summedData, 5'b00000, pcoeffCount};
        
        outputIndex <= outputIndex + 1;
    end
end

assign {startNewTop, bot} = dataTable[inputIndex][1+128+64+8-1 : 64+8];



wire[37:0] offsetSum = dataTable[outputIndex][37+8-1 : 8];
wire[2:0] offsetCount = dataTable[outputIndex][2 : 0];

wire CORRECT_SUM = isPassingOutput ? (summedData == offsetSum) : 1'b1; //1'bX;
wire CORRECT_COUNT = isPassingOutput ? (pcoeffCount == offsetCount) : 1'b1; //1'bX;

endmodule
