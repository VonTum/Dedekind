`timescale 1ns / 1ps

`include "pipelineGlobals_header.v"

module fullPermutationPipeline(
    input clk,
    input clk2x,
    input rst,
    output[5:0] activityMeasure, // Instrumentation wire for profiling (0-40 activity level)
    
    input[1:0] topChannel,
    
    // Input side
    input[127:0] bot,
    input writeBot,
    output readyForInputBot,
    
    // Output side
    input slowDown,
    output resultValid,
    output[47:0] pcoeffSum,
    output[12:0] pcoeffCount,
    output eccStatus
);



wire[128*`NUMBER_OF_PERMUTATORS-1:0] permutedBots;
wire[`NUMBER_OF_PERMUTATORS-1:0] permutedBotsValid;
wire[`NUMBER_OF_PERMUTATORS-1:0] batchesDone;
wire[`NUMBER_OF_PERMUTATORS-1:0] pipelineRequestsSlowdown;

(* dont_merge *) reg permutationGeneratorRST; always @(posedge clk) permutationGeneratorRST <= rst;
/*oldPermutationGenerator67 permutationGen (
    .clk(clk),
    .rst(permutationGeneratorRST),
    
    .inputBot(bot),
    .writeInputBot(writeBot),
    .hasSpaceForNextBot(readyForInputBot),
    
    .outputBot(permutedBots),
    .outputBotValid(permutedBotsValid),
    .botSeriesFinished(batchesDone)
    //,.slowDownPermutationProduction(pipelineRequestsSlowdown)
);

wire readyForInputBotMULTI;
wire[128*`NUMBER_OF_PERMUTATORS-1:0] permutedBotsMULTI;
wire[`NUMBER_OF_PERMUTATORS-1:0] permutedBotsValidMULTI;
wire[`NUMBER_OF_PERMUTATORS-1:0] batchesDoneMULTI;


multiPermutationGenerator67 permutationGenMULTI (
    .clk(clk),
    .rst(permutationGeneratorRST),
    
    .inputBot(bot),
    .writeInputBot(writeBot),
    .hasSpaceForNextBot(readyForInputBotMULTI),
    
    .outputBots(permutedBotsMULTI),
    .outputBotsValid(permutedBotsValidMULTI),
    .botSeriesFinished(batchesDoneMULTI),
    .slowDownPermutationProduction(pipelineRequestsSlowdown)
);
*/

multiPermutationGenerator67 permutationGenMULTI (
    .clk(clk),
    .clk2x(clk2x),
    .rst(permutationGeneratorRST),
    
    .inputBot(bot),
    .writeInputBot(writeBot),
    .hasSpaceForNextBot(readyForInputBot),
    
    .outputBots(permutedBots),
    .outputBotsValid(permutedBotsValid),
    .botSeriesFinished(batchesDone),
    .slowDownPermutationProduction(pipelineRequestsSlowdown)
);


wire grabNewResult;
wire pipelineResultAvailable;

(* dont_merge *) reg[1:0] topChannelD; always @(posedge clk) topChannelD <= topChannel;

(* dont_merge *) reg computePipeRST; always @(posedge clk) computePipeRST <= rst;
// sums all 120 permutations of variables 2,3,4,5,6.
pipeline120Pack pipeline120 (
    .clk(clk),
    .clk2x(clk2x),
    .rst(computePipeRST),
    .activityMeasure(activityMeasure),
    
    .topChannel(topChannelD),
    .bots(permutedBots),
    .isBotsValid(permutedBotsValid),
    .batchesDone(batchesDone),
    .slowDownInput(pipelineRequestsSlowdown),
    
    // Output side
    .grabResults(grabNewResult),
    .resultsAvailable(pipelineResultAvailable),
    .pcoeffSum(pcoeffSum),
    .pcoeffCount(pcoeffCount),
    .eccStatus(eccStatus)
);

(* dont_merge *) reg outputRST; always @(posedge clk) outputRST <= rst;

// Small stalling machine, to allow for propagation delay in grabNewResult and pipelineResultAvailable
reg[1:0] cyclesTillNextResultsGrabTry = 0; always @(posedge clk) cyclesTillNextResultsGrabTry <= cyclesTillNextResultsGrabTry + 1;
wire outputFIFOReadyForResults;
assign grabNewResult = outputFIFOReadyForResults && pipelineResultAvailable && (cyclesTillNextResultsGrabTry == 0);


hyperpipe #(.CYCLES(6)) writePipeToRegister(clk, grabNewResult, resultValid);

// Expect output fifo to be far away from pipelines
hyperpipe #(.CYCLES(5)) outputFIFOReadyForResultsPipe(clk, !slowDown, outputFIFOReadyForResults);

endmodule



module fullPermutationPipeline30 (
    input clk,
    input clk2x,
    input rst,
    input longRST,
    output reg[5:0] activityMeasure, // Instrumentation wire for profiling (0-60 activity level)
    
    input[1:0] topChannel,
    
    // Input side
    input[127:0] bot,
    input writeBot,
    output readyForInputBot,
    
    // Output side
    input slowDown,
    output resultValid,
    output reg[47:0] pcoeffSum,
    output reg[12:0] pcoeffCount,
    output reg eccStatus
);

