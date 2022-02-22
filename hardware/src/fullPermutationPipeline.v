`timescale 1ns / 1ps

module fullPermutationPipeline(
    input clk,
    input clk2x,
    input rst,
    output[5:0] activityMeasure, // Instrumentation wire for profiling (0-40 activity level)
    
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

(* dont_merge *) reg permutationGeneratorRST; always @(posedge clk) permutationGeneratorRST <= rst;
permutationGenerator67 permutationGen (
    .clk(clk),
    .rst(permutationGeneratorRST),
    
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

(* dont_merge *) reg computePipeRST; always @(posedge clk) computePipeRST <= rst;
// sums all 120 permutations of variables 2,3,4,5,6.
pipeline120Pack computePipe (
    .clk(clk),
    .clk2x(clk2x),
    .rst(computePipeRST),
    .activityMeasure(activityMeasure),
    
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

(* dont_merge *) reg outputRST; always @(posedge clk) outputRST <= rst;
// Extra delay needed to properly reset this fifo after other fifos in the system. Otherwise it gets messed up
wire outputRST_Delayed;
hyperpipe #(.CYCLES(10)) outputRSTPipe(clk, outputRST, outputRST_Delayed);

// Small stalling machine, to allow for propagation delay in grabNewResult and pipelineResultAvailable
reg[2:0] cyclesTillNextResultsGrabTry = 0; always @(posedge clk) cyclesTillNextResultsGrabTry <= cyclesTillNextResultsGrabTry + 1;
wire outputFIFOReadyForResults;
assign grabNewResult = outputFIFOReadyForResults && pipelineResultAvailable && (cyclesTillNextResultsGrabTry == 0);


wire grabNewResultArrived;
hyperpipe #(.CYCLES(8)) writePipeToRegister(clk, grabNewResult, grabNewResultArrived);

wire[4:0] outputFIFOUsedw;
// Expect output fifo to be far away from pipelines
hyperpipe #(.CYCLES(5)) outputFIFOReadyForResultsPipe(clk, outputFIFOUsedw < 20, outputFIFOReadyForResults);
wire outputFIFOEmpty; assign resultsAvailable = !outputFIFOEmpty;
FIFO #(.WIDTH(48+13), .DEPTH_LOG2(5)) outputFIFO (
    .clk(clk),
    .rst(outputRST_Delayed),
    
    // input side
    .writeEnable(grabNewResultArrived),
    .dataIn({pcoeffSumFromPipeline, pcoeffCountFromPipeline}),
    .usedw(outputFIFOUsedw),
    
    // output side
    .readEnable(grabResults),
    .dataOut({pcoeffSum, pcoeffCount}),
    .empty(outputFIFOEmpty)
);

endmodule
