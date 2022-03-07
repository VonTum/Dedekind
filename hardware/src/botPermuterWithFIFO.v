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
    input[128*`NUMBER_OF_PERMUTATORS-1:0] bots,
    input[6*`NUMBER_OF_PERMUTATORS-1:0] validBotsPermutes,
    input[`NUMBER_OF_PERMUTATORS-1:0] batchesDone,
    output[`NUMBER_OF_PERMUTATORS-1:0] slowDownInputs,
    
    // Output side
    output[127:0] permutedBot,
    output permutedBotValid,
    output batchFinished,
    input requestSlowDown
);

wire[5:0] batchSizesOut[`NUMBER_OF_PERMUTATORS-1:0];

wor[127:0] botOutFromBatchFIFOs;
wor[5:0] botPermutesFromBatchFIFOs;
wor eccStatusFromBatchPseudoFIFOs;
wire[`NUMBER_OF_PERMUTATORS-1:0] batchFIFOSlowdown;
wire[`NUMBER_OF_PERMUTATORS-1:0] readFromFIFO;

genvar i;
generate
for(i = 0; i < `NUMBER_OF_PERMUTATORS; i = i + 1) begin
    (* dont_merge *) reg batchPseudoFIFORST; always @(posedge clk) batchPseudoFIFORST <= rst;
    BatchBotFIFO_M20K botAndPermutesFIFO (
        .clk(clk),
        .rst(batchPseudoFIFORST),
        
        // Input side
        .botIn(bots[128*i +: 128]),
        .validBotPermutesIn(validBotsPermutes[6*i +: 6]),
        .slowDownInput(batchFIFOSlowdown[i]),
        
        // Batch Control
        .endBatch(batchesDone[i]),
        .batchSize(batchSizesOut[i]),
        
        // Output Side
        .read(readFromFIFO[i]), // 4 cycle latency
        .botOut(botOutFromBatchFIFOs), 
        .validBotPermutesOut(botPermutesFromBatchFIFOs),
        .eccStatus(eccStatusFromBatchPseudoFIFOs)
    );
end
endgenerate

(* dont_merge *) reg batchSizeFIFORST; always @(posedge clk) batchSizeFIFORST <= rst;
wire[8:0] batchSizeFIFOUsedW;
reg batchSizeFIFOAlmostFull; always @(posedge clk) batchSizeFIFOAlmostFull <= batchSizeFIFOUsedW >= 450;
assign slowDownInputs = batchSizeFIFOAlmostFull ? {`NUMBER_OF_PERMUTATORS{1'b1}} : batchFIFOSlowdown;

// Communication with fifo
wire batchSizeFIFOEmpty;
wire[6+`NUMBER_OF_PERMUTATORS-1:0] dataFromBatchFIFO;
wire eccFromBatchFIFO;
wire dataFromBatchFIFOValid;
wire readFromBatchFIFO;

reg[$clog2(`NUMBER_OF_PERMUTATORS) - 1: 0] batchOriginIndex;

integer idx;
always @(*) begin
    batchOriginIndex = 0;
    for(idx = 0; idx < `NUMBER_OF_PERMUTATORS; idx = idx + 1) begin
        if((batchesDone & ({`NUMBER_OF_PERMUTATORS{1'b1}} << idx)) != {`NUMBER_OF_PERMUTATORS{1'b0}}) batchOriginIndex = idx;
    end
end

wire[5:0] batchSizeToBatchSizeFIFO = batchSizesOut[batchOriginIndex];

FastFIFO #(.WIDTH(6 + `NUMBER_OF_PERMUTATORS), .DEPTH_LOG2(9), .IS_MLAB(0)) batchSizeFIFO (
    .clk(clk),
    .rst(batchSizeFIFORST),
    
    // input side
    .writeEnable(|batchesDone),
    .dataIn({batchSizeToBatchSizeFIFO, batchesDone}),
    .usedw(batchSizeFIFOUsedW),
    
    // output side
    .readRequest(readFromBatchFIFO),
    .dataOut(dataFromBatchFIFO),
    .dataOutValid(dataFromBatchFIFOValid),
    .empty(batchSizeFIFOEmpty),
    .eccStatus(eccFromBatchFIFO)
);

reg[5:0] leftoverItemsInThisBatch;
reg[`NUMBER_OF_PERMUTATORS-1:0] selectedFIFO1Hot;
reg batchSizeECC;
wire thisBatchIsDone = leftoverItemsInThisBatch == 0;

wire nextBatchSizeAvailable;
wire grabNextBatchSize = thisBatchIsDone && nextBatchSizeAvailable;
wire[5:0] nextBatchSize;
wire nextBatchSizeECC;
wire[`NUMBER_OF_PERMUTATORS-1:0] nextSelectedFIFO1Hot;

(* dont_merge *) reg batchSizeFIFORegRST; always @(posedge clk) batchSizeFIFORegRST <= rst;
FastFIFOOutputReg #(6+`NUMBER_OF_PERMUTATORS+1) nextBatchSizeReg(clk, batchSizeFIFORegRST, 
    batchSizeFIFOEmpty, {dataFromBatchFIFO, eccFromBatchFIFO}, dataFromBatchFIFOValid, readFromBatchFIFO, // From FIFO
    grabNextBatchSize, nextBatchSizeAvailable, {{nextBatchSize, nextSelectedFIFO1Hot}, nextBatchSizeECC} // Output side
);

wire permuterRequestSlowDown;
wire pushPermutation = !thisBatchIsDone && !permuterRequestSlowDown;
assign readFromFIFO = pushPermutation ? selectedFIFO1Hot : 0;

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
            selectedFIFO1Hot <= nextSelectedFIFO1Hot;
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
