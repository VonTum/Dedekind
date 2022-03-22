`timescale 1ns / 1ps

`include "ipSettings_header.v"

module FIFO_MLAB #(parameter WIDTH = 16, parameter ALMOST_FULL_MARGIN = 4) (
    input clk,
    input rst,
    
    // input side
    input writeEnable,
    input[WIDTH-1:0] dataIn,
    output almostFull,
    
    // output side
    input readEnable,
    output[WIDTH-1:0] dataOut,
    output empty
);
`ifdef USE_FIFO_IP
scfifo scfifo_component (
    .clock (clk),
    .data (dataIn),
    .rdreq (readEnable),
    .wrreq (writeEnable),
    .empty (empty),
    .full (),
    .q (dataOut),
    .usedw (),
    .aclr (1'b0),
    .almost_empty (),
    .almost_full (almostFull),
    .eccstatus (),
    .sclr (rst)
);
defparam
    scfifo_component.add_ram_output_register  = "ON",
    scfifo_component.almost_full_value  = 32 - ALMOST_FULL_MARGIN,
    scfifo_component.enable_ecc  = "FALSE",
    scfifo_component.intended_device_family  = "Stratix 10",
    scfifo_component.lpm_hint  = "RAM_BLOCK_TYPE=MLAB",
    scfifo_component.lpm_numwords  = 32,
    scfifo_component.lpm_showahead  = "ON",
    scfifo_component.lpm_type  = "scfifo",
    scfifo_component.lpm_width  = WIDTH,
    scfifo_component.lpm_widthu  = 5,
    scfifo_component.overflow_checking  = "OFF",
    scfifo_component.underflow_checking  = "OFF",
    scfifo_component.use_eab  = "ON";
`else
reg[WIDTH-1:0] data[31:0];

reg[4:0] writeHead;
reg[4:0] readHead;

wire[4:0] usedw = writeHead - readHead;
assign almostFull = usedw >= 32 - ALMOST_FULL_MARGIN;

assign empty = usedw == 0;
assign dataOut = data[readHead];

always @ (posedge clk) begin
    if(rst) begin
        writeHead <= 0;
        readHead <= 0;
    end else begin
        if(writeEnable) begin
            data[writeHead] <= dataIn;
            writeHead <= writeHead + 1; // uses unsigned overflow
        end
        if(readEnable) begin
            readHead <= readHead + 1; // uses unsigned overflow
        end
    end
