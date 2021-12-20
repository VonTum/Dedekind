`timescale 1ns / 1ps

module varSwap #(
    parameter FIRST_VAR = 5,
    parameter SECOND_VAR = 6
) (
    input [127:0] dataIn,
    output [127:0] dataOut
);

`include "inlineVarSwap_header.v"

`VAR_SWAP_INLINE(FIRST_VAR, SECOND_VAR, dataIn, dataOut)

endmodule
