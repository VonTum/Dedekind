`timescale 1ns / 1ps

module testFullPermutationPipeline();


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
reg rst = 0;
reg inputBotValid = 0;
reg longRST = 0;
reg transmitTop = 0;

reg slowDownOutput = 1;

initial begin
    #31
    rst = 1;
    longRST = 1;
    #140
    rst = 0;
    fork
    begin
    #2048
        longRST = 0;
        /*rst = 1;
        inputBotValid = 0;
        #60
        rst = 0;*/
    end
    begin
    #10
    transmitTop = 1;
    #4
    transmitTop = 0;
    #1024;
    end
    join
    
    // forever #100 inputBotValid = !inputBotValid;
    inputBotValid = 1;
    
    #16040
    slowDownOutput = 0;
    
    //#30000
    //inputBotValid = 0;
    //#6000
    //inputBotValid = 1;
end

parameter MEMSIZE = 2000;
reg[1+128+16+48-1:0] dataTable[MEMSIZE-1:0];
initial $readmemb("FullPermutePipelineTestSetOpenCL7.mem", dataTable);

reg[47:0] resultsSums[MEMSIZE-1:0];
reg[12:0] resultsCounts[MEMSIZE-1:0];
//initial for(integer i = 0; i < MEMSIZE; i = i + 1) resultsTable[i] = 0;

reg[$clog2(MEMSIZE)-1:0] inputIndex = 2;//3754;
reg[$clog2(MEMSIZE)-1:0] outputIndex = 2;//3754;

always @(inputBotValid or inputIndex) if(inputIndex >= MEMSIZE) inputBotValid = 0;

wire[127:0] bot = dataTable[inputIndex][128+16+48-1 : 16+48];
wire[127:0] top = dataTable[0][128+16+48-1 : 16+48];
wire[1:0] topChannel;
wire doneTransmittingTop;
topTransmitter topTransmitter (
    .clk(clock),
    .top(top),
    .transmitTop(transmitTop),
    .doneTransmitting(doneTransmittingTop),
    
    .topChannel(topChannel)
);

wire almostFull;
wire isPassingInput = inputBotValid && !almostFull;
wire isPassingOutput;

wor eccStatus;
wire[47:0] pcoeffSum;
wire[12:0] pcoeffCount;

wire[29:0] activities2x;

fullPermutationPipeline30 fullPermutationPipeline30Test (
    .clk(clock),
    .clk2x(clock2x),
    .rst(rst),
    .longRST(longRST),
    .activities2x(activities2x), // Instrumentation wire for profiling
    
    .topChannel(topChannel),
    
    // Input side
    .botIn(bot),
    .writeBotIn(isPassingInput),
    .almostFull(almostFull),
    
    // Output side
    .slowDown(slowDownOutput),
    .resultValid(isPassingOutput),
    .pcoeffSum(pcoeffSum),
    .pcoeffCount(pcoeffCount),
    .eccStatus(eccStatus)
);

wire nextOutputIsTopOutput = dataTable[outputIndex][1+128+16+48-1];
wire isPassingTopOutput = isPassingOutput && nextOutputIsTopOutput;
wire isPassingBotOutput = isPassingOutput && !nextOutputIsTopOutput;

always @(posedge clock) if(!rst) begin
    if(isPassingInput) begin
        inputIndex <= inputIndex + 1;
    end
    if(isPassingOutput) begin
        resultsSums[outputIndex] <= pcoeffSum;
        resultsCounts[outputIndex] <= pcoeffCount;
        
        outputIndex <= outputIndex + 1;
    end
end

wire[47:0] offsetSum = dataTable[outputIndex][48-1 : 0];
wire[12:0] offsetCount = dataTable[outputIndex][12+48 : 48];

wire CORRECT_SUM = isPassingOutput ? (pcoeffSum == offsetSum) : 1'b1; //1'bX;
wire CORRECT_COUNT = isPassingOutput ? (pcoeffCount == offsetCount) : 1'b1; //1'bX;

endmodule
