`timescale 1ns / 1ps

module topTransmissionTest();

reg clk = 0;
initial forever #7 clk = !clk;
reg clk2x = 0;
initial forever #3 clk2x = !clk2x;

reg[127:0] topToTransmit = 128'b00000000000100110000001101111111000100110011011101010111011111110001001100110011001100110111111101010111011101111111111111111111;

reg transmitTop = 0;
initial begin
    #20
    transmitTop = 1;
    #6
    transmitTop = 0;
end

wire[1:0] topChannel;

wire doneTransmitting;
topTransmitter transmitter (
    clk,
    topToTransmit,
    transmitTop,
    doneTransmitting,
    
    topChannel
);

wire[1:0] topChannel2x;
synchronizer #(.WIDTH(2)) topChannelSync(clk, topChannel, clk2x, topChannel2x);

wire[127:0] receivedTop;

topReceiver receiver (
    clk2x,
    topChannel2x,
    
    receivedTop
);

endmodule
