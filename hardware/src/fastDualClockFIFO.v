`timescale 1ns / 1ps

module fastDualClockFIFO #(parameter WIDTH = 40) (
    input inputClk,
    input outputClk,
    
    input[WIDTH-1:0] dataIn,
    input dataInValid,
    output full,
    
    output[WIDTH-1:0] dataOut,
    output empty
);
endmodule
