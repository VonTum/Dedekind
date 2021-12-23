`timescale 1ns / 1ps

module resetNormalizer #(parameter RESET_CYCLES = 15) (
    input clk,
    input resetn,
    output rst
);

reg[5:0] cyclesSinceReset = 0;
reg rstReg = 1'b1;

// to ease fitting
hyperpipe #(.CYCLES(5), .WIDTH(1)) rstPipe(clk, rstReg, rst);

always @(posedge clk) begin
    if(!resetn) begin
        cyclesSinceReset <= 0;
        rstReg <= 1'b1;
    end else begin
        if(cyclesSinceReset > RESET_CYCLES) begin
            rstReg <= 1'b0;
        end
        cyclesSinceReset <= cyclesSinceReset + 1;
    end
end


endmodule
