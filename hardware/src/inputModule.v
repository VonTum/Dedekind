`timescale 1ns / 1ps

`include "pipelineGlobals_header.v"

`define FIFO_WIDTH 142
`define FIFO_DEPTH_LOG2 5
`define FIFO_ALMOSTFULL 28

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

`ifdef USE_FIFO_IP
pipelineFifo botQueue (
    .clock(clk),
    
    .wrreq(anyBotPermutIsValid),
    .data({bot,validBotPermutesIn,extraDataIn}),
    .full(_unused_full),
    
    .rdreq(popFifo & !fifoEmpty),
    .q(dataFromFifo),
    .empty(fifoEmpty),
	
	.usedw(fifoFullness)
);
`else
FIFO #(.WIDTH(128+6+EXTRA_DATA_WIDTH), .DEPTH_LOG2(`FIFO_DEPTH_LOG2)) botQueue (
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
`endif

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

module inputModule4 (
    input clk,
    
    // input side
    input[127:0] botA, // botB = varSwap(5,6)(A)
    input[127:0] botC, // botD = varSwap(5,6)(C)
    input[11:0] botIndex,
    input validBotA,
    input validBotB,
    input validBotC,
    input validBotD,
    output full,
    output almostFull,
    
    // output side, to countConnectedCore
    input dataRequested,
    output botOutAvailable,
    output[127:0] botOut,
    output[11:0] botOutIndex,
    output[1:0] botOutSubIndex
);

wire readEnable = dataRequested & botOutAvailable;

wire fullAB, fullCD;
wire emptyAB, emptyCD;
wire popABFifo, popCDFifo;
wire[`FIFO_WIDTH-1:0] fifoABData, fifoCDData;
wire[`FIFO_DEPTH_LOG2-1:0] usedwAB, usedwCD;

`ifdef USE_FIFO_IP
pipelineFifo botABFifo (
    .clock(clk),
    
    .wrreq(validBotA | validBotB),
    .data({botA,botIndex,validBotA,validBotB}),
    .full(fullAB),
    
    .rdreq(popABFifo),
    .q(fifoABData),
    .empty(emptyAB),
	
	.usedw(usedwAB)
);
pipelineFifo botCDFifo (
    .clock(clk),
    
    .wrreq(validBotC | validBotD),
    .data({botC,botIndex,validBotC,validBotD}),
    .full(fullCD),
    
    .rdreq(popCDFifo),
    .q(fifoCDData),
    .empty(emptyCD),
	
	.usedw(usedwCD)
);
`else
FIFO #(.WIDTH(`FIFO_WIDTH), .DEPTH_LOG2(`FIFO_DEPTH_LOG2)) botABFifo (
    .clk(clk),
    
    .writeEnable(validBotA | validBotB),
    .dataIn({botA,botIndex,validBotA,validBotB}),
    .full(fullAB),
    
    .readEnable(popABFifo),
    .dataOut(fifoABData),
    .empty(emptyAB),
	
	.usedw(usedwAB)
);

FIFO #(.WIDTH(`FIFO_WIDTH), .DEPTH_LOG2(`FIFO_DEPTH_LOG2)) botCDFifo (
    .clk(clk),
    
    .writeEnable(validBotC | validBotD),
    .dataIn({botC,botIndex,validBotC,validBotD}),
    .full(fullCD),
    
    .readEnable(popCDFifo),
    .dataOut(fifoCDData),
    .empty(emptyCD),
	
	.usedw(usedwCD)
);
`endif

wire almostFullAB = usedwAB >= `FIFO_ALMOSTFULL;
wire almostFullCD = usedwCD >= `FIFO_ALMOSTFULL;

assign full = fullAB | fullCD;
assign almostFull = almostFullAB | almostFullCD;

wire isFullerB = usedwCD >= usedwAB;

wire schedulerReadEnable;
wire schedulerDataAvailable;

wire[127:0] schedulerBotOut;
wire[11:0] schedulerBotIndexOut;
wire unswappedBotBelowOut;
wire swappedBotBelowOut;
wire chosenFifo;
fifoScheduler #(.FIFO_WIDTH(`FIFO_WIDTH)) scheduler(
    .clk(clk),
    .isFullerB(isFullerB),
	 
    .emptyA(emptyAB),
    .dataFromFifoA(fifoABData),
    .popFifoA(popABFifo),
    
    .emptyB(emptyCD),
    .dataFromFifoB(fifoCDData),
    .popFifoB(popCDFifo),
    
    .pop(schedulerReadEnable),
    .dataAvailable(schedulerDataAvailable),
    .chosenFifo(chosenFifo),
    .dataOut({schedulerBotOut, schedulerBotIndexOut, unswappedBotBelowOut, swappedBotBelowOut})
);

