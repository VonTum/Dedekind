`timescale 1ns / 1ps

(* altera_attribute = "-name SYNCHRONIZER_IDENTIFICATION AUTO" *)
module synchronizer #(parameter WIDTH = 1, parameter SYNC_STAGES = 3) (
    input inClk,
    input[WIDTH-1:0] dataIn,
    
    input outClk,
    output[WIDTH-1:0] dataOut
);

reg[WIDTH-1:0] inReg;
always @(posedge inClk) inReg <= dataIn;

reg[WIDTH-1:0] syncRegs[SYNC_STAGES-2:0];
always @(posedge outClk) syncRegs[SYNC_STAGES-2] <= inReg;

generate
for(genvar i = 0; i < SYNC_STAGES-2; i=i+1) begin always @(posedge outClk) syncRegs[i] <= syncRegs[i+1]; end
endgenerate

assign dataOut = syncRegs[0];

endmodule


module resetSynchronizer #(parameter SYNC_STAGES = 3) (
    input clk,
    input aresetnIn, // asynchronous input reset
    output resetnOut
);

reg rstLine[SYNC_STAGES-1:0];

generate
genvar i;
for(i = 0; i < SYNC_STAGES - 1; i = i + 1) begin
    always @(posedge clk or negedge aresetnIn) begin
        if(!aresetnIn) begin
            rstLine[i] <= 1'b0;
        end else begin
            rstLine[i] <= rstLine[i+1];
        end
    end
end
always @(posedge clk or negedge aresetnIn) begin
    if(!aresetnIn) begin
        rstLine[SYNC_STAGES - 1] <= 1'b0;
    end else begin
        rstLine[SYNC_STAGES-1] <= 1'b1;
    end
end
endgenerate

assign resetnOut = rstLine[0];

endmodule



(* altera_attribute = "-name AUTO_SHIFT_REGISTER_RECOGNITION OFF; -name SYNCHRONIZER_IDENTIFICATION AUTO" *) 
module grayCodePipe #(parameter WIDTH = 5) (
    input clkA,
    input[WIDTH-1:0] dataA,
    
    input clkB,
    output[WIDTH-1:0] dataB
);

reg[WIDTH-1:0] grayA;
always @(posedge clkA) begin
    grayA <= dataA ^ (dataA >> 1);
end

reg[WIDTH-1:0] grayB1; always @(posedge clkB) grayB1 <= grayA;
reg[WIDTH-1:0] grayB2; always @(posedge clkB) grayB2 <= grayB1;
generate
    assign dataB[WIDTH-1] = grayB2[WIDTH-1];
    genvar i;
    for(i = WIDTH-2; i >= 0; i = i - 1) begin
        assign dataB[i] = grayB2[i] ^ dataB[i+1];
    end
endgenerate

endmodule

// Used for transferring possibly short pulses from a fast clock domain to a slower clock domain
// May elongate pulse at output
(* altera_attribute = "-name SYNCHRONIZER_IDENTIFICATION AUTO" *)
module fastToSlowPulseSynchronizer #(parameter CLOCK_RATIO = 2, parameter SYNC_STAGES = 2) (
    input fastClk,
    input dataIn,
    
    input slowClk,
    output dataOut
);

reg[CLOCK_RATIO-1:0] delayRegs;
reg slowedPulse;
always @(posedge fastClk) begin
    slowedPulse <= |delayRegs || dataIn;
    delayRegs[CLOCK_RATIO-1:1] <= delayRegs[CLOCK_RATIO-2:0];
    delayRegs[0] <= dataIn;
end

reg[SYNC_STAGES-1:0] syncRegs;
always @(posedge slowClk) begin
    syncRegs[SYNC_STAGES-1:1] <= syncRegs[SYNC_STAGES-2:0];
    syncRegs[0] <= slowedPulse;
end

assign dataOut = syncRegs[SYNC_STAGES-1];

endmodule

