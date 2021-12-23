`timescale 1ns / 1ps
`include "pipelineGlobals_header.v"

module openCLFullPipeline (
    input clock,
    input clock2x, // apparently this specific name gives access to a 2x speed clock. Very useful!
    input resetn,
	input ivalid, 
	input iready,
	output ovalid,
	output oready,
    
    // we reuse bot to set the top, to save on inputs. 
    input startNewTop,
    input[63:0] botLower, // Represents all final 3 var swaps
    input[63:0] botUpper, // Represents all final 3 var swaps
    output[63:0] summedDataPcoeffCountOut   // first 16 bits pcoeffCountOut, last 48 bits summedDataOut
);

// TEMP
reg LAST_RESORT_ACT = 0;


wire[127:0] bot = {botUpper, botLower};
wire[127:0] top;

wire rst;
resetNormalizer rstNormalizer(clock, resetn, rst);

wire[4:0] fifoFullness;
wire[`ADDR_WIDTH-1:0] botIndex;
wire isBotValid;

wire pipelineResultValid;
wire slowThePipeline;

wire pipelineManagerIsReadyForBotIn;
assign oready = (pipelineManagerIsReadyForBotIn && !slowThePipeline) || LAST_RESORT_ACT;

pipelineManager pipelineMngr(
    .clk(clock),
    .rst(rst),
    
    .startNewTop(startNewTop),
    .topIn(bot), // Reuse bot for topIn
    .isBotInValid(ivalid && !slowThePipeline),
    .readyForBotIn(pipelineManagerIsReadyForBotIn),
    .resultValid(pipelineResultValid),
    
    .top(top),
    .botIndex(botIndex),
    .isBotValid(isBotValid),
    .pipelineReady(fifoFullness <= 25)
);

wire[5:0] validBotPermutations;
permuteCheck6 permuteChecker (top, bot, isBotValid, validBotPermutations);


wire[63:0] pipelineResult;
assign pipelineResult[63:41] = 0;
fullPipeline pipeline (
    .clk(clock),
    .rst(rst),
    .top(top),
    
    .bot(bot), // Represents all final 3 var swaps
    .botIndex(botIndex),
    .isBotValid(isBotValid),
    .validBotPermutations(validBotPermutations), // == {vABCin, vACBin, vBACin, vBCAin, vCABin, vCBAin}
    .fifoFullness(fifoFullness),
    .summedDataOut(pipelineResult[37:0]),
    .pcoeffCountOut(pipelineResult[40:38])
);

wire[63:0] dataFromFIFO;

wire validOut;
assign ovalid = validOut || LAST_RESORT_ACT;
outputBuffer resultsBuf (
    .clk(clock),
    .rst(rst),
    
    .dataInValid(pipelineResultValid),
    .dataIn(pipelineResult),
    
    .slowInputting(slowThePipeline),
    .dataOutReady(iready),
    .dataOutValid(validOut),
    .dataOut(dataFromFIFO)
);


/*
TEMP TESTING
*/

//`define LAST_RESORT_DELAY 100000
`define LAST_RESORT_DELAY 100000000

reg LAST_RESORT_hasReset = 0;
reg LAST_RESORT_hasStarted = 0;
reg[31:0] clockTicksSinceStarted = 0;

always @(posedge clock) begin
    if(rst) begin
        LAST_RESORT_hasReset <= 1;
        LAST_RESORT_hasStarted <= 0;
        clockTicksSinceStarted <= 0;
        LAST_RESORT_ACT <= 0;
    end else begin
        if(LAST_RESORT_hasReset && ivalid) begin
            LAST_RESORT_hasStarted <= 1;
        end
        if(LAST_RESORT_hasStarted) begin
            clockTicksSinceStarted <= clockTicksSinceStarted + 1;
        end
        if(clockTicksSinceStarted >= `LAST_RESORT_DELAY) begin
            LAST_RESORT_ACT <= 1;
        end
    end
end

// clock2x test
reg[10:0] clock2xReg;
reg[10:0] clockReg;

always @(posedge clock) if(rst) clockReg <= 0; else clockReg <= clockReg + 1;
always @(posedge clock2x) if(rst) clock2xReg <= 0; else clock2xReg <= clock2xReg + 1;

assign summedDataPcoeffCountOut[63:42] = {clock2xReg, clockReg};
assign summedDataPcoeffCountOut[41:0] = dataFromFIFO[41:0];

endmodule
