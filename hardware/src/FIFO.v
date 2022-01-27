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


module FIFO_MEMORY #(parameter WIDTH = 20, parameter DEPTH_LOG2 = 5, parameter IS_MLAB = 1/* = DEPTH_LOG2 <= 5*/) (
    input clk,
    
    // Write Side
    input writeEnable,
    input[DEPTH_LOG2-1:0] writeAddr,
    input[WIDTH-1:0] dataIn,
    
    // Read Side
    input readAddressStall,
    input[DEPTH_LOG2-1:0] readAddr,
    output[WIDTH-1:0] dataOut,
    output eccStatus
);

`ifdef USE_FIFO_MEMORY_IP

localparam BLOCK_SIZE = IS_MLAB ? 20 : 32;
localparam WIDTH_ENLARGED_TO_BLOCK_SIZE = WIDTH + (BLOCK_SIZE - (WIDTH % BLOCK_SIZE)) % BLOCK_SIZE;

wire[WIDTH_ENLARGED_TO_BLOCK_SIZE-1:0] dataInWide;
wire[WIDTH_ENLARGED_TO_BLOCK_SIZE-1:0] dataOutWide;
assign dataInWide[WIDTH-1:0] = dataIn;
generate
if(WIDTH_ENLARGED_TO_BLOCK_SIZE != WIDTH) assign dataInWide[WIDTH_ENLARGED_TO_BLOCK_SIZE-1:WIDTH] = 0;
endgenerate
assign dataOut = dataOutWide[WIDTH-1:0];

generate
if(IS_MLAB) begin

/* MLAB */
altera_syncram  altera_syncram_component (
    .clock0 (clk),
    .address_a (writeAddr),
    .address_b (readAddr),
    .addressstall_b (readAddressStall),
    .data_a (dataInWide),
    .wren_a (writeEnable),
    .q_b (dataOutWide),
    .aclr0 (1'b0),
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
    .data_b ({WIDTH_ENLARGED_TO_BLOCK_SIZE{1'b1}}),
    .eccencbypass (1'b0),
    .eccencparity (8'b0),
    .eccstatus (),
    .q_a (),
    .rden_a (1'b1),
    .rden_b (1'b1),
    .sclr (1'b0),
    .wren_b (1'b0));
defparam
    altera_syncram_component.address_aclr_b  = "NONE",
    altera_syncram_component.address_reg_b  = "CLOCK0",
    altera_syncram_component.clock_enable_input_a  = "BYPASS",
    altera_syncram_component.clock_enable_input_b  = "BYPASS",
    altera_syncram_component.clock_enable_output_b  = "BYPASS",
    altera_syncram_component.intended_device_family  = "Stratix 10",
    altera_syncram_component.lpm_type  = "altera_syncram",
    altera_syncram_component.numwords_a  = (1 << DEPTH_LOG2),
    altera_syncram_component.numwords_b  = (1 << DEPTH_LOG2),
    altera_syncram_component.operation_mode  = "DUAL_PORT",
    altera_syncram_component.outdata_aclr_b  = "NONE",
    altera_syncram_component.outdata_sclr_b  = "NONE",
    altera_syncram_component.outdata_reg_b  = "UNREGISTERED",
    altera_syncram_component.power_up_uninitialized  = "FALSE",
    altera_syncram_component.ram_block_type  = "MLAB",
    altera_syncram_component.read_during_write_mode_mixed_ports  = "DONT_CARE",
    altera_syncram_component.widthad_a  = DEPTH_LOG2,
    altera_syncram_component.widthad_b  = DEPTH_LOG2,
    altera_syncram_component.width_a  = WIDTH_ENLARGED_TO_BLOCK_SIZE,
    altera_syncram_component.width_b  = WIDTH_ENLARGED_TO_BLOCK_SIZE,
    altera_syncram_component.width_byteena_a  = 1,
    altera_syncram_component.enable_force_to_zero  = "FALSE",
    altera_syncram_component.enable_ecc  = "FALSE";

assign eccStatus = 1'b0;
end else begin

/* M20K */
wire[(IS_MLAB ? -1 : 1):0] eccStatusWire;
assign eccStatus = eccStatusWire[1];

altera_syncram  altera_syncram_component (
    .clock0 (clk),
    .address_a (writeAddr),
    .address_b (readAddr),
    .addressstall_b (readAddressStall),
    .data_a (dataInWide),
    .wren_a (writeEnable),
    .q_b (dataOutWide),
    .aclr0 (1'b0),
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
    .data_b ({WIDTH_ENLARGED_TO_BLOCK_SIZE{1'b1}}),
    .eccencbypass (1'b0),
    .eccencparity (8'b0),
    .eccstatus (eccStatusWire),
    .q_a (),
    .rden_a (1'b1),
    .rden_b (1'b1),
    .sclr (1'b0),
    .wren_b (1'b0));
defparam
    altera_syncram_component.address_aclr_b  = "NONE",
    altera_syncram_component.address_reg_b  = "CLOCK0",
    altera_syncram_component.clock_enable_input_a  = "BYPASS",
    altera_syncram_component.clock_enable_input_b  = "BYPASS",
    altera_syncram_component.clock_enable_output_b  = "BYPASS",
    altera_syncram_component.intended_device_family  = "Stratix 10",
    altera_syncram_component.lpm_type  = "altera_syncram",
    altera_syncram_component.numwords_a  = (1 << DEPTH_LOG2),
    altera_syncram_component.numwords_b  = (1 << DEPTH_LOG2),
    altera_syncram_component.operation_mode  = "DUAL_PORT",
    altera_syncram_component.outdata_aclr_b  = "NONE",
    altera_syncram_component.outdata_sclr_b  = "NONE",
    altera_syncram_component.outdata_reg_b  = "CLOCK0",
    altera_syncram_component.power_up_uninitialized  = "FALSE",
    altera_syncram_component.ram_block_type  = "M20K",
    altera_syncram_component.read_during_write_mode_mixed_ports  = "DONT_CARE",
    altera_syncram_component.widthad_a  = DEPTH_LOG2,
    altera_syncram_component.widthad_b  = DEPTH_LOG2,
    altera_syncram_component.width_a  = WIDTH_ENLARGED_TO_BLOCK_SIZE,
    altera_syncram_component.width_b  = WIDTH_ENLARGED_TO_BLOCK_SIZE,
    altera_syncram_component.width_byteena_a  = 1,
    altera_syncram_component.enable_force_to_zero  = "TRUE",
    altera_syncram_component.enable_ecc  = "TRUE",
    altera_syncram_component.ecc_pipeline_stage_enabled  = "TRUE",
    altera_syncram_component.enable_ecc_encoder_bypass  = "FALSE",
    altera_syncram_component.width_eccstatus  = 2;

end
endgenerate

`else

