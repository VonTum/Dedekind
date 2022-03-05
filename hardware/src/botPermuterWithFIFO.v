`timescale 1ns / 1ps

module botPermuterWithFIFO(
    input clk,
    input rst,
    
    // Input side
    input[127:0] bot,
    input writeData,
    input[5:0] validBotPermutes,
    input batchDone,
    output reg slowDownInput,
    
    // Output side
    output[127:0] permutedBot,
    output permutedBotValid,
    output reg batchFinished,
    input requestSlowDown // = aggregatingPipelineSlowDownInput || outputFIFORequestsSlowdown
);

wire[4:0] inputFifoUsedw;
always @(posedge clk) slowDownInput <= inputFifoUsedw > 16;

wire inputBotQueueEmpty;
wire botPermuterReadyForBot;
wire grabNew6Pack = botPermuterReadyForBot && !inputBotQueueEmpty && !requestSlowDown;

wire[127:0] botToPermuteFromFIFO;
wire[5:0] botFromFIFOValidBotPermutations;
wire batchDonePostFIFO;

(* dont_merge *) reg inputFIFORST; always @(posedge clk) inputFIFORST <= rst;
FIFO #(.WIDTH(128+6+1), .DEPTH_LOG2(5)) inputFIFO (
    .clk(clk),
    .rst(inputFIFORST),
    
    // input side
    .writeEnable(writeData && (|validBotPermutes || batchDone)),
    .dataIn({bot, validBotPermutes, batchDone}),
    .full(),
    .usedw(inputFifoUsedw),
    
    // output side
    .readEnable(grabNew6Pack),
    .dataOut({botToPermuteFromFIFO, botFromFIFOValidBotPermutations, batchDonePostFIFO}),
    .empty(inputBotQueueEmpty)
);

// 2 cycles of delay because that is the latency of the permutation generator
reg batchFinishedPreDelay; always @(posedge clk) batchFinishedPreDelay <= grabNew6Pack && batchDonePostFIFO;
always @(posedge clk) batchFinished <= batchFinishedPreDelay;

(* dont_merge *) reg permuter6RST; always @(posedge clk) permuter6RST <= rst;
// permutes the last 3 variables
botPermuter #(.EXTRA_DATA_WIDTH(0)) permuter6 (
    .clk(clk),
    .rst(permuter6RST),
    
    // input side
    .startNewBurst(grabNew6Pack),
    .botIn(botToPermuteFromFIFO),
    .validBotPermutesIn(botFromFIFOValidBotPermutations), // == {vABCin, vACBin, vBACin, vBCAin, vCABin, vCBAin}
    .extraDataIn(),
    .done(botPermuterReadyForBot),
    
    // output side
    .permutedBotValid(permutedBotValid),
    .permutedBot(permutedBot),
    .selectedPermutationOut(),
    .extraDataOut()
);

endmodule



module BatchBotFIFO_M20K (
    input clk,
    input rst,
    
    // Input side
    input[127:0] botIn,
    input[5:0] validBotPermutesIn,
    output reg slowDownInput,
    
    // Batch Control
    input endBatch,
    output[5:0] batchSize,
    
    // Output Side
    input read, // 4 cycle latency
    output[127:0] botOut, 
    output[5:0] validBotPermutesOut,
    output eccStatus
);

wire write = |validBotPermutesIn;

reg[5:0] batchSizeReg;
assign batchSize = batchSizeReg + write;

reg[8:0] writeAddr;
reg[8:0] readAddr;

wire[8:0] leftoverWords = readAddr - writeAddr;

always @(posedge clk) slowDownInput <= leftoverWords < 100;

always @(posedge clk) begin
    if(rst) begin
        writeAddr <= 0;
        batchSizeReg <= 0;
    end else begin
        writeAddr <= writeAddr + write;
        batchSizeReg <= endBatch ? 0 : batchSize;
    end
end

always @(posedge clk) begin
    if(rst) begin
        readAddr <= 9'b111111111;
    end else begin
        readAddr <= readAddr + read;
    end
end

reg readD; always @(posedge clk) readD <= read;

MEMORY_M20K #(
    .WIDTH(128+6), 
    .DEPTH_LOG2(9), 
    .READ_DURING_WRITE("DONT_CARE")
) memory (
    .clk(clk),
    
    // Write Side
    .writeEnable(write),
    .writeAddr(writeAddr),
    .dataIn({botIn, validBotPermutesIn}),
    
    // Read Side
    .readEnable(readD), // read enable is once cycle later for 
    .readAddressStall(1'b0),
    .readAddr(readAddr),
    .dataOut({botOut, validBotPermutesOut}),
    .eccStatus(eccStatus)
);

endmodule

module botPermuterWithMultiFIFO(
    input clk,
    input rst,
    
    // Input side
    input[127:0] bot,
    input[5:0] validBotPermutes,
    input batchDone,
    output slowDownInput,
    
    // Output side
    output[127:0] permutedBot,
    output permutedBotValid,
    output batchFinished,
    input requestSlowDown
);

wire[5:0] batchSizeOut;

wire pushPermutation;
wor[127:0] botOutFromBatchFIFOs;
wor[5:0] botPermutesFromBatchFIFOs;
wor eccStatusFromBatchPseudoFIFOs;
wire batchFIFOSlowdown;

(* dont_merge *) reg batchPseudoFIFORST; always @(posedge clk) batchPseudoFIFORST <= rst;
BatchBotFIFO_M20K botAndPermutesFIFO (
    .clk(clk),
    .rst(batchPseudoFIFORST),
    
    // Input side
    .botIn(bot),
    .validBotPermutesIn(validBotPermutes),
    .slowDownInput(batchFIFOSlowdown),
    
    // Batch Control
    .endBatch(batchDone),
    .batchSize(batchSizeOut),
    
    // Output Side
    .read(pushPermutation), // 4 cycle latency
    .botOut(botOutFromBatchFIFOs), 
    .validBotPermutesOut(botPermutesFromBatchFIFOs),
    .eccStatus(eccStatusFromBatchPseudoFIFOs)
);

(* dont_merge *) reg batchSizeFIFORST; always @(posedge clk) batchSizeFIFORST <= rst;
wire[8:0] batchSizeFIFOUsedW;
reg batchSizeFIFOAlmostFull; always @(posedge clk) batchSizeFIFOAlmostFull <= batchSizeFIFOUsedW >= 450;
assign slowDownInput = batchSizeFIFOAlmostFull || batchFIFOSlowdown;

// Communication with fifo
wire batchSizeFIFOEmpty;
wire[5:0] batchSizeFromBatchFIFO;
wire eccFromBatchFIFO;
wire dataFromBatchFIFOValid;
wire readFromBatchFIFO;

FastFIFO #(.WIDTH(6), .DEPTH_LOG2(9), .IS_MLAB(0)) batchSizeFIFO (
    .clk(clk),
    .rst(batchSizeFIFORST),
    
    // input side
    .writeEnable(batchDone),
    .dataIn(batchSizeOut),
    .usedw(batchSizeFIFOUsedW),
    
    // output side
    .readRequest(readFromBatchFIFO),
    .dataOut(batchSizeFromBatchFIFO),
    .dataOutValid(dataFromBatchFIFOValid),
    .empty(batchSizeFIFOEmpty),
    .eccStatus(eccFromBatchFIFO)
);

reg[5:0] leftoverItemsInThisBatch;
reg batchSizeECC;
wire thisBatchIsDone = leftoverItemsInThisBatch == 0;

wire nextBatchSizeAvailable;
wire grabNextBatchSize = thisBatchIsDone && nextBatchSizeAvailable;
wire[5:0] nextBatchSize;
wire nextBatchSizeECC;

(* dont_merge *) reg batchSizeFIFORegRST; always @(posedge clk) batchSizeFIFORegRST <= rst;
FastFIFOOutputReg #(6+1) nextBatchSizeReg(clk, batchSizeFIFORegRST, 
    batchSizeFIFOEmpty, {batchSizeFromBatchFIFO, eccFromBatchFIFO}, dataFromBatchFIFOValid, readFromBatchFIFO, // From FIFO
    grabNextBatchSize, nextBatchSizeAvailable, {nextBatchSize, nextBatchSizeECC} // Output side
);

wire permuterRequestSlowDown;
assign pushPermutation = !thisBatchIsDone && !permuterRequestSlowDown;
wire botDataArrives;
hyperpipe #(.CYCLES(4)) dataArrivesPipe(clk, pushPermutation, botDataArrives);
reg batchWasNotDone; always @(posedge clk) batchWasNotDone <= !thisBatchIsDone || grabNextBatchSize;
wire batchDoneArrives;
hyperpipe #(.CYCLES(4)) batchDoneArrivesPipe(clk, batchWasNotDone && thisBatchIsDone, batchDoneArrives);

always @(posedge clk) begin
    if(batchSizeFIFORegRST) begin
        leftoverItemsInThisBatch <= 0;
    end else begin
        if(grabNextBatchSize) begin
            leftoverItemsInThisBatch <= nextBatchSize;
            batchSizeECC <= nextBatchSizeECC;
        end
        if(pushPermutation) begin
            leftoverItemsInThisBatch <= leftoverItemsInThisBatch - 1;
        end
    end
end

botPermuterWithFIFO permuter (
    .clk(clk),
    .rst(rst),
    
    // Input side
    .writeData(botDataArrives || batchDoneArrives),
    .bot(botOutFromBatchFIFOs),
    .validBotPermutes(botPermutesFromBatchFIFOs),
    .batchDone(batchDoneArrives),
    .slowDownInput(permuterRequestSlowDown),
    
    // Output side
    .permutedBot(permutedBot),
    .permutedBotValid(permutedBotValid),
    .batchFinished(batchFinished),
    .requestSlowDown(requestSlowDown)
);

endmodule
