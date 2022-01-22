`timescale 1ns / 1ps

module openCLFullPipelineClock2xAdapter(
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

wire outputQueueOValid;

wire[4:0] inputFIFOusedw;
assign oready = (inputFIFOusedw <= 27) & isInitialized & !stallInput;
wire receivingData = oready & botInValid;
wire sendingData = iready & outputQueueOValid;

assign ovalid = outputQueueOValid || ovalidOverride;


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



wire rst2x;
wire rst2xPostSync;
synchronizer #(.SYNC_STAGES(4)) rstSynchronizer(clock, rst, clock2x, rst2xPostSync);
hyperpipe #(.CYCLES(1)) rst2xPipe(clock2x, rst2xPostSync, rst2x);

wire ivalid2x;
wire iready2x;
wire ovalid2x;
wire oready2x;

wire[127:0] bot2x;
wire[63:0] summedDataPcoeffCountOut2x; 

openCLFullPipeline fastPipeline (
    .clock(clock2x),
    .rst(rst2x),
    .ivalid(ivalid2x), 
    .iready(iready2x),
    .ovalid(ovalid2x),
    .oready(oready2x),
    
    .top(top), // Async signal, does not need synchronization to new clock domain
    
    .bot(bot2x),
    .summedDataPcoeffCountOut(summedDataPcoeffCountOut2x)
);

dualClockFIFOWithDataValid #(.WIDTH(128)) inputFIFO (
    .wrclk(clock),
    .writeEnable(receivingData),
    .dataIn(bot),
    .wrusedw(inputFIFOusedw),
    
    .rdclk(clock2x),
    .rdclr(rst2x),
    .readEnable(oready2x),
    .dataOutValid(ivalid2x),
    .dataOut(bot2x)
);

wire[4:0] outputFIFOusedw2x;
hyperpipe #(.CYCLES(4)) iready2xPipe(clock2x, outputFIFOusedw2x <= 20, iready2x);
wire movingDataToOutput2x = ovalid2x;

dualClockFIFOWithDataValid #(.WIDTH(64)) outputFIFO (
    .wrclk(clock2x),
    .writeEnable(movingDataToOutput2x),
    .dataIn(summedDataPcoeffCountOut2x),
    .wrusedw(outputFIFOusedw2x),
    
    .rdclk(clock),
    .rdclr(rst),
    .readEnable(iready || !ovalid), // keep trying to read if not valid, to get lookahead-like behaviour
    .dataOutValid(outputQueueOValid),
    .dataOut(summedDataPcoeffCountOut)
);

endmodule