wire permutationGeneratorIsAlmostFull;
assign readyForInputBot = !permutationGeneratorIsAlmostFull;

reg pipelinesRequestSlowdown;
wire[127:0] botFromGen;
wire botFromGenValid;
wire botFromGenSeriesFinished;

wire permutationGeneratorECCStatus;
permutationGenerator7 permutationGenerator7 (
    .clk(clk),
    .rst(rst),
    
    .inputBot(bot),
    .writeInputBot(writeBot),
    .almostFull(permutationGeneratorIsAlmostFull),
    
    .slowDown(pipelinesRequestSlowdown),
    .outputBot(botFromGen),
    .outputBotValid(botFromGenValid),
    .botSeriesFinished(botFromGenSeriesFinished),
    .eccStatus(permutationGeneratorECCStatus)
);

wire[127:0] top;
topReceiver topReceiver(clk, topChannel, top);
wire[30*24-1:0] validBotPermutations_VALID;
permuteCheck720Pipelined permuteCheck720Pipelined (
    .clk(clk),
    .top(top),
    .bot(botFromGen),
    .isBotValid(botFromGenValid),
    .validBotPermutations(validBotPermutations_VALID) // 30 sets of 24 valid bot permutations
);

`define MULTI_PERMUTE_CHECK_LATENCY 3

wire[127:0] botFromGen_VALID;
hyperpipe #(.CYCLES(`MULTI_PERMUTE_CHECK_LATENCY), .WIDTH(128)) permutedBotPipe(clk, botFromGen, botFromGen_VALID);
wire botFromGenSeriesFinished_VALID;
hyperpipe #(.CYCLES(`MULTI_PERMUTE_CHECK_LATENCY)) botFromGenSeriesFinishedPipe(clk, botFromGenSeriesFinished, botFromGenSeriesFinished_VALID);

`include "inlineVarSwap_header.v"

// Generate bot permutations, MUST MATCH multiPermutChecks.v
wire[127:0] botPermutations[29:0];
generate
assign botPermutations[0] = botFromGen_VALID;
`VAR_SWAP_INLINE_WITHIN_GENERATE(1, 2, botPermutations[0], botPermutations[5])
`VAR_SWAP_INLINE_WITHIN_GENERATE(1, 3, botPermutations[0], botPermutations[10])
`VAR_SWAP_INLINE_WITHIN_GENERATE(1, 4, botPermutations[0], botPermutations[15])
`VAR_SWAP_INLINE_WITHIN_GENERATE(1, 5, botPermutations[0], botPermutations[20])
`VAR_SWAP_INLINE_WITHIN_GENERATE(1, 6, botPermutations[0], botPermutations[25])

for(genvar i = 0; i < 6; i = i + 1) begin
    `VAR_SWAP_INLINE_WITHIN_GENERATE(2, 3, botPermutations[5*i], botPermutations[5*i+1])
    `VAR_SWAP_INLINE_WITHIN_GENERATE(2, 4, botPermutations[5*i], botPermutations[5*i+2])
    `VAR_SWAP_INLINE_WITHIN_GENERATE(2, 5, botPermutations[5*i], botPermutations[5*i+3])
    `VAR_SWAP_INLINE_WITHIN_GENERATE(2, 6, botPermutations[5*i], botPermutations[5*i+4])
end

wire[29:0] slowDownWOR;
always @(posedge clk) pipelinesRequestSlowdown <= |slowDownWOR;

wire[1:0] activityMeasures[29:0];
wire[$clog2(24*7)+35-1:0] pcoeffSums[29:0];
wire[$clog2(24*7)-1:0] pcoeffCounts[29:0];

wire[29:0] eccStatuses;
wire[29:0] memoryEccStatuses;
always @(posedge clk) eccStatus <= permutationGeneratorECCStatus || |eccStatuses || |memoryEccStatuses;

wire[29:0] outputFIFOWrites;
wire[30*9-1:0] writeAddrs;
wire massFIFORead;
wire[8:0] readAddr;

(* dont_merge *) reg mssLongRST; always @(posedge clk) mssLongRST <= longRST;
MultiStreamSynchronizer #(.DEPTH_LOG2(9), .NUMBER_OF_FIFOS(30)) multiStreamSynchronizer (
    .clk(clk),
    .rst(mssLongRST),
    
    // Write side
    .writes(outputFIFOWrites),
    .writeAddrs(writeAddrs),
    
    // Read side
    .slowDown(slowDown),
    .readEnable(massFIFORead),
    .readAddr(readAddr)
);