end
`endif
endmodule

module FIFO_M20K #(parameter WIDTH = 32, parameter DEPTH_LOG2 = 9, parameter ALMOST_FULL_MARGIN = 4) (
    input clk,
    input rst,
    
    // input side
    input writeEnable,
    input[WIDTH-1:0] dataIn,
    output almostFull,
    
    // output side
    input readEnable,
    output[WIDTH-1:0] dataOut,
    output empty,
    output eccStatus
);
`ifdef USE_FIFO_IP
wire[1:0] eccStatusWires;
assign eccStatus = eccStatusWires[1];
scfifo scfifo_component (
    .clock (clk),
    .data (dataIn),
    .rdreq (readEnable),
    .wrreq (writeEnable),
    .empty (empty),
    .full (),
    .q (dataOut),
    .usedw (),
    .aclr (1'b0),
    .almost_empty (),
    .almost_full (almostFull),
    .eccstatus (eccStatusWires),
    .sclr (rst)
);
defparam
    scfifo_component.add_ram_output_register  = "ON",
    scfifo_component.almost_full_value  = (1 << DEPTH_LOG2) - ALMOST_FULL_MARGIN,
    scfifo_component.enable_ecc  = "TRUE",
    scfifo_component.intended_device_family  = "Stratix 10",
    scfifo_component.lpm_hint  = "RAM_BLOCK_TYPE=M20K",
    scfifo_component.lpm_numwords  = (1 << DEPTH_LOG2),
    scfifo_component.lpm_showahead  = "ON",
    scfifo_component.lpm_type  = "scfifo",
    scfifo_component.lpm_width  = WIDTH,
    scfifo_component.lpm_widthu  = DEPTH_LOG2,
    scfifo_component.overflow_checking  = "OFF",
    scfifo_component.underflow_checking  = "OFF",
    scfifo_component.use_eab  = "ON";
`else
reg[WIDTH-1:0] data [(1 << DEPTH_LOG2) - 1:0];

reg[DEPTH_LOG2-1:0] writeHead;
reg[DEPTH_LOG2-1:0] readHead;

wire[DEPTH_LOG2-1:0] usedw = writeHead - readHead;
assign almostFull = usedw >= (1 << DEPTH_LOG2) - ALMOST_FULL_MARGIN;

assign empty = usedw == 0;
assign dataOut = data[readHead];
assign eccStatus = 1'b0;

always @ (posedge clk) begin
    if(rst) begin
        writeHead <= 0;
        readHead <= 0;
    end else begin
        if(writeEnable) begin
            data[writeHead] <= dataIn;
            writeHead <= writeHead + 1; // uses unsigned overflow
        end
        if(readEnable) begin
            readHead <= readHead + 1; // uses unsigned overflow
        end
    end
end
`endif
endmodule

// 1 Cycle read latency!
module DCFIFO_MLAB #(parameter WIDTH = 16) (
    // write side
    input wrclk,
    input wrrst,
    input writeEnable,
    input[WIDTH-1:0] dataIn,
    output[4:0] usedw,
    
    // read side
    input rdclk,
    input readEnable,
    output reg[WIDTH-1:0] dataOut,
    output empty
);

`ifdef USE_DC_FIFO_IP
wire[WIDTH-1:0] dataOutFromFIFO;
always @(posedge rdclk) dataOut <= dataOutFromFIFO;

dcfifo dcfifo_component (
    .aclr (wrrst),
    .data (dataIn),
    .rdclk (rdclk),
    .rdreq (readEnable),
    .wrclk (wrclk),
    .wrreq (writeEnable),
    .q (dataOutFromFIFO),
    .rdempty (empty),
    .wrusedw (usedw),
    .eccstatus (),
    .rdfull (),
    .rdusedw (),
    .wrempty (),
    .wrfull ());
defparam
    dcfifo_component.enable_ecc  = "FALSE",
    dcfifo_component.intended_device_family  = "Stratix 10",
    dcfifo_component.lpm_hint  = "RAM_BLOCK_TYPE=MLAB,MAXIMUM_DEPTH=32,DISABLE_DCFIFO_EMBEDDED_TIMING_CONSTRAINT=FALSE",
    dcfifo_component.lpm_numwords  = 32,
    dcfifo_component.lpm_showahead  = "OFF",
    dcfifo_component.lpm_type  = "dcfifo",
    dcfifo_component.lpm_width  = WIDTH,
    dcfifo_component.lpm_widthu  = 5,
    dcfifo_component.overflow_checking  = "OFF",
    dcfifo_component.rdsync_delaypipe  = 5,
    dcfifo_component.read_aclr_synch  = "ON",
    dcfifo_component.underflow_checking  = "OFF",
    dcfifo_component.use_eab  = "ON",
    dcfifo_component.write_aclr_synch  = "OFF",
    dcfifo_component.wrsync_delaypipe  = 5;
`else
reg[WIDTH-1:0] data[31:0];

reg[4:0] writeHead;
reg[4:0] readHead;
always @(posedge rdclk) dataOut <= data[readHead];

assign usedw = writeHead - readHead;

assign empty = usedw == 0;
wire full = usedw >= 30; // ((1 << DEPTH_LOG2) - 1); // uses unsigned overflow

always @(posedge wrclk) begin
    if(wrrst) begin
        writeHead <= 0;
    end else begin
        if(writeEnable) begin
            if(full) begin // Extra debug helper
                writeHead <= 5'bXXXXX;
                readHead <= 5'bXXXXX;
                $error("FIFO Overflow!");
            end
            data[writeHead] <= dataIn;
            writeHead <= writeHead + 1; // uses unsigned overflow
        end
    end
