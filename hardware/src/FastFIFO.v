`timescale 1ns / 1ps

(* altera_attribute = "-name AUTO_SHIFT_REGISTER_RECOGNITION off; -name SYNCHRONIZER_IDENTIFICATION AUTO" *) 
module GrayCodePipe #(parameter WIDTH = 5) (
    input clkA,
    input[WIDTH-1:0] dataA,
    
    input clkB,
    output[WIDTH-1:0] dataB
);

reg[WIDTH-1:0] grayA;
always @(posedge clkA) begin
    grayA <= dataA ^ (dataA >> 1);
end

reg[WIDTH-1:0] grayB1; always @(posedge clkB) grayB1 <= grayA;
reg[WIDTH-1:0] grayB2; always @(posedge clkB) grayB2 <= grayB1;
generate
    assign dataB[WIDTH-1] = grayB2[WIDTH-1];
    genvar i;
    for(i = WIDTH-2; i >= 0; i = i - 1) begin
        assign dataB[i] = grayB2[i] ^ dataB[i+1];
    end
endgenerate

endmodule

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
GrayCodePipe #(3) rd_wr(rdclk, readAddr[DEPTH_LOG2-1:DEPTH_LOG2-3], wrclk, readAddrOctant_wr);

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
always @(posedge wrclk) begin
    if(wrrst) begin
        writeAddr <= 1; // Offset of one because read head must not wait at the position of the write head
    end else begin
        writeAddr <= writeAddr + writeEnable;
    end
end

wire[DEPTH_LOG2-1:0] writeAddrWire_rd;
GrayCodePipe #(DEPTH_LOG2) wr_rd(wrclk, writeAddr, rdclk, writeAddrWire_rd);
reg[DEPTH_LOG2-1:0] writeAddr_rd; always @(posedge rdclk) writeAddr_rd <= writeAddrWire_rd;


reg[DEPTH_LOG2-1:0] readAddr;
wire[DEPTH_LOG2-1:0] nextReadAddr = readAddr + 1;
assign empty = nextReadAddr == writeAddr_rd;
wire isReading = readRequest && !empty;

always @(posedge rdclk) begin
    if(rdrst) begin
        readAddr <= 0;
    end else begin
        if(isReading) readAddr <= nextReadAddr;
    end
end

AlmostFullOctants #(DEPTH_LOG2) almostFullComp(wrclk, writeAddr, rdclk, readAddr, almostFull);

hyperpipe #(.CYCLES(IS_MLAB ? 3 : 4)) isValidPipe(rdclk, isReading, dataOutValid);

generate
if(IS_MLAB) begin
DUAL_CLOCK_MEMORY_MLAB #(WIDTH, DEPTH_LOG2) mlabMemory (
    // Write Side
    .wrclk(wrclk),
    .writeEnable(writeEnable),
    .writeAddr(writeAddr),
    .dataIn(dataIn),
    
    // Read Side
    .rdclk(rdclk),
    .rstReadAddr(1'b0),
    .readAddressStall(1'b0),
    .readAddr(readAddr),
    .dataOut(dataOut)
);
assign eccStatus = 1'bZ;
end else begin
DUAL_CLOCK_MEMORY_M20K #(WIDTH, DEPTH_LOG2) m20kMemory (
    // Write Side
    .wrclk(wrclk),
    .writeEnable(writeEnable),
    .writeAddr(writeAddr),
    .dataIn(dataIn),
    
    // Read Side
    .rdclk(rdclk),
    .readEnable(1'b1),
    .readAddressStall(1'b0),
    .readAddr(readAddr),
    .dataOut(dataOut),
    .eccStatus(eccStatus)
);
end
endgenerate

endmodule



// Has a read latency of READ_ADDR_STAGES(MLAB) - READ_ADDR_STAGES+2(M20K) cycles after assertion of readRequest. (Then if the fifo had data dataOutValid should be asserted)
module FastFIFOController #(
    parameter DEPTH_LOG2 = 5,
    parameter READ_ADDR_STAGES = 0,
    parameter WRITE_ADDR_STAGES = 2,
    parameter WRITE_TO_READ_SYNC_STAGES = 0,
    parameter READ_TO_WRITE_SYNC_STAGES = 0,
    parameter MEMORY_HAS_RESET = 0 // This is a little dance that M20K FIFOs have to do, because M20K blocks don't support asynchronous reset of the read address register
) (
    // input side
    input wrclk,
    input wrrst,
    input writeEnable,
    output[DEPTH_LOG2-1:0] usedw,
    
    // output side
    input rdclk,
    input rdrst,
    input readRequest,
    output isReading,
    output empty,
    
    // Memory write side
    output reg[DEPTH_LOG2-1:0] writeAddr,
    
    // Memory read side
    output readAddressStall,
    output reg[DEPTH_LOG2-1:0] nextReadAddr
);


wire[DEPTH_LOG2-1:0] nextWriteAddr = writeAddr + 1;

wire[DEPTH_LOG2-1:0] readsValidUpTo;
hyperpipeDualClock #(.CYCLES_A(WRITE_ADDR_STAGES), .CYCLES_B(WRITE_TO_READ_SYNC_STAGES), .WIDTH(DEPTH_LOG2)) readsValidUpToPipe(wrclk, rdclk, nextWriteAddr, readsValidUpTo);

wire[DEPTH_LOG2-1:0] writesValidUpTo;
hyperpipeDualClock #(.CYCLES_A(READ_ADDR_STAGES), .CYCLES_B(READ_TO_WRITE_SYNC_STAGES), .WIDTH(DEPTH_LOG2)) writesValidUpToPipe(rdclk, wrclk, nextReadAddr, writesValidUpTo);

assign usedw = nextWriteAddr - writesValidUpTo;

generate if(MEMORY_HAS_RESET) begin
    assign empty = readsValidUpTo == nextReadAddr;
    
    assign isReading = readRequest && !empty;
    assign readAddressStall = !isReading;
    
end else begin
    reg rdrstD; always @(posedge rdclk) rdrstD <= rdrst;
    assign empty = readsValidUpTo == nextReadAddr || rdrstD; // Have to add check on rst to prevent transient reset issues
    // Little dance to set the memory's read addr register to 0
    
    assign isReading = readRequest && !empty && !rdrstD;
    
    // Also unstalling on reset is a clever trick to properly set the rdaddr register of the memory block
    assign readAddressStall = !(isReading || rdrstD);
    
end endgenerate

always @(posedge rdclk) begin
    if(rdrst) begin
        // also resets readAddr field within the MLAB to 0
        nextReadAddr <= MEMORY_HAS_RESET; // 0 or 1
    end else begin
        if(!readAddressStall) begin
            nextReadAddr <= nextReadAddr + 1;
        end
    end
end

always @(posedge wrclk) begin
    if(wrrst) begin
        writeAddr <= 0;
    end else begin
        if(writeEnable) begin
            `ifndef ALTERA_RESERVED_QIS
                // Hard failure when writing past full fifo
                if(nextWriteAddr == nextReadAddr - 2) begin
                    nextReadAddr <= {DEPTH_LOG2{1'bX}};
                    writeAddr <= {DEPTH_LOG2{1'bX}};
                end else
            `endif
            writeAddr <= nextWriteAddr;
        end
    end
end

endmodule

// Has a read latency of READ_ADDR_STAGES(MLAB) - READ_ADDR_STAGES+2(M20K) cycles after assertion of readRequest. (Then if the fifo had data dataOutValid should be asserted)
module FastFIFO #(
    parameter WIDTH = 20,
    parameter DEPTH_LOG2 = 5,
    parameter IS_MLAB = 1,// = DEPTH_LOG2 <= 5
    parameter READ_ADDR_STAGES = 0,
    parameter WRITE_ADDR_STAGES = 2
) (
    input clk,
    input rst,
    
    // input side
    input writeEnable,
    input[WIDTH-1:0] dataIn,
    output[DEPTH_LOG2-1:0] usedw,
    
    // output side
    input readRequest,
    output[WIDTH-1:0] dataOut,
    output dataOutValid,
    output empty,
    output eccStatus
);

// Memory write side
wire[DEPTH_LOG2-1:0] writeAddr;

// Memory read side
wire readAddressStall;
wire[DEPTH_LOG2-1:0] nextReadAddr;

wire isReading;
hyperpipe #(.CYCLES((IS_MLAB ? 0 : 2) + READ_ADDR_STAGES)) isReadingPipe(clk, isReading, dataOutValid);

FastFIFOController #(
    .DEPTH_LOG2(DEPTH_LOG2),
    .WRITE_ADDR_STAGES(WRITE_ADDR_STAGES+1),
    .READ_ADDR_STAGES(READ_ADDR_STAGES+1),
    .WRITE_TO_READ_SYNC_STAGES(0),
    .READ_TO_WRITE_SYNC_STAGES(0),
    .MEMORY_HAS_RESET(IS_MLAB)
) controller (
    // input side
    clk,
    rst,
    writeEnable,
    usedw,
    
    // output side
    clk,
    rst,
    readRequest,
    isReading,
    empty,
    
    // Memory write side
    writeAddr,
    
    // Memory read side
    readAddressStall,
    nextReadAddr
);

// Memory write side
wire writeEnableD;
wire[DEPTH_LOG2-1:0] writeAddrD;

hyperpipe #(.CYCLES(WRITE_ADDR_STAGES), .WIDTH(1+DEPTH_LOG2)) writePipe(clk, 
    {writeEnable,  writeAddr},
    {writeEnableD, writeAddrD}
);


// Memory read side
wire readAddressStallD;
wire[DEPTH_LOG2-1:0] nextReadAddrD;

hyperpipe #(.CYCLES(READ_ADDR_STAGES), .WIDTH(1+DEPTH_LOG2)) readAddressStallPipe(clk, 
    {readAddressStall,  nextReadAddr}, 
    {readAddressStallD, nextReadAddrD}
);

wire[WIDTH-1:0] dataInD;
hyperpipe #(.CYCLES(WRITE_ADDR_STAGES), .WIDTH(WIDTH)) writeDataPipe(clk, 
    dataIn,
    dataInD
);

wire[WIDTH-1:0] dataFromMem;
`ifdef ALTERA_RESERVED_QIS
assign dataOut = dataFromMem;
`else
// Ensure data is only valid when dataOutValid is asserted to ease debugging
assign dataOut = dataOutValid ? dataFromMem : {WIDTH{1'bX}};
`endif

generate if(IS_MLAB) begin
MEMORY_MLAB #(WIDTH, DEPTH_LOG2) mlabMemory (
    .clk(clk),
    .rstReadAddr(rst),
    
    // Write Side
    .writeEnable(writeEnableD),
    .writeAddr(writeAddrD),
    .dataIn(dataInD),
    
    // Read Side
    .readAddressStall(readAddressStallD),
    .readAddr(nextReadAddrD),
    .dataOut(dataFromMem)
);
assign eccStatus = 0;
end else begin
MEMORY_M20K #(WIDTH, DEPTH_LOG2) m20kMemory (
    .clk(clk),
    
    // Write Side
    .writeEnable(writeEnableD),
    .writeAddr(writeAddrD),
    .dataIn(dataInD),
    
    // Read Side
    .readEnable(1'b1),
    .readAddressStall(readAddressStallD),
    .readAddr(nextReadAddrD),
    .dataOut(dataFromMem),
    .eccStatus(eccStatus)
);
end endgenerate

endmodule




// Has a read latency of READ_ADDR_STAGES(MLAB) - READ_ADDR_STAGES+2(M20K) cycles after assertion of readRequest. (Then if the fifo had data dataOutValid should be asserted)
module FastDualClockFIFO #(
    parameter WIDTH = 20,
    parameter DEPTH_LOG2 = 5,
    parameter IS_MLAB = 1,// = DEPTH_LOG2 <= 5
    parameter READ_ADDR_STAGES = 1,
    parameter WRITE_ADDR_STAGES = 1
) (
    // input side
    input wrclk,
    input wrrst,
    input writeEnable,
    input[WIDTH-1:0] dataIn,
    output[DEPTH_LOG2-1:0] usedw,
    
    // output side
    input rdclk,
    input rdrst,
    input readRequest,
    output[WIDTH-1:0] dataOut,
    output dataOutValid,
    output empty,
    output eccStatus
);

// Memory write side
wire[DEPTH_LOG2-1:0] writeAddr;

// Memory read side
wire readAddressStall;
wire[DEPTH_LOG2-1:0] nextReadAddr;

wire isReading;

FastFIFOController #(
    .DEPTH_LOG2(DEPTH_LOG2),
    .WRITE_ADDR_STAGES(WRITE_ADDR_STAGES+1),
    .READ_ADDR_STAGES(READ_ADDR_STAGES+1),
    .WRITE_TO_READ_SYNC_STAGES(2),
    .READ_TO_WRITE_SYNC_STAGES(2),
    .MEMORY_HAS_RESET(IS_MLAB)
) controller (
    // input side
    wrclk,
    wrrst,
    writeEnable,
    usedw,
    
    // output side
    rdclk,
    rdrst,
    readRequest,
    isReading,
    empty,
    
    // Memory write side
    writeAddr,
    
    // Memory read side
    readAddressStall,
    nextReadAddr
);

// Memory write side
wire writeEnableD;
wire[DEPTH_LOG2-1:0] writeAddrD;

hyperpipe #(.CYCLES(WRITE_ADDR_STAGES), .WIDTH(1+DEPTH_LOG2)) writePipe(wrclk, 
    {writeEnable,  writeAddr},
    {writeEnableD, writeAddrD}
);

