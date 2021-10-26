`timescale 1ns / 1ps
`define FIFO_WIDTH 142
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

wire fullAB, almostFullAB;
wire emptyAB, almostEmptyAB;
wire popABFifo;
wire[`FIFO_WIDTH-1:0] fifoABData;

FIFO #(.WIDTH(`FIFO_WIDTH)) botABFifo (
    .wClk(clk),.rClk(clk),
    
    .writeEnable(validBotA | validBotB),
    .dataIn({botA,botIndex,validBotA,validBotB}),
    .full(fullAB), .almostFull(almostFullAB),
    
    .readEnable(popABFifo),
    .dataOut(fifoABData),
    .empty(emptyAB), .almostEmpty(almostEmptyAB)
);


wire fullCD, almostFullCD;
wire emptyCD, almostEmptyCD;
wire popCDFifo;
wire[`FIFO_WIDTH-1:0] fifoCDData;

FIFO #(.WIDTH(`FIFO_WIDTH)) botCDFifo (
    .wClk(clk),.rClk(clk),
    
    .writeEnable(validBotC | validBotD),
    .dataIn({botC,botIndex,validBotC,validBotD}),
    .full(fullCD), .almostFull(almostFullCD),
    
    .readEnable(popCDFifo),
    .dataOut(fifoCDData),
    .empty(emptyCD), .almostEmpty(almostEmptyCD)
);

assign full = fullAB | fullCD;
assign almostFull = almostFullAB | almostFullCD;

wire schedulerReadEnable;
wire schedulerDataAvailable;

wire[127:0] schedulerBotOut;
wire[11:0] schedulerBotIndexOut;
wire unswappedBotBelowOut;
wire swappedBotBelowOut;
wire chosenFifo;
fifoScheduler #(.FIFO_WIDTH(`FIFO_WIDTH)) scheduler(
    .clk(clk),
    
    .emptyA(emptyAB),
    .almostEmptyA(almostEmptyAB),
    .dataFromFifoA(fifoABData),
    .popFifoA(popABFifo),
    
    .emptyB(emptyCD),
    .almostEmptyB(almostEmptyCD),
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
    // fifo A
    input emptyA,
    input almostEmptyA,
    input[FIFO_WIDTH-1:0] dataFromFifoA,
    output popFifoA,
    // fifo B
    input emptyB,
    input almostEmptyB,
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
fifoSchedulerDecider decider(emptyA, almostEmptyA, emptyB, almostEmptyB, previousChoice, previousPreviousChoice, dataAvailable, chosenFifo);

endmodule

module fifoSchedulerDecider (
    input emptyA,
    input almostEmptyA,
    
    input emptyB,
    input almostEmptyB,
    
    input previousChoice,
    input previousPreviousChoice,
    
    output hasDataAvailable,
    output chooseB
);

assign hasDataAvailable = !emptyA | !emptyB;

reg chooseBCombin;
assign chooseB = chooseBCombin;
always @(emptyA or emptyB or previousChoice or previousPreviousChoice or almostEmptyA or almostEmptyB) begin
    case ({emptyA,emptyB})
        2'b11: chooseBCombin <= 1'bX;
        2'b10: chooseBCombin <= 1;
        2'b01: chooseBCombin <= 0;
        2'b00: begin
            if(previousChoice == previousPreviousChoice) chooseBCombin <= !previousChoice;
            else case ({almostEmptyA, almostEmptyB})
                2'b01: chooseBCombin <= 0;
                2'b10: chooseBCombin <= 1;
                default: chooseBCombin <= !previousChoice;
            endcase
        end
    endcase
end

endmodule
