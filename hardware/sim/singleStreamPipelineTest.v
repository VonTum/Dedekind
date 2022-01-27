`timescale 1ns / 1ps

`include "../src/pipelineGlobals_header.v"

module singleStreamPipelineTest();

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
    #33000
    rst = 0;
    #10
    inputBotValid = 1;
    #30000
    inputBotValid = 0;
    #6000
    inputBotValid = 1;
end

parameter MEMSIZE = 100000;
reg[1+128+64+8-1:0] dataTable[MEMSIZE-1:0];
initial $readmemb("singleStreamPipelineTestSetForOpenCL7.mem", dataTable);

reg[64+8-1:0] resultsTable[MEMSIZE-1:0];
//initial for(integer i = 0; i < MEMSIZE; i = i + 1) resultsTable[i] = 0;

wire[127:0] top = dataTable[0][128+8-1:8];

reg[$clog2(MEMSIZE)-1:0] inputIndex = 1;
reg[$clog2(MEMSIZE)-1:0] outputIndex = 1;

always @(inputBotValid or inputIndex) if(inputIndex >= MEMSIZE) inputBotValid <= 0;

wire[127:0] bot;

wire[8:0] usedw;
wire validOutput;
wire isReadyForInput = usedw <= 250;
wire[5:0] countOut;
wire EXTRA_DATA_OUT; // should always be 1
wire inputFifoECC;
wire collectorECC;
wire isBotValidECC;
streamingCountConnectedCore #(1) elementUnderTest (
    clock,
    rst,
    top,
    
    // Input side
    inputBotValid && isReadyForInput,
    bot, 
    1'b1,
    usedw,
    
    // Output side
    validOutput,
    countOut,
    EXTRA_DATA_OUT,
    inputFifoECC,
    collectorECC,
    isBotValidECC
);

wire isPassingInput = inputBotValid & isReadyForInput;
wire isPassingOutput = validOutput;

always @(posedge clock) if(!rst) begin
    if(isPassingInput) begin
        inputIndex <= inputIndex + 1;
    end
    if(isPassingOutput) begin
        resultsTable[outputIndex] = {58'b0, countOut};
        
        outputIndex <= outputIndex + 1;
    end
end

assign bot = dataTable[inputIndex][128+8-1 : 8];

wire[4:0] offsetCount = dataTable[outputIndex][7 : 0];

wire CORRECT = isPassingOutput ? (countOut == offsetCount) : 1'b1; //1'bX;

endmodule
