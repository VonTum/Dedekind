`timescale 1ns / 1ps

module OpenCLFullPermutePipelineTest();

reg clock;
initial begin
    clock = 0;
    forever #2 clock = ~clock;
end
reg clock2x;
initial begin
    clock2x = 0;
    forever #1 clock2x = ~clock2x;
end
reg rst;
reg inputBotValid = 0;
initial begin
    rst = 0;
    #31
    rst = 1;
    #4
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

parameter MEMSIZE = 200;
reg[1+128+16+48-1:0] dataTable[MEMSIZE-1:0];
initial $readmemb("FullPermutePipelineTestSetOpenCL7.mem", dataTable);

reg[16+48-1:0] resultsTable[MEMSIZE-1:0];
//initial for(integer i = 0; i < MEMSIZE; i = i + 1) resultsTable[i] = 0;

reg[$clog2(MEMSIZE)-1:0] inputIndex = 0;
reg[$clog2(MEMSIZE)-1:0] outputIndex = 0;

always @(inputBotValid or inputIndex) if(inputIndex >= MEMSIZE) inputBotValid <= 0;

wire[127:0] bot;

wire startNewTop;
wire isReadyForInput;
wire validOutput;
reg isReadyForOutput = 0; // controller is always ready for output

initial begin
    forever #10000 isReadyForOutput = !isReadyForOutput;
end

wire[63:0] summedDataPcoeffCountOut;
wire eccStatus = summedDataPcoeffCountOut[63];
wire[47:0] summedData = summedDataPcoeffCountOut[47:0];
wire[12:0] pcoeffCount = summedDataPcoeffCountOut[13+48-1:48];

OpenCLFullPermutationPipeline elementUnderTest (
    .clock(clock),
    .clock2x(clock2x),
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

wire isPassingInput = inputBotValid && isReadyForInput;
wire isPassingOutput = validOutput && isReadyForOutput;

wire nextOutputIsTopOutput = dataTable[outputIndex][1+128+16+48-1];
wire isPassingTopOutput = isPassingOutput && nextOutputIsTopOutput;
wire isPassingBotOutput = isPassingOutput && !nextOutputIsTopOutput;

always @(posedge clock) if(!rst) begin
    if(isPassingInput) begin
        inputIndex <= inputIndex + 1;
    end
    if(isPassingOutput) begin
        resultsTable[outputIndex] = summedDataPcoeffCountOut;
        
        outputIndex <= outputIndex + 1;
    end
end

assign {startNewTop, bot} = dataTable[inputIndex][1+128+16+48-1 : 16+48];

wire[47:0] offsetSum = dataTable[outputIndex][48-1 : 0];
wire[12:0] offsetCount = dataTable[outputIndex][12+48 : 48];

wire CORRECT_SUM = isPassingBotOutput ? (summedData == offsetSum) : 1'b1; //1'bX;
wire CORRECT_COUNT = isPassingBotOutput ? (pcoeffCount == offsetCount) : 1'b1; //1'bX;

// 4 digit fixed-point real number
wire[127:0] OCCUPANCY = isPassingTopOutput ? (summedDataPcoeffCountOut[63:32] << 6) * (10000 / 40) / summedDataPcoeffCountOut[31:0] : 1'bX;

endmodule
