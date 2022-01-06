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
