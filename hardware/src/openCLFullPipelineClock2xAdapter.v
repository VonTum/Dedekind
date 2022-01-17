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


wire rst;
wire isInitialized;
resetNormalizer rstNormalizer(clock, resetn, rst, isInitialized);
wire rst2x;
wire rst2xPostSync;
synchronizer #(.SYNC_STAGES(4)) rstSynchronizer(clock, rst, clock2x, rst2xPostSync);
hyperpipe #(.CYCLES(9)) rst2xPipe(clock2x, rst2xPostSync, rst2x);

wire ivalid2x;
wire iready2x;
wire ovalid2x;
wire oready2x;

wire startNewTop2x;
wire[63:0] botLower2x; // Represents all final 3 var swaps
wire[63:0] botUpper2x; // Represents all final 3 var swaps
wire[63:0] summedDataPcoeffCountOut2x; 

openCLFullPipeline fastPipeline (
    .clock(clock2x),
    .rst(rst2x),
    .ivalid(ivalid2x), 
    .iready(iready2x),
    .ovalid(ovalid2x),
    .oready(oready2x),
    
    .startNewTop(startNewTop2x),
    .botLower(botLower2x),
    .botUpper(botUpper2x),
    .summedDataPcoeffCountOut(summedDataPcoeffCountOut2x)
);

wire[4:0] inputFIFOusedw;
assign oready = (inputFIFOusedw <= 27) & isInitialized;
wire receivingData = oready & ivalid;

dualClockFIFOWithDataValid #(.WIDTH(64+64+1)) inputFIFO (
    .wrclk(clock),
    .writeEnable(receivingData),
    .dataIn({botLower, botUpper, startNewTop}),
    .wrusedw(inputFIFOusedw),
    
    .rdclk(clock2x),
    .rdclr(rst2x),
    .readEnable(oready2x),
    .dataOutValid(ivalid2x),
    .dataOut({botLower2x, botUpper2x, startNewTop2x})
);

wire[4:0] outputFIFOusedw2x;
hyperpipe #(.CYCLES(2)) oready2xPipe(clock2x, outputFIFOusedw2x <= 27, iready2x);
wire movingDataToOutput2x = iready2x & ovalid2x;

dualClockFIFOWithDataValid #(.WIDTH(64)) outputFIFO (
    .wrclk(clock2x),
    .writeEnable(movingDataToOutput2x),
    .dataIn(summedDataPcoeffCountOut2x),
    .wrusedw(outputFIFOusedw2x),
    
    .rdclk(clock),
    .rdclr(rst),
    .readEnable(iready || !ovalid), // keep trying to read if not valid, to get lookahead-like behaviour
    .dataOutValid(ovalid),
    .dataOut(summedDataPcoeffCountOut)
);

endmodule