wire isSwappedVariant;
swapGraphMaker graphMaker(
    .clk(clk),
    
    .bot(schedulerBotOut),
    .unswappedBotValid(unswappedBotBelowOut),
    .swappedBotValid(swappedBotBelowOut),
    .dataInAvailable(schedulerDataAvailable),
    .readEnableBackend(schedulerReadEnable),
    
    .permutedBot(botOut),
    .readEnable(readEnable),
    .isSwappedVariant(isSwappedVariant),
    .dataAvailable(botOutAvailable)
);

reg[11:0] botOutIndexReg;
reg schedulerChoiceReg;
always @(posedge clk) begin
    if(schedulerReadEnable) begin
        botOutIndexReg <= schedulerBotIndexOut;
        schedulerChoiceReg <= chosenFifo;
    end
end
assign botOutIndex = botOutIndexReg;
assign botOutSubIndex = {schedulerChoiceReg, isSwappedVariant};

endmodule

module swapGraphMaker (
    input clk,
    
    input[127:0] bot,
    input unswappedBotValid,
    input swappedBotValid,
    input dataInAvailable,
    output readEnableBackend,
    
    output [127:0] permutedBot,
    input readEnable,
    output isSwappedVariant,
    output reg dataAvailable
);

reg[127:0] curBot;
reg doUnswapped, doSwapped;

wire twoLeft = doUnswapped & doSwapped;
wire tryingToRead = !dataAvailable | dataAvailable & !twoLeft & readEnable;
assign readEnableBackend = tryingToRead & dataInAvailable;

assign permutedBot[31:0] = curBot[31:0];
assign permutedBot[127:96] = curBot[127:96];
assign permutedBot[63:32] = doSwapped ? curBot[95:64] : curBot[63:32];
assign permutedBot[95:64] = doSwapped ? curBot[63:32] : curBot[95:64];

assign isSwappedVariant = doSwapped;

initial dataAvailable = 0;

always @ (posedge clk) begin
    if(readEnableBackend) begin
        curBot <= bot;
        doUnswapped <= unswappedBotValid;
        doSwapped <= swappedBotValid;
    end
    if(tryingToRead) begin
        dataAvailable <= dataInAvailable;
    end
    if(readEnable & twoLeft & doSwapped) begin
        doSwapped <= 0;
    end
end

endmodule

module fifoScheduler #(parameter FIFO_WIDTH = 8) (
    input clk,
	 input isFullerB,
    // fifo A
    input emptyA,
    input[FIFO_WIDTH-1:0] dataFromFifoA,
    output popFifoA,
    // fifo B
    input emptyB,
    input[FIFO_WIDTH-1:0] dataFromFifoB,
    output popFifoB,
    // output side
    input pop,
    output[FIFO_WIDTH-1:0] dataOut,
    output chosenFifo, // 1 for B, 0 for A
    output dataAvailable
);

wire isGrabbingFromFifo = pop & dataAvailable;
assign popFifoA = isGrabbingFromFifo & !chosenFifo; // chosenFifo == A
assign popFifoB = isGrabbingFromFifo & chosenFifo; // chosenFifo == B

assign dataOut = chosenFifo ? dataFromFifoB : dataFromFifoA;

reg previousChoice = 0;
reg previousPreviousChoice = 0;
always @(posedge clk) begin
    if(isGrabbingFromFifo) begin
        previousPreviousChoice <= previousChoice;
        previousChoice <= chosenFifo;
    end
end
fifoSchedulerDecider decider(emptyA, emptyB, isFullerB, previousChoice, previousPreviousChoice, dataAvailable, chosenFifo);

endmodule

module fifoSchedulerDecider (
    input emptyA,
    input emptyB,
	 input isFullerB,
    input previousChoice,
    input previousPreviousChoice,
    
    output hasDataAvailable,
    output chooseB
);

assign hasDataAvailable = !emptyA | !emptyB;

reg chooseBCombin;
assign chooseB = chooseBCombin;
always @(emptyA or emptyB or previousChoice or previousPreviousChoice or isFullerB) begin
    case ({emptyA,emptyB})
        2'b11: chooseBCombin <= 1'bX;
        2'b10: chooseBCombin <= 1;
        2'b01: chooseBCombin <= 0;
        2'b00: begin
            if(previousChoice == previousPreviousChoice) chooseBCombin <= !previousChoice;
				chooseBCombin <= isFullerB;
        end
    endcase
end

endmodule
