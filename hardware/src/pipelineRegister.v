`timescale 1ns / 1ps

module pipelineRegister #(parameter WIDTH = 8) (
    input clk,
    input rst,
    
    // input side
    input write,
    input[WIDTH-1:0] dataIn,
    
    // output side
    input grab,
    output reg hasData,// Shared status
    output reg[WIDTH-1:0] data
);

always @ (posedge clk) begin
    if(rst) begin
        hasData <= 0;
    end else begin
        if(write) begin
            hasData <= 1;
        end else if(grab) begin
            hasData <= 0;
        end
    end
    
    if(write) begin
        data <= dataIn;
    end
end

endmodule
