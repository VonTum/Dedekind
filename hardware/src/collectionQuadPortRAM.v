`timescale 1ns / 1ps

module collectionQuadPortRAM #(parameter ADDR_BITS = 13) (
    input clk,
    input[9:0] data_a,
    input[9:0] data_b,
    input[ADDR_BITS-1:0] read_address_b,
    input wren_a,
    input wren_b,
    input[ADDR_BITS-1:0] write_address_a,
    input[ADDR_BITS-1:0] write_address_b,
    output[9:0] q_b
);



    altera_syncram  altera_syncram_component (
                .clock0 (clk),
                .address2_a ({ADDR_BITS{1'b0}}),
                .address2_b (read_address_b),
                .address_a (write_address_a),
                .address_b (write_address_b),
                .data_a (data_a),
                .data_b (data_b),
                .rden_a (1'b0), // Tie off read port A, not used!
                .rden_b (1'b1),
                .wren_a (wren_a),
                .wren_b (wren_b),
                .q_a (),
                .q_b (q_b),
                .aclr0 (1'b0),
                .aclr1 (1'b0),
                .addressstall_a (1'b0),
                .addressstall_b (1'b0),
                .byteena_a (1'b1),
                .byteena_b (1'b1),
                .clock1 (1'b1),
                .clocken0 (1'b1),
                .clocken1 (1'b1),
                .clocken2 (1'b1),
                .clocken3 (1'b1),
                .eccencbypass (1'b0),
                .eccencparity (8'b0),
                .eccstatus (),
                .sclr (1'b0));
    defparam
        altera_syncram_component.address_aclr_a  = "NONE",
        altera_syncram_component.address_aclr_b  = "NONE",
        altera_syncram_component.address_reg_b  = "CLOCK0",
        altera_syncram_component.clock_enable_input_a  = "BYPASS",
        altera_syncram_component.clock_enable_input_b  = "BYPASS",
        altera_syncram_component.clock_enable_output_a  = "BYPASS",
        altera_syncram_component.clock_enable_output_b  = "BYPASS",
        altera_syncram_component.indata_reg_b  = "CLOCK0",
        altera_syncram_component.intended_device_family  = "Stratix 10",
        altera_syncram_component.lpm_type  = "altera_syncram",
        altera_syncram_component.maximum_depth  = 2048,
        altera_syncram_component.numwords_a  = (1 << ADDR_BITS),
        altera_syncram_component.numwords_b  = (1 << ADDR_BITS),
        altera_syncram_component.operation_mode  = "QUAD_PORT",
        altera_syncram_component.outdata_aclr_a  = "NONE",
        altera_syncram_component.outdata_sclr_a  = "NONE",
        altera_syncram_component.outdata_aclr_b  = "NONE",
        altera_syncram_component.outdata_sclr_b  = "NONE",
        altera_syncram_component.outdata_reg_a  = "CLOCK0",
        altera_syncram_component.outdata_reg_b  = "CLOCK0",
        altera_syncram_component.power_up_uninitialized  = "FALSE",
        altera_syncram_component.ram_block_type  = "M20K",
        altera_syncram_component.enable_force_to_zero  = "TRUE",
        altera_syncram_component.read_during_write_mode_port_a  = "DONT_CARE",
        altera_syncram_component.read_during_write_mode_port_b  = "DONT_CARE",
        altera_syncram_component.read_during_write_mode_mixed_ports  = "NEW_A_OLD_B",
        altera_syncram_component.widthad_a  = ADDR_BITS,
        altera_syncram_component.widthad_b  = ADDR_BITS,
        altera_syncram_component.widthad2_a  = ADDR_BITS,
        altera_syncram_component.widthad2_b  = ADDR_BITS,
        altera_syncram_component.width_a  = 10,
        altera_syncram_component.width_b  = 10,
        altera_syncram_component.width_byteena_a  = 1,
        altera_syncram_component.width_byteena_b  = 1;



endmodule