// Memory read side
wire readAddressStallD;
wire[DEPTH_LOG2-1:0] nextReadAddrD;

hyperpipe #(.CYCLES(READ_ADDR_STAGES), .WIDTH(1+DEPTH_LOG2), .MAX_FAN(1)) readAddressStallPipe(rdclk, 
    {readAddressStall,  nextReadAddr}, 
    {readAddressStallD, nextReadAddrD}
);

wire[WIDTH-1:0] dataInD;
hyperpipe #(.CYCLES(WRITE_ADDR_STAGES), .WIDTH(WIDTH)) writeDataPipe(wrclk, 
    dataIn,
    dataInD
);

hyperpipe #(.CYCLES((IS_MLAB ? 1 : 2) + READ_ADDR_STAGES)) isReadingPipe(rdclk, isReading, dataOutValid);


wire[WIDTH-1:0] dataFromMem;
`ifdef ALTERA_RESERVED_QIS
assign dataOut = dataFromMem;
`else
// Ensure data is only valid when dataOutValid is asserted to ease debugging
assign dataOut = dataOutValid ? dataFromMem : {WIDTH{1'bX}};
`endif

generate if(IS_MLAB) begin
DUAL_CLOCK_MEMORY_MLAB #(WIDTH, DEPTH_LOG2) mlabMemory_DC (
    // Write Side
    .wrclk(wrclk),
    .writeEnable(writeEnableD),
    .writeAddr(writeAddrD),
    .dataIn(dataInD),
    
    // Read Side
    .rdclk(rdclk),
    .rstReadAddr(rdrst),
    .readAddressStall(readAddressStallD),
    .readAddr(nextReadAddrD),
    .dataOut(dataFromMem)
);
assign eccStatus = 0;
end else begin
DUAL_CLOCK_MEMORY_M20K #(WIDTH, DEPTH_LOG2) m20kMemory_DC (
    // Write Side
    .wrclk(wrclk),
    .writeEnable(writeEnableD),
    .writeAddr(writeAddrD),
    .dataIn(dataInD),
    
    // Read Side
    .rdclk(rdclk),
    .readEnable(1'b1),
    .readAddressStall(readAddressStallD),
    .readAddr(nextReadAddrD),
    .dataOut(dataFromMem),
    .eccStatus(eccStatus)
);
end endgenerate

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
