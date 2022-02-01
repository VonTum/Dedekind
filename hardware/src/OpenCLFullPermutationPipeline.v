`timescale 1ns / 1ps

module OpenCLFullPermutationPipeline(
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

wire[127:0] bot = {botUpper, botLower};
wire botInValid = ivalid && !startNewTop;
wire topInValid = ivalid && startNewTop;

wire rst;
wire isInitialized;
resetNormalizer rstNormalizer(clock, resetn, rst, isInitialized);

wire[127:0] top;
wire stallInput;
wire ovalidOverride;

wire resultsAvailable;

wire readyForInputBot;
assign oready = readyForInputBot & isInitialized & !stallInput;
wire receivingData = oready & botInValid;
wire sendingData = iready & resultsAvailable;


assign ovalid = resultsAvailable || ovalidOverride;


topManager topMngr (
    .clk(clock),
    .rst(rst),
    
    .topIn(bot),
    .topInValid(oready & topInValid),
    
    .topOut(top),
    
    // Input queue side
    .botWentIn(receivingData),
    .stallInput(stallInput),
    
    // Output queue side
    .botWentOut(sendingData),
    .iready(iready),
    .ovalidOverride(ovalidOverride)
);

wire[47:0] summedDataOut;
wire[12:0] pcoeffCountOut;
wire eccStatus;

assign summedDataPcoeffCountOut = {eccStatus, 2'b00, pcoeffCountOut, summedDataOut};

fullPermutationPipeline permutationPipeline (
    .clk(clock),
    .clk2x(clock2x),
    .rst(rst),
    
    .top(top),
    .bot(bot),
    .writeBot(receivingData),
    .readyForInputBot(readyForInputBot),
    
    .grabResults(sendingData),
    .resultsAvailable(resultsAvailable),
    .pcoeffSum(summedDataOut),
    .pcoeffCount(pcoeffCountOut),
    .eccStatus(eccStatus)
);

endmodule
