`timescale 1ns / 1ps

module popcnt6 (
    input[5:0] data,
    
    output[2:0] count
);

wire[1:0] partA = data[0] + data[1] + data[2];
wire[1:0] partB = data[3] + data[4] + data[5];

assign count = partA + partB;

endmodule

module inputModule6 #(parameter EXTRA_DATA_WIDTH = 12) (
    input clk,
    input rst,
    
    // input side
    input[127:0] bot,
    input anyBotPermutIsValid, // == botIsValid & |validBotPermutesIn
    input[5:0] validBotPermutesIn, // == {vABCin, vACBin, vBACin, vBCAin, vCABin, vCBAin}
    input[EXTRA_DATA_WIDTH-1:0] extraDataIn,
    output[4:0] fifoFullness,
    
    // output side
    input readyForBotBurst,
    output[127:0] botOut,
    output botOutValid,
    output[2:0] selectedBotPermutation,
    output[EXTRA_DATA_WIDTH-1:0] extraDataOut
);

wire rstLocal; // Manual reset tree, can't use constraints to have it generate it for me. 
hyperpipe #(.CYCLES(2)) rstPipe(clk, rst, rstLocal);


reg[2:0] cyclesUntilNextBotRequest;
wire readyForBotRequest = cyclesUntilNextBotRequest == 0;
reg popFifoDataMayBeAvailableNow;
wire popFifo = readyForBotBurst && readyForBotRequest && !popFifoDataMayBeAvailableNow;
always @(posedge clk) popFifoDataMayBeAvailableNow <= rstLocal ? 0 : popFifo;
wire fifoDataValidPreDelay;
wire[128+EXTRA_DATA_WIDTH+6-1:0] dataFromFifoPreDelay;

FastFIFO #(.WIDTH(128+6+EXTRA_DATA_WIDTH), .READ_ADDR_STAGES(1)) botQueue (
    .clk(clk),
    .rst(rst),
    
    .writeEnable(anyBotPermutIsValid),
    .dataIn({bot,validBotPermutesIn,extraDataIn}),
    .usedw(fifoFullness),
    
    .readRequest(popFifo),
    .dataOut(dataFromFifoPreDelay),
    .dataOutValid(fifoDataValidPreDelay)
);
reg fifoDataValid; always @(posedge clk) if(popFifoDataMayBeAvailableNow) fifoDataValid <= fifoDataValidPreDelay;
reg[128+EXTRA_DATA_WIDTH+6-1:0] dataFromFifo; always @(posedge clk) if(popFifoDataMayBeAvailableNow) dataFromFifo <= dataFromFifoPreDelay;
wire startNewBurst = fifoDataValid & popFifoDataMayBeAvailableNow;

wire[127:0] botFromFifo;
wire[5:0] validBotPermutesFromFifo;// == {vABCin, vACBin, vBACin, vBCAin, vCABin, vCBAin}
wire[EXTRA_DATA_WIDTH-1:0] extraDataFromFifo;
assign {botFromFifo, validBotPermutesFromFifo, extraDataFromFifo} = dataFromFifo;


wire[2:0] validBotPermutesPopCnt;
popcnt6 permuteCounter(validBotPermutesFromFifo, validBotPermutesPopCnt);
always @(posedge clk) begin
    if(rstLocal)
        cyclesUntilNextBotRequest <= 0;
    else if(startNewBurst)
        cyclesUntilNextBotRequest <= validBotPermutesPopCnt;
    else if(!readyForBotRequest)
        cyclesUntilNextBotRequest <= cyclesUntilNextBotRequest - 1;
end


botPermuter #(.EXTRA_DATA_WIDTH(EXTRA_DATA_WIDTH)) permuter (
    .clk(clk),
    .rst(rst),
    
    // input side
    .startNewBurst(startNewBurst),
    .botIn(botFromFifo),
    .validBotPermutesIn(validBotPermutesFromFifo), 
    .extraDataIn(extraDataFromFifo),
    
    // output side
    .permutedBotValid(botOutValid),
    .permutedBot(botOut),
    .selectedPermutationOut(selectedBotPermutation),
    .extraDataOut(extraDataOut)
);

endmodule
