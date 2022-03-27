`timescale 1ns / 1ps

(* altera_attribute = "-name AUTO_SHIFT_REGISTER_RECOGNITION off; -name SYNCHRONIZER_IDENTIFICATION off" *)
module hyperpipe 
#(parameter CYCLES = 1, parameter WIDTH = 1, parameter MAX_FAN = 80) 
(
    input clk,
    input [WIDTH-1:0] din,
    output [WIDTH-1:0] dout
);

generate
if (CYCLES == 0) begin : GEN_COMB_INPUT
    assign dout = din;
end else
begin : GEN_REG_INPUT
    integer i;
    (* maxfan = MAX_FAN *) reg[WIDTH-1:0] R_data [CYCLES-1:0];
    
    always @(posedge clk) begin
        R_data[0] <= din;
        for(i = 1; i < CYCLES; i = i + 1) R_data[i] <= R_data[i-1];
    end
    assign dout = R_data[CYCLES-1];
end
endgenerate

endmodule

(* altera_attribute = "-name AUTO_SHIFT_REGISTER_RECOGNITION off; -name SYNCHRONIZER_IDENTIFICATION auto" *)
module hyperpipeDualClock 
#(parameter CYCLES_A = 1, parameter CYCLES_B = 1, parameter WIDTH = 1) 
(
    input clkA,
    input clkB,
    input [WIDTH-1:0] din,
    output [WIDTH-1:0] dout
);

wire[WIDTH-1:0] dIntermediate;
generate
if (CYCLES_A == 0) begin : GEN_COMB_INPUT_A
    assign dIntermediate = din;
end else
begin : GEN_REG_INPUT_A
    integer i;
    reg[WIDTH-1:0] R_data_A [CYCLES_A-1:0];
    
    always @(posedge clkA) begin
        R_data_A[0] <= din;
        for(i = 1; i < CYCLES_A; i = i + 1) R_data_A[i] <= R_data_A[i-1];
    end
    assign dIntermediate = R_data_A[CYCLES_A-1];
end
if (CYCLES_B == 0) begin : GEN_COMB_INPUT_B
    assign dout = dIntermediate;
end else
begin : GEN_REG_INPUT_B
    integer i;
    reg[WIDTH-1:0] R_data_B [CYCLES_B-1:0];
    
    always @(posedge clkB) begin   
        R_data_B[0] <= dIntermediate;
        for(i = 1; i < CYCLES_B; i = i + 1) R_data_B[i] <= R_data_B[i-1];
    end
    assign dout = R_data_B[CYCLES_B-1];
end
endgenerate
endmodule

module shiftRegister #(parameter CYCLES = 1, parameter WIDTH = 1) (
    input clk,
    input[WIDTH-1:0] dataIn,
    output[WIDTH-1:0] dataOut
);

localparam ITER_UPTO_INCLUDING = CYCLES-2;
localparam BITWIDTH = $clog2(ITER_UPTO_INCLUDING+1);

reg[BITWIDTH-1:0] curIndex = 0; always @(posedge clk) curIndex <= curIndex >= ITER_UPTO_INCLUDING ? 0 : curIndex + 1;
reg[BITWIDTH-1:0] curIndexD; always @(posedge clk) curIndexD <= curIndex;

MEMORY_MLAB #(
    .WIDTH(WIDTH),
    .DEPTH_LOG2(BITWIDTH),
    .READ_DURING_WRITE("DONT_CARE"),
    .OUTPUT_REGISTER(1)
) mem (
    .clk(clk),
  
    // Write Side
    .writeEnable(1'b1),
    .writeAddr(curIndexD),
    .dataIn(dataIn),
  
    // Read Side
    .readAddr(curIndex),
    .dataOut(dataOut)
);
endmodule

module shiftRegister_M20K #(parameter CYCLES = 1, parameter WIDTH = 1) (
    input clk,
    input[WIDTH-1:0] dataIn,
    output[WIDTH-1:0] dataOut,
    output eccStatus
);

generate if (CYCLES <= 3) begin : TOO_FEW_CYCLES
    hyperpipe #(.CYCLES(CYCLES), .WIDTH(WIDTH)) pipe(clk, dataIn, dataOut);
    assign eccStatus = 1'b0;
end else begin : M20K_MEMORY
    localparam ITER_UPTO_INCLUDING = CYCLES-3;
    localparam BITWIDTH = $clog2(ITER_UPTO_INCLUDING+1);

    reg[BITWIDTH-1:0] curIndex = 0; always @(posedge clk) curIndex <= curIndex >= ITER_UPTO_INCLUDING ? 0 : curIndex + 1;
    reg[BITWIDTH-1:0] curIndexD; always @(posedge clk) curIndexD <= curIndex;
    
    MEMORY_M20K #(
        .WIDTH(WIDTH),
        .DEPTH_LOG2(BITWIDTH),
        .READ_DURING_WRITE("DONT_CARE")
    ) mem (
        .clk(clk),
        
        // Write Side
        .writeEnable(1'b1),
        .writeAddr(curIndexD),
        .dataIn(dataIn),
        
        // Read Side
        .readEnable(1'b1),
        .readAddr(curIndex),
        .dataOut(dataOut),
        .eccStatus(eccStatus)
    );
end endgenerate
endmodule

module enabledShiftRegister
#(parameter CYCLES = 1, parameter WIDTH = 1, parameter RESET_VALUE = 0) 
(
    input clk,
    input clkEn,
    input rst,
    input [WIDTH-1:0] din,
    output [WIDTH-1:0] dout
);

    generate if (CYCLES==0) begin : GEN_COMB_INPUT
        assign dout = din;
    end 
    else begin : GEN_REG_INPUT  
        integer i;
        reg [WIDTH-1:0] R_data [CYCLES-1:0];
        
        always @ (posedge clk) if(rst) 
            for(i = 0; i < CYCLES; i = i + 1) 
                R_data[i] <= RESET_VALUE;
        else if(clkEn) begin   
            R_data[0] <= din;      
            for(i = 1; i < CYCLES; i = i + 1) 
                R_data[i] <= R_data[i-1];
        end
        assign dout = R_data[CYCLES-1];
    end
    endgenerate  

endmodule

// Quartus Prime Verilog Template
//
// Hyper-Pipelining Variable Latency Module

module hyperpipe_vlat 
#(parameter WIDTH = 1, parameter MAX_PIPE = 100) // Valid range for MAX_PIPE: 0 to 100 inclusive
(
    input clk,
    input  [WIDTH-1:0] din,
    output [WIDTH-1:0] dout
);
    
    // Capping the value of MAX_PIPE to 100 because MAX_PIPE > 100 could cause errors
    localparam MAX_PIPE_CAPPED = (MAX_PIPE > 100) ? 100 : ((MAX_PIPE < 0) ? 0 : MAX_PIPE);

    // Converting MAX_PIPE_CAPPED to string so it can be used as a string when setting altera_attribute
    localparam MAX_PIPE_STR = {((MAX_PIPE_CAPPED / 100) % 10) + 8'd48, ((MAX_PIPE_CAPPED / 10) % 10) + 8'd48, (MAX_PIPE_CAPPED % 10) + 8'd48};

    (* altera_attribute = {"-name ADV_NETLIST_OPT_ALLOWED NEVER_ALLOW; -name HYPER_RETIMER_ADD_PIPELINING ", MAX_PIPE_STR} *)
    reg [WIDTH-1:0] vlat_r /* synthesis preserve */;

    always @ (posedge clk) begin
        vlat_r <= din;
    end

    assign dout = vlat_r;

endmodule
