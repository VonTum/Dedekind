`timescale 1ns / 1ps

module fullPermutationPipeline(
    input clk,
    input clk2x,
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
reg[2:0] cyclesSinceGrabNewResult;
wire isReadyForNextGrab = cyclesSinceGrabNewResult == 7;
assign grabNewResult = !resultsAvailable && pipelineResultAvailable && isReadyForNextGrab;

always @(posedge clk) begin
    if(outputRST || grabNewResult) begin
        cyclesSinceGrabNewResult <= 0;
    end else begin
        if(!isReadyForNextGrab) begin
            cyclesSinceGrabNewResult <= cyclesSinceGrabNewResult + 1;
        end
    end
end

wire grabNewResultArrived;
hyperpipe #(.CYCLES(6)) writePipeToRegister(clk, grabNewResult, grabNewResultArrived);

pipelineRegister #(.WIDTH(48+13)) outputReg(
    .clk(clk),
    .rst(outputRST),
    
    .write(grabNewResultArrived),
    .dataIn({pcoeffSumFromPipeline, pcoeffCountFromPipeline}),
    
    .grab(grabResults),
    .hasData(resultsAvailable),
    .data({pcoeffSum, pcoeffCount})
);

endmodule
