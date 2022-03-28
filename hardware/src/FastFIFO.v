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
        if(isReading) readAddr <= readAddr + 1;
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

// Expects sufficient readRequests while resetting, so that the output pipe is flushed properly
module LowLatencyFastDualClockFIFO_MLAB #(parameter WIDTH = 20, parameter ALMOST_FULL_MARGIN = 8) (
    // Write Side
    input wrclk,
    input wrrst,
    input writeEnable,
    input[WIDTH-1:0] dataIn,
    output reg almostFull, // Works in octants. Activated when 75-87.5% of the fifo is used (128-64 for M20K(512), 8-4 for MLAB(32))
    
    // Read Side
    input rdclk,
    input rdrst,
    input readRequestPre,
    output reg[WIDTH-1:0] dataOut,
    output reg dataOutAvailable
);

reg[4:0] writeAddr;
reg[4:0] readAddr;

always @(posedge wrclk) begin
    if(wrrst) begin
        writeAddr <= 1; // Offset of one because read head must not wait at the position of the write head
    end else begin
        writeAddr <= writeAddr + writeEnable;
    end
end

wire[4:0] writeAddrWire_rd;
wire[4:0] readAddrWire_wr; 
grayCodePipe #(5) wr_rd(wrclk, writeAddr, rdclk, writeAddrWire_rd);
grayCodePipe #(5) rd_wr(rdclk, readAddr, wrclk, readAddrWire_wr);
reg[4:0] canReadUpTo; always @(posedge rdclk) canReadUpTo <= writeAddrWire_rd - 1;

wire[4:0] spaceLeft = readAddrWire_wr - writeAddr;
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
NO_READ_CLOCK_MEMORY_MLAB #(.WIDTH(WIDTH), .DEPTH_LOG2(5)) mlabMemory (
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
        dataOut <= dataFromMLAB;
        dataOutAvailable <= newReadAddr;
    end
end

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
