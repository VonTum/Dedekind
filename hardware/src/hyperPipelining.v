`timescale 1ns / 1ps

`include "ipSettings_header.v"

(* altera_attribute = "-name SYNCHRONIZER_IDENTIFICATION off" *)
(* altera_attribute = "-name AUTO_SHIFT_REGISTER_RECOGNITION off" *)
module hyperpipe 
#(parameter CYCLES = 1, parameter WIDTH = 1) 
(
	input clk,
	input [WIDTH-1:0] din,
	output [WIDTH-1:0] dout
);

	generate if (CYCLES==0) begin : GEN_COMB_INPUT
		assign dout = din;
	end else
	begin : GEN_REG_INPUT  
		integer i;
		reg [WIDTH-1:0] R_data [CYCLES-1:0];
        
		always @ (posedge clk) 
		begin   
			R_data[0] <= din;      
			for(i = 1; i < CYCLES; i = i + 1) 
            	R_data[i] <= R_data[i-1];
		end
		assign dout = R_data[CYCLES-1];
	end
	endgenerate  

endmodule

// This is for generating shift register pipes
(* altera_attribute = "-name SYNCHRONIZER_IDENTIFICATION off" *)
(* altera_attribute = "-name AUTO_SHIFT_REGISTER_RECOGNITION on" *) 
module shiftRegister 
#(parameter CYCLES = 1, parameter WIDTH = 1) 
(
	input clk,
	input [WIDTH-1:0] dataIn,
	output [WIDTH-1:0] dataOut
);

	generate if (CYCLES==0) begin : GEN_COMB_INPUT
		assign dataOut = dataIn;
	end else
`ifdef USE_SHIFTREG_IP
	if(CYCLES >= 5 && CYCLES * WIDTH >= 41) begin
        wire[WIDTH-1:0] unusedTap;
        wire[WIDTH-1:0] dataOutDirectlyFromMemory;
        altshift_taps  ALTSHIFT_TAPS_component (
            .clock (clk),
            .shiftin (dataIn),
            .shiftout (dataOutDirectlyFromMemory),
            .taps (unusedTap)
            // synopsys translate_off
				, // These ports may not be generated in some situations, due to the above directive. Store comma with them to not cause an error
            .aclr (),
            .clken (),
            .sclr ()
            // synopsys translate_on
        );
        // Separate out one of the stages into a register at the end to improve timing
        reg[WIDTH-1:0] dataOutReg;
        always @(posedge clk) dataOutReg <= dataOutDirectlyFromMemory;
        assign dataOut = dataOutReg;
defparam
		ALTSHIFT_TAPS_component.intended_device_family  = "Stratix 10",
		ALTSHIFT_TAPS_component.lpm_hint  = CYCLES <= 32 ? "RAM_BLOCK_TYPE=MLAB" : "RAM_BLOCK_TYPE=M20K",
		ALTSHIFT_TAPS_component.lpm_type  = "altshift_taps",
		ALTSHIFT_TAPS_component.number_of_taps  = 1,
		ALTSHIFT_TAPS_component.tap_distance  = CYCLES-1,
		ALTSHIFT_TAPS_component.width  = WIDTH;
	end else
`endif
	begin : GEN_REG_INPUT  
		integer i;
		reg [WIDTH-1:0] R_data [CYCLES-1:0];
        
		always @ (posedge clk) 
		begin   
			R_data[0] <= dataIn;      
			for(i = 1; i < CYCLES; i = i + 1) 
            	R_data[i] <= R_data[i-1];
		end
		assign dataOut = R_data[CYCLES-1];
	end
	endgenerate  

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
