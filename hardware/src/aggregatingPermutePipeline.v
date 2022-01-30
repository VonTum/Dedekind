`timescale 1ns / 1ps

module aggregatingPermutePipeline #(parameter PCOEFF_COUNT_BITWIDTH = 10) (
    input clk,
    input rst,
    input[127:0] top,
    
    input[5:0] validBotPermutes,
    input[127:0] bot,
    input batchDone,
    output[4:0] inputFifoUsedw,
    
    input grabResults,
    output resultsAvaiblable,
    output[PCOEFF_COUNT_BITWIDTH+35-1:0] pcoeffSum,
    output[PCOEFF_COUNT_BITWIDTH-1:0] pcoeffCount
    
    // TODO
    //output eccStatus
);

wire[8:0] outputFIFOUsedw;
wire aggregatingPipelineSlowDownInput;
wire inputBotQueueEmpty;
wire botPermuterRequestsNewBot;
wire grabNew6Pack = botPermuterRequestsNewBot && !inputBotQueueEmpty && !aggregatingPipelineSlowDownInput && !(outputFIFOUsedw > 200); // TODO see if this is enough?
wire[127:0] botToPermute;
wire[5:0] botToPermuteValidPermutations;
wire batchDonePostFIFO;

FIFO #(.WIDTH(128+6+1), .DEPTH_LOG2(5)) inputFIFO (
    .clk(clk),
    .rst(rst),
     
    // input side
    .writeEnable(|validBotPermutes),
    .dataIn({bot, validBotPermutes, batchDone}),
    .full(),
    .usedw(inputFifoUsedw),
    
    // output side
    .readEnable(grabNew6Pack),
    .dataOut({botToPermute, botToPermuteValidPermutations, batchDonePostFIFO}),
    .empty(inputBotQueueEmpty)
);

wire permutedBotValid;
wire[127:0] permutedBot;

// permutes the last 3 variables
botPermuter #(.EXTRA_DATA_WIDTH(0)) permuter6 (
    .clk(clk),
    .rst(rst),
    
    // input side
    .startNewBurst(grabNew6Pack),
    .botIn(botToPermute),
    .validBotPermutesIn(botToPermuteValidPermutations), // == {vABCin, vACBin, vBACin, vBCAin, vCABin, vCBAin}
    .extraDataIn(),
    .done(botPermuterRequestsNewBot),
    
    // output side
    .permutedBotValid(permutedBotValid),
    .permutedBot(permutedBot),
    .selectedPermutationOut(),
    .extraDataOut()
);

wire botAvailableForAggregatingPipeline = grabNew6Pack && batchDonePostFIFO;

wire aggregateFinished;
wire[PCOEFF_COUNT_BITWIDTH+35-1:0] pcoeffSumFromPipeline;
wire[PCOEFF_COUNT_BITWIDTH-1:0] pcoeffCountFromPipeline;
wire aggrPipelineECC;
aggregatingPipeline #(.PCOEFF_COUNT_BITWIDTH(PCOEFF_COUNT_BITWIDTH)) computePipe (
    .clk(clk),
    .rst(rst),
    .top(top),
    
    .isBotValid(permutedBotValid),
    .bot(permutedBot),
    .lastBotOfBatch(botAvailableForAggregatingPipeline),
    .slowDownInput(aggregatingPipelineSlowDownInput),
    
    .resultsValid(aggregateFinished),
    .pcoeffSum(pcoeffSumFromPipeline),
    .pcoeffCount(pcoeffCountFromPipeline),
    
    .eccStatus(aggrPipelineECC)
);

wire resultsFIFOEmpty;
assign resultsAvaiblable = !resultsFIFOEmpty;
FIFO #(.WIDTH(PCOEFF_COUNT_BITWIDTH+35 + PCOEFF_COUNT_BITWIDTH), .DEPTH_LOG2(9)) resultsFIFO (
    .clk(clk),
    .rst(rst),
     
    // input side
    .writeEnable(aggregateFinished),
    .dataIn({pcoeffSumFromPipeline, pcoeffCountFromPipeline}),
    .full(),
    .usedw(outputFIFOUsedw),
    
    // output side
    .readEnable(grabResults),
    .dataOut({pcoeffSum, pcoeffCount}),
    .empty(resultsFIFOEmpty)
);


endmodule