end

always @(posedge rdclk) begin
    if(wrrst) begin
        readHead <= 0;
    end else begin
        if(readEnable) begin
            if(empty) begin // Extra debug helper
                writeHead <= 5'bXXXXX;
                readHead <= 5'bXXXXX;
                $error("FIFO Underflow!");
            end
            readHead <= readHead + 1; // uses unsigned overflow
        end
    end
end

`endif
endmodule



// 2 Cycles read latency!
module DCFIFO_M20K #(parameter WIDTH = 16) (
    // write side
    input wrclk,
    input wrrst,
    input writeEnable,
    input[WIDTH-1:0] dataIn,
    output[4:0] usedw,
    
    // read side
    input rdclk,
    input readEnable,
    output reg[WIDTH-1:0] dataOut,
    output empty
);

`ifdef USE_DC_FIFO_IP
wire[WIDTH-1:0] dataOutFromFIFO;
always @(posedge rdclk) dataOut <= dataOutFromFIFO;

dcfifo dcfifo_component (
    .aclr (wrrst),
    .data (dataIn),
    .rdclk (rdclk),
    .rdreq (readEnable),
    .wrclk (wrclk),
    .wrreq (writeEnable),
    .q (dataOutFromFIFO),
    .rdempty (empty),
    .wrusedw (usedw),
    .eccstatus (),
    .rdfull (),
    .rdusedw (),
    .wrempty (),
    .wrfull ());
defparam
    dcfifo_component.enable_ecc  = "FALSE",
    dcfifo_component.intended_device_family  = "Stratix 10",
    dcfifo_component.lpm_hint  = "RAM_BLOCK_TYPE=MLAB,MAXIMUM_DEPTH=32,DISABLE_DCFIFO_EMBEDDED_TIMING_CONSTRAINT=FALSE",
    dcfifo_component.lpm_numwords  = 32,
    dcfifo_component.lpm_showahead  = "OFF",
    dcfifo_component.lpm_type  = "dcfifo",
    dcfifo_component.lpm_width  = WIDTH,
    dcfifo_component.lpm_widthu  = 5,
    dcfifo_component.overflow_checking  = "OFF",
    dcfifo_component.rdsync_delaypipe  = 5,
    dcfifo_component.read_aclr_synch  = "ON",
    dcfifo_component.underflow_checking  = "OFF",
    dcfifo_component.use_eab  = "ON",
    dcfifo_component.write_aclr_synch  = "OFF",
    dcfifo_component.wrsync_delaypipe  = 5;
`else
reg[WIDTH-1:0] data[31:0];

reg[4:0] writeHead;
reg[4:0] readHead;
always @(posedge rdclk) dataOut <= data[readHead];

assign usedw = writeHead - readHead;

assign empty = usedw == 0;
wire full = usedw >= 30; // ((1 << DEPTH_LOG2) - 1); // uses unsigned overflow

always @(posedge wrclk) begin
    if(wrrst) begin
        writeHead <= 0;
    end else begin
        if(writeEnable) begin
            if(full) begin // Extra debug helper
                writeHead <= 5'bXXXXX;
                readHead <= 5'bXXXXX;
                $error("FIFO Overflow!");
            end
            data[writeHead] <= dataIn;
            writeHead <= writeHead + 1; // uses unsigned overflow
        end
    end
end

always @(posedge rdclk) begin
    if(wrrst) begin
        readHead <= 0;
    end else begin
        if(readEnable) begin
            if(empty) begin // Extra debug helper
                writeHead <= 5'bXXXXX;
                readHead <= 5'bXXXXX;
                $error("FIFO Underflow!");
            end
            readHead <= readHead + 1; // uses unsigned overflow
        end
    end
end

`endif
endmodule

