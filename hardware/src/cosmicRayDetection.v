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


module smallECCEncoder(
    input[7:0] data,
    output[1:0] eccValue
);

assign eccValue = data[1:0] ^ data[3:2] ^ data[5:4] ^ data[7:6];

endmodule

