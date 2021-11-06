`timescale 1ns / 1ps
`define FIFO_WIDTH 142
`define FIFO_DEPTH_LOG2 5
`define FIFO_ALMOSTFULL 28

`ifdef ALTERA_RESERVED_QIS
`define USE_FIFO_IP
`endif

//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 08/25/2021 02:19:14 AM
// Design Name: 
// Module Name: graphCollector
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: 
// 
// Dependencies: 
// 
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
// 
//////////////////////////////////////////////////////////////////////////////////


module inputModule (
    input clk,
    
    // input side
    input[127:0] top,
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
    input readEnable,
    output graphAvailable,
    output[127:0] graphOut,
    output[11:0] graphIndex,
    output[1:0] graphSubIndex
);

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
    
    .top(top),
    .bot(schedulerBotOut),
    .unswappedBotValid(unswappedBotBelowOut),
    .swappedBotValid(swappedBotBelowOut),
    .dataInAvailable(schedulerDataAvailable),
    .readEnableBackend(schedulerReadEnable),
    
    .graph(graphOut),
    .readEnable(readEnable),
    .isSwappedVariant(isSwappedVariant),
    .dataAvailable(graphAvailable)
);

reg[11:0] graphIndexReg;
reg schedulerChoiceReg;
always @(posedge clk) begin
    if(schedulerReadEnable) begin
        graphIndexReg <= schedulerBotIndexOut;
        schedulerChoiceReg <= chosenFifo;
    end
end
assign graphIndex = graphIndexReg;
assign graphSubIndex = {schedulerChoiceReg, isSwappedVariant};

endmodule

module swapGraphMaker (
    input clk,
    
    input[127:0] top,
    input[127:0] bot,
    input unswappedBotValid,
    input swappedBotValid,
    input dataInAvailable,
    output readEnableBackend,
    
    output [127:0] graph,
    input readEnable,
    output isSwappedVariant,
    output reg dataAvailable
);

reg[127:0] curBot;
reg doUnswapped, doSwapped;

wire twoLeft = doUnswapped & doSwapped;
wire tryingToRead = !dataAvailable | dataAvailable & !twoLeft & readEnable;
assign readEnableBackend = tryingToRead & dataInAvailable;

wire[127:0] resultingBot;
assign resultingBot[31:0] = curBot[31:0];
assign resultingBot[127:96] = curBot[127:96];
assign resultingBot[63:32] = doSwapped ? curBot[95:64] : curBot[63:32];
assign resultingBot[95:64] = doSwapped ? curBot[63:32] : curBot[95:64];

assign graph = top & ~resultingBot;
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