reg[WIDTH-1:0] memory[(1 << DEPTH_LOG2) - 1:0];

reg[DEPTH_LOG2-1:0] writeAddrReg;
reg[WIDTH-1:0] writeDataReg;
reg writeEnableReg;

reg[DEPTH_LOG2-1:0] readAddrReg;

always @(posedge clk) begin
    if(!readAddressStall) readAddrReg <= readAddr;
    
    if(writeEnableReg) begin
        memory[writeAddrReg] <= writeDataReg;
    end
    
    writeAddrReg <= writeAddr;
    writeDataReg <= dataIn;
    writeEnableReg <= writeEnable;
end

wire[WIDTH-1:0] dataFromMem = (writeEnableReg && writeAddrReg == readAddrReg) ? {WIDTH{1'bX}} : memory[readAddrReg];
hyperpipe #(.CYCLES(IS_MLAB ? 0 : 2), .WIDTH(WIDTH)) dataOutPipe(clk, dataFromMem, dataOut);

assign hasBitError = 1'b0;

`endif

endmodule

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

wire[DEPTH_LOG2-1:0] readsValidUpTo; 
hyperpipe #(.CYCLES(WRITE_ADDR_STAGES+1), .WIDTH(DEPTH_LOG2)) readsValidUpToPipe(clk, writeAddr + 1, readsValidUpTo);
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
            writeAddr <= writeAddr + 1;
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

FIFO_MEMORY #(WIDTH, DEPTH_LOG2, IS_MLAB) fifoMemory (
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

FIFO_MEMORY #(WIDTH_A, DEPTH_LOG2, IS_MLAB) fifoMemoryA (
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

FIFO_MEMORY #(WIDTH_B, DEPTH_LOG2, IS_MLAB) fifoMemoryB (
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

