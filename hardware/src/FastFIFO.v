`timescale 1ns / 1ps


// Has a read latency of READ_ADDR_STAGES(MLAB) - READ_ADDR_STAGES+2(M20K) cycles after assertion of readRequest. (Then if the fifo had data dataOutValid should be asserted)
module FastFIFOController #(
    parameter DEPTH_LOG2 = 5,
    parameter READ_ADDR_STAGES = 0,
    parameter WRITE_ADDR_STAGES = 2,
    parameter WRITE_TO_READ_SYNC_STAGES = 0,
    parameter READ_TO_WRITE_SYNC_STAGES = 0
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

assign empty = readsValidUpTo == nextReadAddr;

reg rdrstD; always @(posedge rdclk) rdrstD <= rdrst;

assign isReading = readRequest && !empty && !rdrstD;

// Also unstalling on reset is a clever trick to properly set the rdaddr register of the memory block
assign readAddressStall = !(isReading || rdrstD);

always @(posedge rdclk) begin
    if(rdrst) begin
        // also resets readAddr field within the MLAB to 0
        nextReadAddr <= 0;
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
    .READ_TO_WRITE_SYNC_STAGES(0)
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


MEMORY_BLOCK #(WIDTH, DEPTH_LOG2, IS_MLAB) fifoMemory (
    .clk(clk),
    
    // Write Side
    .writeEnable(writeEnableD),
    .writeAddr(writeAddrD),
    .dataIn(dataInD),
    
    // Read Side
    .readAddressStall(readAddressStallD),
    .readAddr(nextReadAddrD),
    .dataOut(dataOut),
    .eccStatus(eccStatus)
);

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
    .READ_TO_WRITE_SYNC_STAGES(2)
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

localparam MAXFAN_BLOCKSIZE = IS_MLAB ? 20 : 32;

// Memory read side
wire readAddressStallD;
wire[DEPTH_LOG2-1:0] nextReadAddrD;

hyperpipe #(.CYCLES(READ_ADDR_STAGES), .WIDTH(1+DEPTH_LOG2), .MAX_FAN(MAXFAN_BLOCKSIZE)) readAddressStallPipe(rdclk, 
    {readAddressStall,  nextReadAddr}, 
    {readAddressStallD, nextReadAddrD}
);

wire[WIDTH-1:0] dataInD;
hyperpipe #(.CYCLES(WRITE_ADDR_STAGES), .WIDTH(WIDTH)) writeDataPipe(wrclk, 
    dataIn,
    dataInD
);

wire dataValidFromMem;
hyperpipe #(.CYCLES((IS_MLAB ? 0 : 2) + READ_ADDR_STAGES)) isReadingPipe(rdclk, isReading, dataValidFromMem);
wire[WIDTH-1:0] dataFromMem;
generate
if(IS_MLAB) begin
    wire readRequestShouldBeAvailable;
    hyperpipe #(.CYCLES((IS_MLAB ? 0 : 2) + READ_ADDR_STAGES), .MAX_FAN(MAXFAN_BLOCKSIZE)) readRequestPipe(rdclk, readRequest, readRequestShouldBeAvailable);
    
    // Extra register to combat metastability from an unregistered write into the memory. 
    reg[WIDTH-1:0] storedDataOut;
    reg storedDataOutValid;
    
    always @(posedge rdclk) begin
        if(rdrst) begin
            storedDataOutValid <= 0;
        end else begin
            if(readRequestShouldBeAvailable) begin
                storedDataOutValid <= dataValidFromMem;
                storedDataOut <= dataFromMem;
            end
        end
    end
    assign dataOutValid = storedDataOutValid && readRequestShouldBeAvailable;
    assign dataOut = storedDataOut;
end else begin
    assign dataOutValid = dataValidFromMem;
    assign dataOut = dataFromMem;
end
endgenerate

DUAL_CLOCK_MEMORY_BLOCK #(WIDTH, DEPTH_LOG2, IS_MLAB) fifoMemory (
    // Write Side
    .wrclk(wrclk),
    .writeEnable(writeEnableD),
    .writeAddr(writeAddrD),
    .dataIn(dataInD),
    
    // Read Side
    .rdclk(rdclk),
    .readAddressStall(readAddressStallD),
    .readAddr(nextReadAddrD),
    .dataOut(dataFromMem),
    .eccStatus(eccStatus)
);

endmodule