for(genvar permutationI = 0; permutationI < 30; permutationI = permutationI + 1) begin
    (* dont_merge *) reg pipelineRST; always @(posedge clk) pipelineRST <= rst;
    (* dont_merge *) reg pipelineLongRST; always @(posedge clk) pipelineLongRST <= longRST;
    
    wire[$clog2(24*7)+35-1:0] sumFromPipeline;
    wire[$clog2(24*7)-1:0] countFromPipeline;
    aggregatingPermutePipeline24 #(.PCOEFF_COUNT_BITWIDTH($clog2(24*7))) pipeline (
        .clk(clk),
        .clk2x(clk2x),
        .rst(pipelineRST),
        .longRST(pipelineLongRST),
        .topChannel(topChannel),
        .activityMeasure(activityMeasures[permutationI]), // Instrumentation wire for profiling (0-2 activity level)
        
        // Input side
        .botIn(botPermutations[permutationI]),
        .validBotPermutes(validBotPermutations_VALID[24*permutationI +: 24]),
        .batchDone(botFromGenSeriesFinished_VALID),
        .almostFull(slowDownWOR[permutationI]),
        
        // Output side
        .slowDown(slowDown),
        .resultValid(outputFIFOWrites[permutationI]),
        .pcoeffSum(sumFromPipeline),
        .pcoeffCount(countFromPipeline),
        
        .eccStatus(eccStatuses[permutationI])
    );
    
    MEMORY_M20K #(.WIDTH($clog2(24*7)+35+$clog2(24*7)), .DEPTH_LOG2(9)) pipelineOutFIFO (
        .clk(clk),
        
        // Write Side
        .writeEnable(outputFIFOWrites[permutationI]),
        .writeAddr(writeAddrs[9*permutationI +: 9]),
        .dataIn({sumFromPipeline, countFromPipeline}),
        
        // Read Side
        .readEnable(massFIFORead),
        .readAddr(readAddr),
        .dataOut({pcoeffSums[permutationI], pcoeffCounts[permutationI]}),
        .eccStatus(memoryEccStatuses[permutationI])
    );
end

reg[2:0] activityMeasures15[14:0];
reg[$clog2(24*7*2)+35-1:0] pcoeffSums15[14:0];
reg[$clog2(24*7*2)-1:0] pcoeffCounts15[14:0];

for(genvar i = 0; i < 15; i = i + 1) begin
    always @(posedge clk) begin
        activityMeasures15[i] <= activityMeasures[2*i] + activityMeasures[2*i+1];
        pcoeffSums15[i] <= pcoeffSums[2*i] + pcoeffSums[2*i+1];
        pcoeffCounts15[i] <= pcoeffCounts[2*i] + pcoeffCounts[2*i+1];
    end
end

reg[3:0] activityMeasures8[7:0];
reg[$clog2(24*7*2*2)+35-1:0] pcoeffSums8[7:0];
reg[$clog2(24*7*2*2)-1:0] pcoeffCounts8[7:0];

for(genvar i = 0; i < 7; i = i + 1) begin
    always @(posedge clk) begin
        activityMeasures8[i] <= activityMeasures15[2*i] + activityMeasures15[2*i+1];
        pcoeffSums8[i] <= pcoeffSums15[2*i] + pcoeffSums15[2*i+1];
        pcoeffCounts8[i] <= pcoeffCounts15[2*i] + pcoeffCounts15[2*i+1];
    end
end
always @(posedge clk) begin
    activityMeasures8[7] <= activityMeasures15[14];
    pcoeffSums8[7] <= pcoeffSums15[14];
    pcoeffCounts8[7] <= pcoeffCounts15[14];
end

reg[4:0] activityMeasures4[3:0];
reg[$clog2(24*7*2*2*2)+35-1:0] pcoeffSums4[3:0];
reg[$clog2(24*7*2*2*2)-1:0] pcoeffCounts4[3:0];

for(genvar i = 0; i < 4; i = i + 1) begin
    always @(posedge clk) begin
        activityMeasures4[i] <= activityMeasures8[2*i] + activityMeasures8[2*i+1];
        pcoeffSums4[i] <= pcoeffSums8[2*i] + pcoeffSums8[2*i+1];
        pcoeffCounts4[i] <= pcoeffCounts8[2*i] + pcoeffCounts8[2*i+1];
    end
end

reg[5:0] activityMeasures2[1:0];
reg[$clog2(24*7*2*2*2*2)+35-1:0] pcoeffSums2[1:0];
reg[$clog2(24*7*2*2*2*2)-1:0] pcoeffCounts2[1:0];

for(genvar i = 0; i < 2; i = i + 1) begin
    always @(posedge clk) begin
        activityMeasures2[i] <= activityMeasures4[2*i] + activityMeasures4[2*i+1];
        pcoeffSums2[i] <= pcoeffSums4[2*i] + pcoeffSums4[2*i+1];
        pcoeffCounts2[i] <= pcoeffCounts4[2*i] + pcoeffCounts4[2*i+1];
    end
end

endgenerate

always @(posedge clk) begin
    activityMeasure <= activityMeasures2[0] + activityMeasures2[1];
    pcoeffSum <= pcoeffSums2[0] + pcoeffSums2[1];
    pcoeffCount <= pcoeffCounts2[0] + pcoeffCounts2[1];
end

`define M20K_READ_LATENCY 3
`define SUM_PIPELINE_STAGES 5
hyperpipe #(.CYCLES(`M20K_READ_LATENCY + `SUM_PIPELINE_STAGES)) resultArrivesPipe(clk, massFIFORead, resultValid);

endmodule


