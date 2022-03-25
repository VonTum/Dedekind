`timescale 1ns / 1ps

`include "ipSettings_header.v"

module dualClockFIFO #(parameter WIDTH = 60, parameter DEPTH_LOG2 = 5, parameter SYNC_STAGES = 5) (
    input wrclk,
    input writeEnable,
    output wrfull,
    input[WIDTH-1:0] dataIn,
    output[DEPTH_LOG2-1:0] wrusedw,
    
    input rdclk,
    input rdclr, // aclr is synchronized to read clock
    input readEnable,
    output rdempty,
    output[WIDTH-1:0] dataOut,
    output[DEPTH_LOG2-1:0] rdusedw
);

`ifdef USE_DC_FIFO_IP

dcfifo  dcfifo_component (
    .aclr(rdclr),
    .data(dataIn),
    .rdclk(rdclk),
    .rdreq(readEnable),
    .wrclk(wrclk),
    .wrreq(writeEnable),
    .q(dataOut),
    .rdempty(rdempty),
    .wrfull(wrfull),
    .wrusedw(wrusedw),
    .rdusedw(rdusedw),
    .eccstatus(),
    .rdfull(),
    .wrempty()
);
defparam
    dcfifo_component.intended_device_family  = "Stratix 10",
    dcfifo_component.lpm_hint  = "RAM_BLOCK_TYPE=MLAB,DISABLE_DCFIFO_EMBEDDED_TIMING_CONSTRAINT=FALSE",
    dcfifo_component.lpm_numwords  = (1 << DEPTH_LOG2),
    dcfifo_component.lpm_showahead  = "OFF",
    dcfifo_component.lpm_type  = "dcfifo",
    dcfifo_component.lpm_width  = WIDTH,
    dcfifo_component.lpm_widthu  = DEPTH_LOG2,
    dcfifo_component.overflow_checking  = "OFF",
    dcfifo_component.underflow_checking  = "ON",
    dcfifo_component.use_eab  = "ON",
    dcfifo_component.read_aclr_synch  = "OFF",
    dcfifo_component.write_aclr_synch  = "ON",
    dcfifo_component.rdsync_delaypipe  = SYNC_STAGES,
    dcfifo_component.wrsync_delaypipe  = SYNC_STAGES,
    dcfifo_component.enable_ecc  = "FALSE"; // ECC is too slow for 600MHz!
    
`else

// Used for simulation only, definitely not good for synthesis!

reg[WIDTH-1:0] memory [(1 << DEPTH_LOG2) - 1:0]; // one extra element to differentiate between empty fifo and full

reg[DEPTH_LOG2-1:0] writeHead;
wire[DEPTH_LOG2-1:0] writeHeadSyncToRead;
synchronizer #(.WIDTH(DEPTH_LOG2), .SYNC_STAGES(SYNC_STAGES)) writeSyncPipe(wrclk, writeHead, rdclk, writeHeadSyncToRead);
reg[DEPTH_LOG2-1:0] readHead;
wire[DEPTH_LOG2-1:0] readHeadSyncToWrite;
synchronizer #(.WIDTH(DEPTH_LOG2), .SYNC_STAGES(SYNC_STAGES)) readSyncPipe(rdclk, readHead, wrclk, readHeadSyncToWrite);

assign wrusedw = writeHead - readHeadSyncToWrite;
assign rdusedw = writeHeadSyncToRead - readHead;
assign rdempty = writeHeadSyncToRead == readHead;
assign wrfull = wrusedw == (1 << DEPTH_LOG2) - 1;

reg[WIDTH-1:0] dataOutReg;
assign dataOut = dataOutReg;

wire isReading = readEnable & !rdempty;
always @(posedge rdclk) begin
    if(rdclr) begin
        readHead <= 0;
        dataOutReg <= {WIDTH{1'bX}};
    end else begin
        if(isReading) begin
            dataOutReg <= memory[readHead];
            readHead <= readHead + 1; // uses unsigned overflow
        end
    end
end

wire wrclr;
synchronizer clrSync(rdclk, rdclr, wrclk, wrclr);
always @(posedge wrclk) begin
    if(wrclr) begin
        writeHead <= 0;
    end else begin
        if(writeEnable) begin
            memory[writeHead] <= dataIn;
            writeHead <= writeHead + 1; // uses unsigned overflow
        end
    end
end

`endif

endmodule


module dualClockFIFOWithDataValid #(parameter WIDTH = 160, parameter DEPTH_LOG2 = 5, parameter SYNC_STAGES = 5) (
    input wrclk,
    input writeEnable,
    input[WIDTH-1:0] dataIn,
    output[DEPTH_LOG2-1:0] wrusedw,
    
    input rdclk,
    input rdclr,
    input readEnable,
    output reg dataOutValid,
    output[WIDTH-1:0] dataOut
);

wire rdclrLocal; // Manual reset tree, can't use constraints to have it generate it for me. 
hyperpipe #(.CYCLES(2)) rstPipe(rdclk, rdclr, rdclrLocal);

wire rdempty;
always @(posedge rdclk) begin
    if(rdclrLocal) begin
        dataOutValid <= 1'b0;
    end else begin
        dataOutValid <= readEnable && !rdempty;
    end
end

dualClockFIFO #(.WIDTH(WIDTH), .DEPTH_LOG2(DEPTH_LOG2), .SYNC_STAGES(SYNC_STAGES)) fifo (
    .wrclk(wrclk),
    .writeEnable(writeEnable),
    .wrfull(), // unused, writes should be paused ahead of time
    .dataIn(dataIn),
    .wrusedw(wrusedw),
    
    .rdclk(rdclk),
    .rdclr(rdclrLocal),
    .readEnable(readEnable),
    .rdempty(rdempty),
    .dataOut(dataOut),
    .rdusedw() // unused, reads should poll the dataOutValid flag
);

endmodule
