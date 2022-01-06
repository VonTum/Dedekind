`timescale 1ns / 1ps

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
    input requestBot,
    output[127:0] botOut,
    output botOutValid,
    output[2:0] selectedBotPermutation,
    output[EXTRA_DATA_WIDTH-1:0] extraDataOut
);

wire fifoEmpty;
wire popFifo;
wire[128+EXTRA_DATA_WIDTH+6-1:0] dataFromFifo;

FIFO #(.WIDTH(128+6+EXTRA_DATA_WIDTH), .DEPTH_LOG2(5 /*Size 32*/)) botQueue (
    .clk(clk),
    .rst(rst),
    
    .writeEnable(anyBotPermutIsValid),
    .dataIn({bot,validBotPermutesIn,extraDataIn}),
    .full(_unused_full),
    
    .readEnable(popFifo & !fifoEmpty),
    .dataOut(dataFromFifo),
    .empty(fifoEmpty),
	 
    .usedw(fifoFullness)
);

wire fifoDataAvailable = !fifoEmpty;
wire[127:0] botFromFifo;
wire[5:0] validBotPermutesFromFifo;// == {vABCin, vACBin, vBACin, vBCAin, vCABin, vCBAin}
wire[EXTRA_DATA_WIDTH-1:0] extraDataFromFifo;

assign {botFromFifo, validBotPermutesFromFifo, extraDataFromFifo} = dataFromFifo;

botPermuter #(.EXTRA_DATA_WIDTH(EXTRA_DATA_WIDTH)) permuter (
    .clk(clk),
    .rst(rst),
    
    // input side
    .botIn(botFromFifo),
    .botInIsValid(fifoDataAvailable),
    .validBotPermutesIn(validBotPermutesFromFifo), 
    .extraDataIn(extraDataFromFifo),
    .requestBotFromInput(popFifo),
    
    // output side
    .requestBot(requestBot),
    .permutedBotValid(botOutValid),
    .permutedBot(botOut),
    .selectedPermutationOut(selectedBotPermutation),
    .extraDataOut(extraDataOut)
);

endmodule
