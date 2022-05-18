`timescale 1 ps / 1 ps

module cosmicRayDetection (
    input clk,
    input rst,
    
    input eccStatus,
    
    output reg eccErrorOccured // Errors in M20Ks
);

// ECC Detection
always @(posedge clk) begin
    if(rst) begin
        eccErrorOccured <= 0;
    end else begin
        if(eccStatus) eccErrorOccured <= 1;
    end
end

endmodule
