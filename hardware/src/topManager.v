`timescale 1ns / 1ps

module topManager(
    input clk,
    input rst,
    
    input[127:0] topIn,
    input topInValid,
    
    output[127:0] topOut,
    
    output stallInput,
    input pipelineIsEmpty
);


reg[127:0] topInWaiting;
reg isTopInWaiting;
assign stallInput = isTopInWaiting;

// A bit of delay after a new top to make sure that pipelineIsEmpty reaches stability
reg[2:0] cylcesSinceTopStart;
wire isReadyForTopStart = cylcesSinceTopStart == 7;
always @(posedge clk) begin
    if(rst || topInValid) begin
        cylcesSinceTopStart <= 0;
    end else begin
        if(!isReadyForTopStart) cylcesSinceTopStart <= cylcesSinceTopStart + 1;
    end
end

reg readyToInstallNewTop; always @(posedge clk) readyToInstallNewTop <= pipelineIsEmpty && isTopInWaiting && isReadyForTopStart;
reg readyToLoadNewTop; always @(posedge clk) readyToLoadNewTop <= readyToInstallNewTop;
reg readyToLoadNewTopD; always @(posedge clk) readyToLoadNewTopD <= readyToLoadNewTop;
reg loadNewTop; always @(posedge clk) loadNewTop <= readyToLoadNewTop && !readyToLoadNewTopD; // Create top loading pulse

// Makes all paths starting at topReg false paths. This is possible because top is a de-facto global constant. 
// Data delay is allowed to go up to `OUTPUT_INDEX_OFFSET = 1024 cycles, which I assume will be plenty
/*(* altera_attribute = "-name CUT ON -to *" *) */reg[127:0] topReg;
assign topOut = topReg;

always @(posedge clk) begin
    if(rst) begin
        isTopInWaiting <= 1'b0;
    end else begin
        if(topInValid) begin
            topInWaiting <= topIn;
            isTopInWaiting <= 1'b1;
        end else if(loadNewTop) begin
            topReg <= topInWaiting;
            isTopInWaiting <= 1'b0;
        end
    end
end


endmodule

