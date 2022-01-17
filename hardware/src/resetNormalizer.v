`timescale 1ns / 1ps

module resetNormalizer #(parameter RESET_CYCLES = 15, parameter INITIALIZE_CYCLES = 25) (
    input clk,
    input resetn,
    output reg rst,
    output reg isInitialized
);

reg[5:0] cyclesSinceReset = 0;

always @(posedge clk) begin
    if(!resetn) begin
        cyclesSinceReset <= 0;
        rst <= 1'b1;
        isInitialized <= 1'b0;
    end else begin
        if(cyclesSinceReset > RESET_CYCLES) begin
            rst <= 1'b0;
        end
        if(cyclesSinceReset > INITIALIZE_CYCLES) begin
            isInitialized <= 1'b1;
        end
        cyclesSinceReset <= cyclesSinceReset + 1;
    end
end


endmodule
