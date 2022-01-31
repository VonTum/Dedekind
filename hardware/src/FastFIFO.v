`timescale 1ns / 1ps


// Has a read latency of READ_ADDR_STAGES(MLAB) - READ_ADDR_STAGES+2(M20K) cycles after assertion of readRequest. (Then if the fifo had data dataOutValid should be asserted)
module FastFIFOController #(
    parameter DEPTH_LOG2 = 5,
    parameter IS_MLAB = 1,// = DEPTH_LOG2 <= 5
    parameter READ_ADDR_STAGES = 0,
    parameter WRITE_ADDR_STAGES = 2
) (
    input clk,
    input rst,
    
    // input side
    input writeEnable,
    output[DEPTH_LOG2-1:0] usedw,
    
    // output side
    input readRequest,
    output dataOutValid,
    output empty,
    
    // Memory write side
    output writeEnableD,
    output[DEPTH_LOG2-1:0] writeAddrD,
    
    // Memory read side
    output readAddressStallD,
    output[DEPTH_LOG2-1:0] nextReadAddrD
);

wire rstLocal; // Manual reset tree, can't use constraints to have it generate it for me. 
hyperpipe #(.CYCLES(2)) rstPipe(clk, rst, rstLocal);

reg rstD; always @(posedge clk) rstD <= rstLocal;

reg[DEPTH_LOG2-1:0] nextReadAddr;
reg[DEPTH_LOG2-1:0] writeAddr;
wire[DEPTH_LOG2-1:0] nextWriteAddr = writeAddr + 1;

wire[DEPTH_LOG2-1:0] readsValidUpTo; 
hyperpipe #(.CYCLES(WRITE_ADDR_STAGES+1), .WIDTH(DEPTH_LOG2)) readsValidUpToPipe(clk, nextWriteAddr, readsValidUpTo);
assign usedw = readsValidUpTo - nextReadAddr;

assign empty = readsValidUpTo == nextReadAddr;

wire isReading = readRequest && !empty && !rstD;

hyperpipe #(.CYCLES((IS_MLAB ? 0 : 2) + READ_ADDR_STAGES)) isReadingPipe(clk, isReading, dataOutValid);

// clever trick to properly set the rdaddr register of the memory block
wire isReadingOrHasJustReset = isReading || rstD;
always @(posedge clk) begin
    if(rstLocal) begin
        // also resets readAddr field within the MLAB to 0
        nextReadAddr <= 0;
        writeAddr <= 0;
    end else begin
        if(isReadingOrHasJustReset) begin
            nextReadAddr <= nextReadAddr + 1;
        end
        
        if(writeEnable) begin
            writeAddr <= nextWriteAddr;
        end
    end
end


hyperpipe #(.CYCLES(WRITE_ADDR_STAGES), .WIDTH(1+DEPTH_LOG2)) writePipe(clk, 
    {writeEnable,  writeAddr},
    {writeEnableD, writeAddrD}
);

wire readAddressStall = !isReadingOrHasJustReset;
hyperpipe #(.CYCLES(READ_ADDR_STAGES), .WIDTH(1+DEPTH_LOG2)) readAddressStallPipe(clk, 
    {readAddressStall,  nextReadAddr}, 
    {readAddressStallD, nextReadAddrD}
);

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
wire writeEnableD;
wire[DEPTH_LOG2-1:0] writeAddrD;

// Memory read side
wire readAddressStallD;
wire[DEPTH_LOG2-1:0] nextReadAddrD;

FastFIFOController #(
    .DEPTH_LOG2(DEPTH_LOG2),
    .IS_MLAB(IS_MLAB),
    .READ_ADDR_STAGES(READ_ADDR_STAGES),
    .WRITE_ADDR_STAGES(WRITE_ADDR_STAGES)
) controller (
    clk,
    rst,
    
    // input side
    writeEnable,
    usedw,
    
    // output side
    readRequest,
    dataOutValid,
    empty,
    
    // Memory write side
    writeEnableD,
    writeAddrD,
    
    // Memory read side
    readAddressStallD,
    nextReadAddrD
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
module FastFIFOPartitioned #(
    parameter WIDTH_A = 20,
    parameter WIDTH_B = 20,
    parameter DEPTH_LOG2 = 5,
    parameter IS_MLAB = 1,// = DEPTH_LOG2 <= 5
    parameter READ_ADDR_STAGES = 0,
    parameter WRITE_ADDR_STAGES = 2, 
    parameter PARTITION_B_LAG = 3
) (
    input clk,
    input rst,
    
    // input side
    input writeEnable,
    input[WIDTH_A-1:0] dataAIn,
    input[WIDTH_B-1:0] dataBIn,
    output[DEPTH_LOG2-1:0] usedw,
    
    // output side
    input readRequest,
    output[WIDTH_A-1:0] dataAOut,
    output[WIDTH_B-1:0] dataBOut,
    output dataOutValid,
    output empty,
    output eccStatusA,
    output eccStatusB
);

// Memory write side
wire writeEnableD;
wire[DEPTH_LOG2-1:0] writeAddrD;

// Memory read side
wire readAddressStallD;
wire[DEPTH_LOG2-1:0] nextReadAddrD;

FastFIFOController #(
    .DEPTH_LOG2(DEPTH_LOG2),
    .IS_MLAB(IS_MLAB),
    .READ_ADDR_STAGES(READ_ADDR_STAGES),
    .WRITE_ADDR_STAGES(WRITE_ADDR_STAGES)
) controller (
    clk,
    rst,
    
    // input side
    writeEnable,
    usedw,
    
    // output side
    readRequest,
    dataOutValid,
    empty,
    
    // Memory write side
    writeEnableD,
    writeAddrD,
    
    // Memory read side
    readAddressStallD,
    nextReadAddrD
);

wire[WIDTH_A-1:0] dataAInD;
hyperpipe #(.CYCLES(WRITE_ADDR_STAGES), .WIDTH(WIDTH_A)) writeDataAPipe(clk, dataAIn, dataAInD);

wire[WIDTH_A-1:0] dataBInD;
hyperpipe #(.CYCLES(WRITE_ADDR_STAGES), .WIDTH(WIDTH_B)) writeDataBPipe(clk, dataBIn, dataBInD);

// Memory write side
wire writeEnableD_B;
wire[DEPTH_LOG2-1:0] writeAddrD_B;

// Memory read side
wire readAddressStallD_B;
wire[DEPTH_LOG2-1:0] nextReadAddrD_B;

hyperpipe #(.CYCLES(PARTITION_B_LAG), .WIDTH(1+DEPTH_LOG2+1+DEPTH_LOG2)) partitionBMemorySignals(clk, 
    {writeEnableD,   writeAddrD,   readAddressStallD,   nextReadAddrD},
    {writeEnableD_B, writeAddrD_B, readAddressStallD_B, nextReadAddrD_B}
);

MEMORY_BLOCK #(WIDTH_A, DEPTH_LOG2, IS_MLAB) fifoMemoryA (
    .clk(clk),
    
    // Write Side
    .writeEnable(writeEnableD),
    .writeAddr(writeAddrD),
    .dataIn(dataAInD),
    
    // Read Side
    .readAddressStall(readAddressStallD),
    .readAddr(nextReadAddrD),
    .dataOut(dataAOut),
    .eccStatus(eccStatusA)
);

MEMORY_BLOCK #(WIDTH_B, DEPTH_LOG2, IS_MLAB) fifoMemoryB (
    .clk(clk),
    
    // Write Side
    .writeEnable(writeEnableD_B),
    .writeAddr(writeAddrD_B),
    .dataIn(dataBInD),
    
    // Read Side
    .readAddressStall(readAddressStallD_B),
    .readAddr(nextReadAddrD_B),
    .dataOut(dataBOut),
    .eccStatus(eccStatusB)
);

endmodule
