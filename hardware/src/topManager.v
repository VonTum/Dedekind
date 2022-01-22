`timescale 1ns / 1ps

module topManager(
    input clk,
    input rst,
    
    input[127:0] topIn,
    input topInValid,
    
    output[127:0] topOut,
    
    // Input queue side
    input botWentIn,
    output stallInput,
    
    // Output queue side
    input botWentOut,
    input iready,
    output ovalidOverride
);


reg[31:0] inputBotCount;
reg[31:0] outputBotCount;
reg pipelineIsEmpty; always @(posedge clk) pipelineIsEmpty <= (inputBotCount == outputBotCount);

reg[127:0] topInWaiting;
reg isTopInWaiting;
assign stallInput = isTopInWaiting;

reg readyToInstallNewTop; always @(posedge clk) readyToInstallNewTop <= pipelineIsEmpty & isTopInWaiting;
reg readyToLoadNewTop; always @(posedge clk) readyToLoadNewTop <= readyToInstallNewTop & iready;
reg readyToLoadNewTopD; always @(posedge clk) readyToLoadNewTopD <= readyToLoadNewTop;
reg loadNewTop; always @(posedge clk) loadNewTop <= readyToLoadNewTop && !readyToLoadNewTopD; // Create top loading pulse
assign ovalidOverride = loadNewTop;

// Makes all paths starting at topReg false paths. This is possible because top is a de-facto global constant. 
// Data delay is allowed to go up to `OUTPUT_INDEX_OFFSET = 1024 cycles, which I assume will be plenty
(* altera_attribute = "-name CUT ON -to *" *) reg[127:0] topReg;
assign topOut = topReg;

always @(posedge clk) begin
    if(rst || loadNewTop) begin
        inputBotCount <= 0;
        outputBotCount <= 0;
    end else begin
        if(botWentIn) inputBotCount <= inputBotCount + 1;
        if(botWentOut) outputBotCount <= outputBotCount + 1;
    end
end


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
