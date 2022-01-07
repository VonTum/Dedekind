`timescale 1ns / 1ps

`include "ipSettings_header.v"

module FIFO #(parameter WIDTH = 16, parameter DEPTH_LOG2 = 5) (
    input clk,
    input rst,
     
    // input side
    input writeEnable,
    input[WIDTH-1:0] dataIn,
    output full,
    
    // output side
    input readEnable,
    output[WIDTH-1:0] dataOut,
    output empty,
     
    output[DEPTH_LOG2-1:0] usedw
);
`ifdef USE_FIFO_IP
scfifo  scfifo_component (
    .clock (clk),
    .data (dataIn),
    .rdreq (readEnable),
    .wrreq (writeEnable),
    .empty (empty),
    .full (full),
    .q (dataOut),
    .usedw (usedw),
    .aclr (1'b0),
    .almost_empty (),
    .almost_full (),
    .eccstatus (),
    .sclr (rst)
);
defparam
    scfifo_component.add_ram_output_register  = "ON",
    scfifo_component.enable_ecc  = "FALSE",
    scfifo_component.intended_device_family  = "Stratix 10",
    scfifo_component.lpm_hint  = DEPTH_LOG2 <= 5 ? "RAM_BLOCK_TYPE=MLAB" : "RAM_BLOCK_TYPE=M20K",
    scfifo_component.lpm_numwords  = (1 << DEPTH_LOG2),
    scfifo_component.lpm_showahead  = "ON",
    scfifo_component.lpm_type  = "scfifo",
    scfifo_component.lpm_width  = WIDTH,
    scfifo_component.lpm_widthu  = DEPTH_LOG2,
    scfifo_component.overflow_checking  = "OFF",
    scfifo_component.underflow_checking  = "OFF",
    scfifo_component.use_eab  = "ON";
`else
reg[WIDTH-1:0] data [(1 << DEPTH_LOG2) - 1:0]; // one extra element to differentiate between empty fifo and full

reg[DEPTH_LOG2-1:0] writeHead;
reg[DEPTH_LOG2-1:0] readHead;

assign usedw = writeHead - readHead;

assign empty = usedw == 0;
assign full = usedw == -1; // ((1 << DEPTH_LOG2) - 1); // uses unsigned overflow
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



// Has a read latency of 3 cycles after assertion of readEnable. (Then if the fifo had data dataOutValid should be asserted)
module FastFIFO #(parameter WIDTH = 20) (
    input clk,
    input rst,
     
    // input side
    input writeEnable,
    input[WIDTH-1:0] dataIn,
    output full,
    output ready,
    output[4:0] usedw_writeSide,
    
    // output side
    input readEnable,
    output[WIDTH-1:0] dataOut,
    output empty,
    output dataOutValid,
    output[4:0] usedw_readSide
);


`ifdef USE_FIFO2_IP

localparam FIFO_RADIX = 20; // For MLAB

localparam FIFO2_WIDTH_ENLARGED_TO_RADIX = WIDTH + (FIFO_RADIX - (WIDTH % FIFO_RADIX)) % FIFO_RADIX;

wire[FIFO2_WIDTH_ENLARGED_TO_RADIX-1:0] dataInWide;
wire[FIFO2_WIDTH_ENLARGED_TO_RADIX-1:0] dataOutWide;
assign dataInWide[WIDTH-1:0] = dataIn;
assign dataInWide[FIFO2_WIDTH_ENLARGED_TO_RADIX-1:WIDTH] = 0;
assign dataOut = dataOutWide[WIDTH-1:0];

alt_pipeline_dcfifo #(
    .DATAWIDTH(FIFO2_WIDTH_ENLARGED_TO_RADIX),
    .RAM_BLK_TYPE("MLAB"),
    .USE_ACLR_PORT(0),
    .RAM_WRPTR_DUPLICATE(0),
    .RAM_RDPTR_DUPLICATE(0)
) scfifo_mode (
    .w_clk        (clk),
    .r_clk        (clk),
    .w_aclr       (1'b0),
    .r_aclr       (1'b0),
    .w_sclr       (rst),
    .r_sclr       (rst),
    .w_req        (writeEnable),
    .w_data       (dataInWide),
    .w_usedw      (usedw_writeSide),
    .w_full       (full),
    .w_ready      (ready),
    .r_req        (readEnable),
    .r_data       (dataOutWide),
    .r_usedw      (usedw_readSide),
    .r_empty      (empty),
    .r_valid      (dataOutValid)
);
`else

wire writeEnableDelayed;
wire[WIDTH-1:0] dataInDelayed;
hyperpipe #(.CYCLES(2), .WIDTH(WIDTH+1)) writeDelayPipe (clk,
    {writeEnable, dataIn},
    {writeEnableDelayed, dataInDelayed}
);

wire[WIDTH-1:0] dOutPreDelay;
wire dataOutValidPreDelay = readEnable & !empty;
hyperpipe #(.CYCLES(3), .WIDTH(WIDTH+1+5)) readDelayPipe (clk,
    {dOutPreDelay, dataOutValidPreDelay, usedw_writeSide},
    {dataOut, dataOutValid, usedw_readSide}
);

assign ready = !full;

FIFO #(.WIDTH(WIDTH), .DEPTH_LOG2(5)) fifo (
    .clk(clk),
    .rst(rst),
     
    // input side
    .writeEnable(writeEnableDelayed & !full), // Write protection
    .dataIn(dataInDelayed),
    .full(full),
    
    // output side
    .readEnable(dataOutValidPreDelay), // Read protection
    .dataOut(dOutPreDelay),
    .empty(empty),
     
    .usedw(usedw_writeSide)
);

`endif

endmodule
