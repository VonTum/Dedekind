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


module MLAB_RAM #(parameter WIDTH = 20) (
    input clk,
    input rst,
    
    // Write Side
    input writeEnable,
    input[4:0] writeAddr,
    input[WIDTH-1:0] dataIn,
    
    // Read Side
    input readAddressStall,
    input[4:0] readAddr,
    output[WIDTH-1:0] dataOut
);

`ifdef USE_MLAB_IP

localparam BLOCK_SIZE = 20; // For MLAB
localparam WIDTH_ENLARGED_TO_BLOCK_SIZE = WIDTH + (BLOCK_SIZE - (WIDTH % BLOCK_SIZE)) % BLOCK_SIZE;

wire[WIDTH_ENLARGED_TO_BLOCK_SIZE-1:0] dataInWide;
wire[WIDTH_ENLARGED_TO_BLOCK_SIZE-1:0] dataOutWide;
assign dataInWide[WIDTH-1:0] = dataIn;
assign dataInWide[WIDTH_ENLARGED_TO_BLOCK_SIZE-1:WIDTH] = 0;
assign dataOut = dataOutWide[WIDTH-1:0];

altera_syncram  altera_syncram_component (
    .clock0 (clk),
    .aclr0 (rst),
    .address_a (writeAddr),
    .address_b (readAddr),
    .addressstall_b (readAddressStall),
    .data_a (dataInWide),
    .wren_a (writeEnable),
    .q_b (dataOutWide),
    .aclr1 (1'b0),
    .address2_a (1'b1),
    .address2_b (1'b1),
    .addressstall_a (1'b0),
    .byteena_a (1'b1),
    .byteena_b (1'b1),
    .clock1 (1'b1),
    .clocken0 (1'b1),
    .clocken1 (1'b1),
    .clocken2 (1'b1),
    .clocken3 (1'b1),
    .data_b ({160{1'b1}}),
    .eccencbypass (1'b0),
    .eccencparity (8'b0),
    .eccstatus (),
    .q_a (),
    .rden_a (1'b1),
    .rden_b (1'b1),
    .sclr (1'b0),
    .wren_b (1'b0));
defparam
    altera_syncram_component.address_aclr_b  = "CLEAR0",
    altera_syncram_component.address_reg_b  = "CLOCK0",
    altera_syncram_component.clock_enable_input_a  = "BYPASS",
    altera_syncram_component.clock_enable_input_b  = "BYPASS",
    altera_syncram_component.clock_enable_output_b  = "BYPASS",
    altera_syncram_component.intended_device_family  = "Stratix 10",
    altera_syncram_component.lpm_type  = "altera_syncram",
    altera_syncram_component.maximum_depth  = 32,
    altera_syncram_component.numwords_a  = 32,
    altera_syncram_component.numwords_b  = 32,
    altera_syncram_component.operation_mode  = "DUAL_PORT",
    altera_syncram_component.outdata_aclr_b  = "NONE",
    altera_syncram_component.outdata_sclr_b  = "NONE",
    altera_syncram_component.outdata_reg_b  = "UNREGISTERED",
    altera_syncram_component.power_up_uninitialized  = "FALSE",
    altera_syncram_component.ram_block_type  = "MLAB",
    altera_syncram_component.read_during_write_mode_mixed_ports  = "DONT_CARE",
    altera_syncram_component.widthad_a  = 5,
    altera_syncram_component.widthad_b  = 5,
    altera_syncram_component.width_a  = WIDTH_ENLARGED_TO_BLOCK_SIZE,
    altera_syncram_component.width_b  = WIDTH_ENLARGED_TO_BLOCK_SIZE,
    altera_syncram_component.width_byteena_a  = 1;

`else

reg[WIDTH-1:0] memory[31:0];

reg[4:0] writeAddrReg;
reg[WIDTH-1:0] writeDataReg;
reg writeEnableReg;

reg[4:0] readAddrReg;

always @(posedge clk or posedge rst) begin
    if(rst)
        readAddrReg <= 0;
    else
        if(!readAddressStall) readAddrReg <= readAddr;
end

always @(posedge clk) begin
    writeAddrReg <= writeAddr;
    writeDataReg <= dataIn;
    writeEnableReg <= writeEnable;
    
    if(writeEnableReg) begin
        memory[writeAddrReg] <= writeDataReg;
    end
end

assign dataOut = (writeEnableReg && writeAddrReg == readAddrReg) ? {WIDTH{1'bX}} : memory[readAddrReg];

`endif

endmodule

// Has a read latency of 0 cycles after assertion of readRequest. (Then if the fifo had data dataOutValid should be asserted)
module FastFIFO #(parameter WIDTH = 20) (
    input clk,
    input rst,
     
    // input side
    input writeEnable,
    input[WIDTH-1:0] dataIn,
    output[4:0] usedw,
    
    // output side
    input readRequest,
    output[WIDTH-1:0] dataOut,
    output dataOutValid
);

reg[4:0] nextReadAddr;
reg[4:0] writeAddr;

reg[4:0] readsValidUpTo; always @(posedge clk) readsValidUpTo <= writeAddr + 1;
assign usedw = readsValidUpTo - nextReadAddr;

wire fifoHasData = readsValidUpTo != nextReadAddr;
wire isReading = readRequest & fifoHasData;
assign dataOutValid = isReading;

always @(posedge clk) begin
    if(rst) begin
        // also resets readAddr field within the MLAB to 0
        nextReadAddr <= 1;
        writeAddr <= 0;
    end else begin
        if(isReading) begin
            nextReadAddr <= nextReadAddr + 1;
        end
        
        if(writeEnable) begin
            writeAddr <= writeAddr + 1;
        end
    end
end

MLAB_RAM #(WIDTH) fifoMemory (
    .clk(clk),
    .rst(rst),
    
    // Write Side
    .writeEnable(writeEnable),
    .writeAddr(writeAddr),
    .dataIn(dataIn),
    
    // Read Side
    .readAddressStall(!isReading),
    .readAddr(nextReadAddr),
    .dataOut(dataOut)
);

endmodule
