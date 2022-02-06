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

// Registers to have basically a 1-deep queue, so that the module can "show it's readyness" before 40 clock cycles after reset
reg[127:0] storedInputBot;
reg botInValid;
reg topInValid;

always @(posedge clock) begin
    if(!resetn) begin
        botInValid <= 0;
        topInValid <= 0;
    end else begin
        if(oready) begin
            storedInputBot <= {botUpper, botLower};
            botInValid <= ivalid && !startNewTop;
            topInValid <= ivalid && startNewTop;
        end
    end
end


wire rst;
wire isInitialized;
resetNormalizer rstNormalizer(clock, resetn, rst, isInitialized);

wire[127:0] top;
wire stallInput;
wire ovalidOverride;

wire resultsAvailable;

wire readyForInputBot;
assign oready = (readyForInputBot && isInitialized && !stallInput) || (!botInValid && !topInValid && !rst); // Actively pull data into the input register if possible
wire receivingBot = oready && botInValid;
wire sendingBotResult = iready && resultsAvailable;


assign ovalid = resultsAvailable || ovalidOverride;


topManager topMngr (
    .clk(clock),
    .rst(rst),
    
    .topIn(storedInputBot),
    .topInValid(oready & topInValid),
    
    .topOut(top),
    
    // Input queue side
    .botWentIn(receivingBot),
    .stallInput(stallInput),
    
    // Output queue side
    .botWentOut(sendingBotResult),
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
    .bot(storedInputBot),
    .writeBot(receivingBot),
    .readyForInputBot(readyForInputBot),
    
    .grabResults(sendingBotResult),
    .resultsAvailable(resultsAvailable),
    .pcoeffSum(summedDataOut),
    .pcoeffCount(pcoeffCountOut),
    .eccStatus(eccStatus)
);

endmodule
