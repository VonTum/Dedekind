`timescale 1ns / 1ps

module fullPermutationPipeline(
    input clk,
    input rst,
    
    input[127:0] top,
    
    // Input side
    input[127:0] bot,
    input writeBot,
    output readyForInputBot,
    
    // Output side
    input grabResults,
    output resultsAvailable,
    output[47:0] pcoeffSum,
    output[12:0] pcoeffCount,
    output eccStatus
);



wire[127:0] permutedBot;
wire permutedBotValid;
wire batchDone;
wire pipelineRequestsSlowdown;
wire permutationGeneratorRequestsInputBot;

assign readyForInputBot = permutationGeneratorRequestsInputBot && !pipelineRequestsSlowdown;

permutationGenerator67 permutationGen (
    .clk(clk),
    .rst(rst),
    
    .inputBot(bot),
    .writeInputBot(writeBot),
    .hasSpaceForNextBot(permutationGeneratorRequestsInputBot),
    
    .outputBot(permutedBot),
    .outputBotValid(permutedBotValid),
    .botSeriesFinished(batchDone)
);

wire grabNewResult;
wire pipelineResultAvailable;
wire[47:0] pcoeffSumFromPipeline;
wire[12:0] pcoeffCountFromPipeline;

// sums all 120 permutations of variables 2,3,4,5,6.
pipeline120Pack computePipe (
    .clk(clk),
    .rst(rst),
    
    .top(top),
    .bot(permutedBot),
    .isBotValid(permutedBotValid),
    .batchDone(batchDone),
    .slowDownInput(pipelineRequestsSlowdown),
    
    // Output side
    .grabResults(grabNewResult),
    .resultsAvailable(pipelineResultAvailable),
    .pcoeffSum(pcoeffSumFromPipeline),
    .pcoeffCount(pcoeffCountFromPipeline),
    .eccStatus(eccStatus)
);

reg[2:0] cyclesSinceGrabNewResult;
wire isReadyForNextGrab = cyclesSinceGrabNewResult == 7;
assign grabNewResult = !resultsAvailable && pipelineResultAvailable && isReadyForNextGrab;

always @(posedge clk) begin
    if(rst || grabNewResult) begin
        cyclesSinceGrabNewResult <= 0;
    end else begin
        if(!isReadyForNextGrab) begin
            cyclesSinceGrabNewResult <= cyclesSinceGrabNewResult + 1;
        end
    end
end

wire grabNewResultArrived;
hyperpipe #(.CYCLES(5)) writePipeToRegister(clk, grabNewResult, grabNewResultArrived);

pipelineRegister #(.WIDTH(48+13)) outputReg(
    .clk(clk),
    .rst(rst),
    
    .write(grabNewResultArrived),
    .dataIn({pcoeffSumFromPipeline, pcoeffCountFromPipeline}),
    
    .grab(grabResults),
    .hasData(resultsAvailable),
    .data({pcoeffSum, pcoeffCount})
);

endmodule
