`timescale 1ns / 1ps

module topManager(
    input clk,
    input rst,
    
    input[127:0] topIn,
    input topInValid,
    output stallInput,
    input pipelineIsEmpty,
    
    output[1:0] topChannel
);


reg[127:0] topInWaiting;
reg isTopInWaiting;
assign stallInput = isTopInWaiting;

// A bit of delay after a new top to make sure that pipelineIsEmpty reaches stability
reg[3:0] cylcesSinceTopStart;
wire newTopWarmingUp = cylcesSinceTopStart != 15;
always @(posedge clk) begin
    if(rst || topInValid) begin
        cylcesSinceTopStart <= 0;
    end else begin
        if(newTopWarmingUp) cylcesSinceTopStart <= cylcesSinceTopStart + 1;
    end
end

reg transmitNewTop; always @(posedge clk) transmitNewTop <= isTopInWaiting && (!pipelineIsEmpty || newTopWarmingUp);

wire doneTransmitting;
topTransmitter transmitter(
    .clk(clk),
    .top(topInWaiting),
    .transmitTop(transmitNewTop),
    .doneTransmitting(doneTransmitting),
    
    .topChannel(topChannel)
);

always @(posedge clk) begin
    if(rst) begin
        isTopInWaiting <= 1'b0;
    end else begin
        if(topInValid) begin
            topInWaiting <= topIn;
            isTopInWaiting <= 1'b1;
        end else if(doneTransmitting && !newTopWarmingUp) begin
            isTopInWaiting <= 1'b0;
        end
    end
end

endmodule


module topTransmitter(
    input clk,
    input[127:0] top,
    input transmitTop,
    output doneTransmitting,
    
    output[1:0] topChannel
);

reg[8:0] curIndex = 9'b100000000;
wire doneTransmittingWire = curIndex[8];
hyperpipe #(.CYCLES(12)) doneTransmittingPipe(clk, doneTransmittingWire, doneTransmitting); // Lots of delay to allow the new top to be properly installed throughout the design before continuing

reg controlState = 0;

wire[6:0] bitSelector = curIndex[7:1];

reg topBitstream; always @(posedge clk) topBitstream <= top[bitSelector];

assign topChannel = {topBitstream, controlState};

always @(posedge clk) begin
    if(transmitTop) begin
        curIndex <= 0;
    end else if(!doneTransmittingWire) begin
        curIndex <= curIndex + 1;
    end
    
    if(curIndex[0]) begin
        controlState <= !controlState;
    end
end

endmodule

module topReceiver(
    input clk,
    input[1:0] topChannel,
    
    output reg[127:0] top
);

wire data;
wire control;
assign {data, control} = topChannel;

reg prevControl; always @(posedge clk) prevControl <= control;

reg dataD; always @(posedge clk) dataD <= data;

// A bit is read on each transition of transmit. This is for proper communication across clock domains
reg transmitD; always @(posedge clk) transmitD <= control ^ prevControl;

reg transmitDD; always @(posedge clk) transmitDD <= transmitD;
reg dataDD; always @(posedge clk) dataDD <= dataD;

always @(posedge clk) begin
    if(transmitDD) begin
        top <= {dataDD, top[127:1]};
    end
end

endmodule
