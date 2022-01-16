`timescale 1ns / 1ps

`include "ipSettings_header.v"



// data width is one bit as properly synchronizing multiple bits is impossible. 
module synchronizer #(parameter WIDTH = 1, parameter SYNC_STAGES = 3) (
    input inClk,
    input[WIDTH-1:0] dataIn,
    
    input outClk,
    output[WIDTH-1:0] dataOut
);

reg[WIDTH-1:0] inReg;
always @(posedge inClk) inReg <= dataIn;

reg[WIDTH-1:0] syncRegs[SYNC_STAGES-2:0];
always @(posedge outClk) syncRegs[SYNC_STAGES-2] <= inReg;

generate
for(genvar i = 0; i < SYNC_STAGES-2; i=i+1) begin always @(posedge outClk) syncRegs[i] <= syncRegs[i+1]; end
endgenerate

assign dataOut = syncRegs[0];

endmodule




module dualClockFIFO #(parameter WIDTH = 160, parameter DEPTH_LOG2 = 5, parameter SYNC_STAGES = 5) (
    input aclr,
    
    input rdclk,
    input readEnable,
    output rdempty,
    output[WIDTH-1:0] dataOut,
    output[DEPTH_LOG2-1:0] rdusedw,
    
    input wrclk,
    input writeEnable,
    output wrfull,
    input[WIDTH-1:0] dataIn,
    output[DEPTH_LOG2-1:0] wrusedw
);

`ifdef USE_DC_FIFO_IP
dcfifo  dcfifo_component (
    .aclr(aclr),
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
    dcfifo_component.enable_ecc  = "FALSE",
    dcfifo_component.intended_device_family  = "Stratix 10",
    dcfifo_component.lpm_hint  = "RAM_BLOCK_TYPE=MLAB,DISABLE_DCFIFO_EMBEDDED_TIMING_CONSTRAINT=TRUE",
    dcfifo_component.lpm_numwords  = (1 << DEPTH_LOG2),
    dcfifo_component.lpm_showahead  = "OFF",
    dcfifo_component.lpm_type  = "dcfifo",
    dcfifo_component.lpm_width  = WIDTH,
    dcfifo_component.lpm_widthu  = DEPTH_LOG2,
    dcfifo_component.overflow_checking  = "OFF",
    dcfifo_component.rdsync_delaypipe  = SYNC_STAGES,
    dcfifo_component.read_aclr_synch  = "ON",
    dcfifo_component.underflow_checking  = "ON",
    dcfifo_component.use_eab  = "ON",
    dcfifo_component.write_aclr_synch  = "ON",
    dcfifo_component.wrsync_delaypipe  = SYNC_STAGES;
    
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
assign rdempty = wrusedw == 0;
assign wrfull = wrusedw == (1 << DEPTH_LOG2) - 1;


always @(posedge rdclk or posedge aclr) begin
    if(aclr) begin
        readHead <= 0;
    end else begin
        if(readEnable & !rdempty) begin
            readHead <= readHead + 1; // uses unsigned overflow
        end
    end
end

reg[WIDTH-1:0] dataOutReg;
assign dataOut = dataOutReg;
always @(posedge rdclk) dataOutReg <= memory[readHead];

always @(posedge wrclk or posedge aclr) begin
    if(aclr) begin
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
