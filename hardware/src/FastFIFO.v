`timescale 1ns / 1ps

module AlmostFullOctants #(parameter DEPTH_LOG2 = 9) (
    // Write clk
    input wrclk,
    input[DEPTH_LOG2-1:0] writeAddr,
    
    // Read clk
    input rdclk,
    input[DEPTH_LOG2-1:0] readAddr,
    
    // Write clk
    output reg almostFull
);

wire[2:0] writeAddrOctant = writeAddr[DEPTH_LOG2-1:DEPTH_LOG2-3];
wire[2:0] readAddrOctant_wr;
grayCodePipe #(3) rd_wr(rdclk, readAddr[DEPTH_LOG2-1:DEPTH_LOG2-3], wrclk, readAddrOctant_wr);

wire[2:0] leftoverOctant = readAddrOctant_wr - writeAddrOctant;
always @(posedge wrclk) almostFull <= leftoverOctant == 1; // At least a quarter of the FIFO is still free. Should synthesize to a single 6-1 LUT

endmodule

module FastDualClockFIFO_SAFE #(parameter IS_MLAB = 0, parameter WIDTH = 16, parameter DEPTH_LOG2 = 9) (
    // Write Side
    input wrclk,
    input wrrst,
    
    input writeEnable,
    input[WIDTH-1:0] dataIn,
    output almostFull, // Works in octants. Activated when 75-87.5% of the fifo is used (128-64 for M20K(512), 8-4 for MLAB(32))
    
    // Read Side
    input rdclk,
    input rdrst,
    input readRequest,
    output[WIDTH-1:0] dataOut,
    output empty,
    output dataOutValid,
    output eccStatus
);

reg[DEPTH_LOG2-1:0] writeAddr;
reg[DEPTH_LOG2-1:0] readAddr;

always @(posedge wrclk) begin
    if(wrrst) begin
        writeAddr <= 1; // Offset of one because read head must not wait at the position of the write head
    end else begin
        writeAddr <= writeAddr + writeEnable;
    end
end

wire[DEPTH_LOG2-1:0] writeAddrWire_rd;
grayCodePipe #(DEPTH_LOG2) wr_rd(wrclk, writeAddr, rdclk, writeAddrWire_rd);
reg[DEPTH_LOG2-1:0] canReadUpTo; always @(posedge rdclk) canReadUpTo <= writeAddrWire_rd - 1;


assign empty = readAddr == canReadUpTo;
wire isReading = readRequest && !empty;

always @(posedge rdclk) begin
    if(rdrst) begin
        readAddr <= 0;
    end else begin
        readAddr <= readAddr + isReading;
    end
end

AlmostFullOctants #(DEPTH_LOG2) almostFullComp(wrclk, writeAddr, rdclk, readAddr, almostFull);

hyperpipe #(.CYCLES(IS_MLAB ? 3 : 4)) isValidPipe(rdclk, isReading, dataOutValid);

generate
if(IS_MLAB) begin
DUAL_CLOCK_MEMORY_MLAB #(.WIDTH(WIDTH), .DEPTH_LOG2(DEPTH_LOG2), .OUTPUT_REGISTER(1)) mlabMemory (
    // Write Side
    .wrclk(wrclk),
    .writeEnable(writeEnable),
    .writeAddr(writeAddr),
    .dataIn(dataIn),
    
    // Read Side
    .rdclk(rdclk),
    .readAddr(readAddr),
    .dataOut(dataOut)
);
assign eccStatus = 1'bZ;
end else begin
DUAL_CLOCK_MEMORY_M20K #(.WIDTH(WIDTH), .DEPTH_LOG2(DEPTH_LOG2)) m20kMemory (
    // Write Side
    .wrclk(wrclk),
    .writeEnable(writeEnable),
    .writeAddr(writeAddr),
    .dataIn(dataIn),
    
    // Read Side
    .rdclk(rdclk),
    .readEnable(1'b1),
    .readAddr(readAddr),
    .dataOut(dataOut),
    .eccStatus(eccStatus)
);
end
endgenerate

endmodule


// Read latency of 4 cycles
module FastFIFO_SAFE_M20K #(
    parameter WIDTH = 16,
    parameter DEPTH_LOG2 = 9,
    parameter ALMOST_FULL_MARGIN = 50,
    parameter ALMOST_EMPTY_MARGIN = 50,
    parameter HOLD_LAST_READ = 1
) (
    input clk,
    input rst,
    
    // Write Side
    input writeEnable,
    input[WIDTH-1:0] dataIn,
    output reg almostFull,
    output almostEmpty,
    
    // Read Side
    input readRequest,
    output[WIDTH-1:0] dataOut, // Holds the last valid data
    output empty,
    output dataOutValid,
    output eccStatus
);

reg[DEPTH_LOG2-1:0] writeAddr;
reg[DEPTH_LOG2-1:0] readAddr;

wire[DEPTH_LOG2-1:0] writesLeft = readAddr - writeAddr;
wire[DEPTH_LOG2-1:0] usedw = writeAddr - readAddr - 1;
assign almostEmpty = usedw <= ALMOST_EMPTY_MARGIN;

assign empty = writesLeft == (1 << DEPTH_LOG2) - 1;
always @(posedge clk) almostFull <= writesLeft < ALMOST_FULL_MARGIN;

wire isReading = readRequest && !empty;

always @(posedge clk) begin
    if(rst) begin
        writeAddr <= 1; // Offset of one because read head must not wait at the position of the write head
        readAddr <= 0;
    end else begin
        writeAddr <= writeAddr + writeEnable;
        readAddr <= readAddr + isReading;
    end
end

hyperpipe #(.CYCLES(4)) isValidPipe(clk, isReading, dataOutValid);

reg isReadingD; always @(posedge clk) isReadingD <= isReading;

MEMORY_M20K #(.WIDTH(WIDTH), .DEPTH_LOG2(DEPTH_LOG2), .FORCE_TO_ZERO(!HOLD_LAST_READ)) m20kMemory (
    .clk(clk),
    
    // Write Side
    .writeEnable(writeEnable),
    .writeAddr(writeAddr),
    .dataIn(dataIn),
    
    // Read Side
    .readEnable(isReadingD), // readAddr must first increment, only then read
    .readAddr(readAddr),
    .dataOut(dataOut),
    .eccStatus(eccStatus)
);

endmodule

/*
    Allows the synchronized outputting of several intermittent streams of data. 
    Outputs a data element set every 4 cycles if all are available
    This module does not provide almost full flags as they have been deemed unneccesary
*/
module MultiStreamSynchronizer #(parameter DEPTH_LOG2 = 9, parameter NUMBER_OF_FIFOS = 20, parameter ALMOST_FULL_MARGIN = 16) (
    input clk,
    input rst,
    
    // Write side
    input[NUMBER_OF_FIFOS-1:0] writes,
    output[DEPTH_LOG2*NUMBER_OF_FIFOS-1:0] writeAddrs,
    output reg[NUMBER_OF_FIFOS-1:0] almostFulls,
    
    // Read side
    output reg readEnable,
    output reg[DEPTH_LOG2-1:0] readAddr,
    input slowDown
);

reg[DEPTH_LOG2-1:0] readAddrReg;
reg[DEPTH_LOG2-1:0] writeAddrsRegs[NUMBER_OF_FIFOS-1:0];

reg[NUMBER_OF_FIFOS-1:0] hasDatas;

genvar i;
generate
for(i = 0; i < NUMBER_OF_FIFOS; i = i + 1) begin
    assign writeAddrs[DEPTH_LOG2 * i +: DEPTH_LOG2] = writeAddrsRegs[i];
    wire[DEPTH_LOG2-1:0] leftoverWords = readAddrReg - writeAddrsRegs[i] - 1;
    always @(posedge clk) begin
        hasDatas[i] <= readAddrReg != writeAddrsRegs[i];
        almostFulls[i] <= leftoverWords < ALMOST_FULL_MARGIN;
        if(rst) begin
            writeAddrsRegs[i] <= 0;
        end else begin
            writeAddrsRegs[i] <= writeAddrsRegs[i] + writes[i];
        end
    end
end
endgenerate

reg read; // try to read every 3 cycles
reg readD;
reg[1:0] cycler = 0;

always @(posedge clk) begin
    readD <= read;
    readEnable <= readD;
    read <= cycler == 0 && !slowDown && &hasDatas;
    cycler <= cycler >= 2 ? 0 : cycler + 1;
    readAddr <= readAddrReg - 1;
    if(rst) begin
        readAddrReg <= 0;
    end else begin
        readAddrReg <= readAddrReg + read;
    end
end

endmodule



// Expects sufficient readRequests while resetting, so that the output pipe is flushed properly
module LowLatencyFastDualClockFIFO_MLAB #(parameter WIDTH = 20, parameter DEPTH_LOG2 = 5, parameter ALMOST_FULL_MARGIN = 8) (
    // Write Side
    input wrclk,
    input wrrst,
    input writeEnable,
    input[WIDTH-1:0] dataIn,
    output reg almostFull,
    
    // Read Side
    input rdclk,
    input rdrst,
    input readRequestPre,
    output reg[WIDTH-1:0] dataOut, // Forced to 0 if not dataOutAvailable
    output reg dataOutAvailable
);

reg[DEPTH_LOG2-1:0] writeAddr;
reg[DEPTH_LOG2-1:0] readAddr;

always @(posedge wrclk) begin
    if(wrrst) begin
        writeAddr <= 1; // Offset of one because read head must not wait at the position of the write head
    end else begin
        writeAddr <= writeAddr + writeEnable;
    end
end

wire[DEPTH_LOG2-1:0] writeAddrWire_rd;
wire[DEPTH_LOG2-1:0] readAddrWire_wr; 
grayCodePipe #(DEPTH_LOG2) wr_rd(wrclk, writeAddr, rdclk, writeAddrWire_rd);
grayCodePipe #(DEPTH_LOG2) rd_wr(rdclk, readAddr, wrclk, readAddrWire_wr);
reg[DEPTH_LOG2-1:0] canReadUpTo; always @(posedge rdclk) canReadUpTo <= writeAddrWire_rd - 1;

wire[DEPTH_LOG2-1:0] spaceLeft = readAddrWire_wr - writeAddr;
always @(posedge wrclk) almostFull <= spaceLeft <= ALMOST_FULL_MARGIN;

wire canReadNext = readAddr != canReadUpTo;

(* dont_merge *) reg readRequestAddr; always @(posedge rdclk) readRequestAddr <= readRequestPre;
(* dont_merge *) reg readRequestData; always @(posedge rdclk) readRequestData <= readRequestPre;

reg newReadAddr;
always @(posedge rdclk) begin
    if(readRequestAddr) begin
        if(rdrst) begin
            readAddr <= 0;
        end else begin
            readAddr <= readAddr + canReadNext;
        end
        newReadAddr <= canReadNext;
    end
end

wire[WIDTH-1:0] dataFromMLAB;
NO_READ_CLOCK_MEMORY_MLAB #(.WIDTH(WIDTH), .DEPTH_LOG2(DEPTH_LOG2)) mlabMemory (
    // Write Side
    .wrclk(wrclk),
    .writeEnable(writeEnable),
    .writeAddr(writeAddr),
    .dataIn(dataIn),
    
    // Read Side
    .readAddr(readAddr),
    .dataOut(dataFromMLAB)
);

always @(posedge rdclk) begin
    if(readRequestData) begin
        dataOut <= newReadAddr ? dataFromMLAB : 0; // Force output to 0 if not valid
        dataOutAvailable <= newReadAddr;
    end
end

endmodule



// Expects sufficient readRequests while resetting, so that the output pipe is flushed properly
module LowLatencyFastDualClockFIFO_M20K #(parameter WIDTH = 32, parameter DEPTH_LOG2 = 9, parameter ALMOST_FULL_MARGIN = 8) (
    // Write Side
    input wrclk,
    input wrrst,
    input writeEnable,
    input[WIDTH-1:0] dataIn,
    output reg almostFull,
    
    // Read Side
    input rdclk,
    input rdrst,
    input readRequest,
    output[WIDTH-1:0] dataOut, // Forced to 0 if not dataOutAvailable
    output reg dataOutAvailable,
    
    output reg eccStatus
);

reg[DEPTH_LOG2-1:0] writeAddr;
reg[DEPTH_LOG2-1:0] readAddr;

always @(posedge wrclk) begin
    if(wrrst) begin
        writeAddr <= 0;
    end else begin
        writeAddr <= writeAddr + writeEnable;
    end
end

wire[DEPTH_LOG2-1:0] writeAddrWire_rd;
wire[DEPTH_LOG2-1:0] readAddrWire_wr; 
grayCodePipe #(DEPTH_LOG2) wr_rd(wrclk, writeAddr, rdclk, writeAddrWire_rd);
grayCodePipe #(DEPTH_LOG2) rd_wr(rdclk, readAddr, wrclk, readAddrWire_wr);
reg[DEPTH_LOG2-1:0] canReadUpTo; always @(posedge rdclk) canReadUpTo <= writeAddrWire_rd;

wire[DEPTH_LOG2-1:0] spaceLeft = readAddrWire_wr - writeAddr - 1;
always @(posedge wrclk) almostFull <= spaceLeft <= ALMOST_FULL_MARGIN;

wire canReadNext = readAddr != canReadUpTo;

(* dont_merge *) reg storedAddrValid;
(* dont_merge *) reg storedAddrValidForClear;
always @(posedge rdclk) begin
    if(readRequest) begin
        if(rdrst) begin
            readAddr <= 0;
        end else begin
            readAddr <= readAddr + canReadNext;
        end
        storedAddrValid <= canReadNext;
        storedAddrValidForClear <= canReadNext;
        dataOutAvailable <= storedAddrValid;
    end
end

wire eccWire;
always @(posedge rdclk) eccStatus <= eccWire;
LOW_LATENCY_M20K #(.WIDTH(WIDTH), .DEPTH_LOG2(DEPTH_LOG2), .USE_SCLEAR(1), .USE_PIPELINE_REGISTER(0)) m20kMemory (
    // Write Side
    .wrclk(wrclk),
    .writeEnable(writeEnable),
    .writeAddr(writeAddr),
    .dataIn(dataIn),
    
    // Read Side
    .rdclk(rdclk),
    .readClockEnable(readRequest),
    .readAddr(readAddr),
    .dataOut(dataOut),
    .eccStatus(eccWire),
    
    // Optional read output reg sclear
    .rstOutReg(!storedAddrValidForClear)
);

endmodule


// 4 cycles read latency
module FastDualClockFIFO_M20K #(parameter WIDTH = 32, parameter DEPTH_LOG2 = 9, parameter ALMOST_FULL_MARGIN = 8) (
    // Write Side
    input wrclk,
    input wrrst,
    input writeEnable,
    input[WIDTH-1:0] dataIn,
    output reg almostFull,
    
    // Read Side
    input rdclk,
    input rdrst,
    input readRequest,
    output[WIDTH-1:0] dataOut, // Forced to 0 if not dataOutValid
    output reg dataOutValid,
    
    output reg eccStatus
);

reg[DEPTH_LOG2-1:0] writeAddr;
reg[DEPTH_LOG2-1:0] readAddr;

always @(posedge wrclk) begin
    if(wrrst) begin
        writeAddr <= 1;
    end else begin
        writeAddr <= writeAddr + writeEnable;
    end
end

wire[DEPTH_LOG2-1:0] writeAddrWire_rd;
wire[DEPTH_LOG2-1:0] readAddrWire_wr; 
grayCodePipe #(DEPTH_LOG2) wr_rd(wrclk, writeAddr, rdclk, writeAddrWire_rd);
grayCodePipe #(DEPTH_LOG2) rd_wr(rdclk, readAddr, wrclk, readAddrWire_wr);
reg[DEPTH_LOG2-1:0] canReadUpTo; always @(posedge rdclk) canReadUpTo <= writeAddrWire_rd - 1;

wire[DEPTH_LOG2-1:0] spaceLeft = readAddrWire_wr - writeAddr;
always @(posedge wrclk) almostFull <= spaceLeft <= ALMOST_FULL_MARGIN;

wire canReadNext = readAddr != canReadUpTo;

reg memRead; always @(posedge rdclk) memRead <= canReadNext && readRequest;
reg valid_ADDR; always @(posedge rdclk) valid_ADDR <= memRead;
reg valid_PIPE; always @(posedge rdclk) valid_PIPE <= valid_ADDR;
always @(posedge rdclk) dataOutValid <= valid_PIPE;

(* dont_merge *) reg storedAddrValid;
(* dont_merge *) reg storedAddrValidForClear;
always @(posedge rdclk) begin
    if(rdrst) begin
        readAddr <= 0;
    end else begin
        readAddr <= readAddr + (canReadNext && readRequest);
    end
end

wire eccWire;
always @(posedge rdclk) eccStatus <= eccWire;
DUAL_CLOCK_MEMORY_M20K #(.WIDTH(WIDTH), .DEPTH_LOG2(DEPTH_LOG2), .FORCE_TO_ZERO(1)) m20kMemory (
    // Write Side
    .wrclk(wrclk),
    .writeEnable(writeEnable),
    .writeAddr(writeAddr),
    .dataIn(dataIn),
    
    // Read Side
    .rdclk(rdclk),
    .readEnable(memRead),
    .readAddr(readAddr),
    .dataOut(dataOut), // == 0 if not reading
    .eccStatus(eccWire)
);

endmodule



// Expects sufficient readRequests while resetting, so that the output pipe is flushed properly
module LowLatencyFastFIFO_M20K #(parameter WIDTH = 32, parameter DEPTH_LOG2 = 9, parameter ALMOST_FULL_MARGIN = 64) (
    input clk,
    input rst,
    
    // Write Side
    input writeEnable,
    input[WIDTH-1:0] dataIn,
    output reg almostFull,
    
    // Read Side
    input readEnable,
    output reg[WIDTH-1:0] dataOut,
    output reg dataOutAvailable,
    
    output reg eccStatus
);

reg[DEPTH_LOG2-1:0] writeAddr;
reg[DEPTH_LOG2-1:0] readAddr;

wire[DEPTH_LOG2-1:0] spaceLeft = readAddr - writeAddr - 1;
always @(posedge clk) almostFull <= spaceLeft <= ALMOST_FULL_MARGIN;

wire canReadNext = readAddr != writeAddr;

wire memECC;

wire[WIDTH-1:0] dataFromM20K;
reg newReadAddrStoredInM20K;
reg newDataStoredInM20K;
always @(posedge clk) begin
    if(rst) begin
        writeAddr <= 0;
        readAddr <= 0;
    end else begin
        writeAddr <= writeAddr + writeEnable;
        readAddr <= readAddr + (canReadNext && readEnable);
    end
    if(readEnable || rst) begin
        newReadAddrStoredInM20K <= canReadNext;
        newDataStoredInM20K <= newReadAddrStoredInM20K;
        
        dataOut <= dataFromM20K;
        dataOutAvailable <= newDataStoredInM20K;
        
        eccStatus <= memECC && newDataStoredInM20K;
    end
end

LOW_LATENCY_M20K #(.WIDTH(WIDTH), .DEPTH_LOG2(DEPTH_LOG2), .USE_SCLEAR(0), .USE_PIPELINE_REGISTER(0)) m20kMemory (
    // Write Side
    .wrclk(clk),
    .writeEnable(writeEnable),
    .writeAddr(writeAddr),
    .dataIn(dataIn),
    
    // Read Side
    .rdclk(clk),
    .readClockEnable(readEnable),
    .readAddr(readAddr),
    .dataOut(dataFromM20K),
    .eccStatus(memECC),
    
    .rstOutReg(1'b0) // Unconnected
);

endmodule

/*
Output register to be placed after fifo. This allows for zero-latency access, but dramatically reduces the throughput of the output port!
Only use with low-throughput reading where lookahead reading is required
*/
module FastFIFOOutputReg #(parameter WIDTH = 8) (
    input clk,
    input rst,
    
    // To Read side of Fifo
    input fifoEmpty,
    input[WIDTH-1:0] dataFromFIFO,
    input dataFromFIFOValid,
    output readFromFIFO,
    
    // Output interface
    input grab,
    output reg dataAvailable,
    output reg[WIDTH-1:0] dataOut
);

/*
Fix for a HORRIBLE bug. For FIFOs with long read latency, a very rare event may occur where the FIFO is read twice, and the first read is lost:

Timeline:
- FIFO is empty, dataAvailable = 0.  
- item arrives in FIFO, readFromFIFO is called, and the fifo becomes empty again. 
- First data element is now in flight. 
- An element arrives in the fifo, and yet again a positive edge on !fifoEmpty triggers readFromFIFO. 
- Second data element now also in flight. 
- First arrives
- Second arrives, overwriting the first. BUG

I spent weeks on this. WEEKS!
*/
reg dataFromFIFOIsInFlight;
assign readFromFIFO = !fifoEmpty && (grab || (!dataAvailable && !dataFromFIFOIsInFlight));

always @(posedge clk) begin
    if(rst) begin
        dataAvailable <= 0;
        dataFromFIFOIsInFlight <= 0;
    end else begin
        if(grab) dataAvailable <= 0;
        else if(dataFromFIFOValid) begin
            dataAvailable <= 1;
            dataFromFIFOIsInFlight <= 0;
            dataOut <= dataFromFIFO;
        end
        if(readFromFIFO) dataFromFIFOIsInFlight <= 1; // Cannot happen together with dataFromFIFOValid
    end
end

endmodule
