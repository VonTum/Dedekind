`timescale 1 ps / 1 ps

module cosmicRayDetection (
    input clk,
    input rst,
    
    input eccStatus,
    
    output reg eccErrorOccured, // Errors in M20Ks
    output reg seuOccured, // Errors in CRAM
    output reg seuSysError,
    output reg[63:0] seuData
);

// ECC Detection
always @(posedge clk) begin
    if(rst) begin
        eccErrorOccured <= 0;
    end else begin
        if(eccStatus) eccErrorOccured <= 1;
    end
end

wire seuValid;
wire seuSysErrorWire;
wire[63:0] seuDataWire;

/*`ifdef ALTERA_RESERVED_QIS
seuDetection_stratix10_asd_1910_6oetfnq #(
    .ERROR_DEPTH       (4),
    .REGION_MASK_WIDTH (1),
    .START_ADDR        (0)
) stratix10_asd_0 (
    .clk                   (clk),             //   input,   width = 1,             clk.clk
    .reset                 (rst),             //   input,   width = 1,           reset.reset
    .avst_seu_source_data  (seuDataWire),     //  output,  width = 64, avst_seu_source.data
    .avst_seu_source_valid (seuValid),        //  output,   width = 1,                .valid
    .avst_seu_source_ready (!rst),            //   input,   width = 1,                .ready
    .sys_error_sys_error   (seuSysErrorWire)  //  output,   width = 1,       sys_error.sys_error
);
`else*/
assign seuValid = 0;
assign seuSysErrorWire = 0;
assign seuDataWire = 0;
//`endif

always @(posedge clk) begin
    if(rst) begin
        seuOccured <= 0;
        seuSysError <= 0;
    end else begin
        if(seuValid && !seuOccured) begin
            seuOccured <= 1;
            seuData <= seuDataWire;
            seuSysError <= seuSysErrorWire;
        end
    end
end

endmodule
