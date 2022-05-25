`timescale 1ns / 1ps

`define ADDR_WIDTH 11
`define ADDR_DEPTH (1 << `ADDR_WIDTH)

`define INPUT_FIFO_DEPTH_LOG2 5

module streamingCountConnectedCore #(parameter EXTRA_DATA_WIDTH = 1) (
    input clk,
    input clk2x,
    input rst,
    input longRST,
    output isActive2x, // Instrumentation wire for profiling
    
    // Input side
    input isBotValid,
    input[127:0] graphIn,
    input[EXTRA_DATA_WIDTH-1:0] extraDataIn,
    input freezeCore,
    output almostFull,
    
    // Output side
    output reg resultValid,
    output[5:0] connectCount,
    output reg[EXTRA_DATA_WIDTH-1:0] extraDataOut,
    output reg eccStatus
);

reg[31:0] validIn_COUNT;
reg[31:0] validOut_COUNT;

reg[31:0] batchDoneIn_COUNT;
reg[31:0] batchDoneOut_COUNT;

always @(posedge clk) begin
    if(rst) begin
        validIn_COUNT <= 0;
        validOut_COUNT <= 0;
        batchDoneIn_COUNT <= 0;
        batchDoneOut_COUNT <= 0;
    end else begin
        if(isBotValid === 1'b1) validIn_COUNT <= validIn_COUNT + 1;
        if(resultValid === 1'b1) validOut_COUNT <= validOut_COUNT + 1;
        if(extraDataIn === 1'b1) batchDoneIn_COUNT <= batchDoneIn_COUNT + 1;
        if(extraDataOut === 1'b1) batchDoneOut_COUNT <= batchDoneOut_COUNT + 1;
    end
end


(* dont_merge *) reg pipelineRST; always @(posedge clk) pipelineRST <= rst;

wire collectorECC; reg collectorECC_D; always @(posedge clk) collectorECC_D <= collectorECC;
wire isBotValidECC; reg isBotValidECC_D; always @(posedge clk) isBotValidECC_D <= isBotValidECC;
wire pipelineECC;

reg resultValid_D; always @(posedge clk) resultValid_D <= resultValid;
always @(posedge clk) eccStatus <= (!longRST && ((collectorECC_D && resultValid_D) || isBotValidECC_D)) || pipelineECC; // Collector ECC only matters if bot data should have actually been read. This also handles bad ECC from uninitialized memory

reg[`ADDR_WIDTH-1:0] curBotIndex = 0;
always @(posedge clk) begin
`ifdef ALTERA_RESERVED_QIS
    if(!freezeCore) curBotIndex <= curBotIndex + 1;
`elsif SYNTHESIS
    if(!freezeCore) curBotIndex <= curBotIndex + !freezeCore;
`else
    /* 
    FreezeCore being uninitialized at the start makes curBotIndex undefined for the whole duration of the run
    It doesn't matter in practice what the value of curBotIndex is, as long as each value is repeated every 512 cycles. 
    The synthesis guards allow this non-synthesizeable fix to exist
    */
    if(freezeCore !== 1'bX && freezeCore !== 1'bZ) begin
        if(!freezeCore) curBotIndex <= curBotIndex + !freezeCore;
    end
`endif
end

wire inputFifoValid2x;
wire requestGraph2x;

wire[127:0] graphToComputeModule2x;
wire[`ADDR_WIDTH-1:0] addrToComputeModule2x;

wire pipelineRST2x;
synchronizer pipelineRSTSynchronizer(clk, pipelineRST, clk2x, pipelineRST2x);
(* dont_merge *) reg inputFIFORST2x; always @(posedge clk2x) inputFIFORST2x <= pipelineRST2x;
(* dont_merge *) reg cccRST2x; always @(posedge clk2x) cccRST2x <= pipelineRST2x;

// request Pipe has 1 cycle, FIFO has 5 cycles read latency, dataOut pipe has 0 cycles
`define FIFO_READ_LATENCY (1+5+0)
(* dont_merge *) reg requestGraph2xD; always @(posedge clk2x) requestGraph2xD <= requestGraph2x;
wire inputFifoECC2x;
FastDualClockFIFO_M20K #(.WIDTH(128+`ADDR_WIDTH), .DEPTH_LOG2(6), .ALMOST_FULL_MARGIN(10)) inputFIFO (// 7 cycles turnaround. 3 margin cycles
    // input side
    .wrclk(clk),
    .wrrst(pipelineRST),
    .writeEnable(isBotValid),
    .dataIn({graphIn, curBotIndex}),
    .almostFull(almostFull),
    
    // output side
    .rdclk(clk2x),
    .rdrst(inputFIFORST2x),
    .readRequest(requestGraph2xD),
    .dataOut({graphToComputeModule2x, addrToComputeModule2x}), // Forced to 0 if not inputFifoValid2x
    .dataOutValid(inputFifoValid2x),
    
    .eccStatus(inputFifoECC2x)
);

wire writeToCollector2x;
wire[5:0] connectCountToCollector2x;
wire[`ADDR_WIDTH-1:0] addrToCollector2x;

wire pipelineECC2x;
fastToSlowPulseSynchronizer eccSync(clk2x, pipelineECC2x || inputFifoECC2x, clk, pipelineECC);

pipelinedCountConnectedCore #(.EXTRA_DATA_WIDTH(`ADDR_WIDTH), .DATA_IN_LATENCY(`FIFO_READ_LATENCY)) countConnectedCore (
    .clk(clk2x),
    .rst(cccRST2x),
    .isActive(isActive2x),
    
    // input side
    .request(requestGraph2x),
    .graphIn(graphToComputeModule2x),
    .graphInValid(inputFifoValid2x),
    .extraDataIn(addrToComputeModule2x),
    
    // output side
    .done(writeToCollector2x),
    .connectCountOut(connectCountToCollector2x),
    .extraDataOut(addrToCollector2x),
    .eccStatus(pipelineECC2x)
);

wire[1:0] connectCountToCollectorECC2x;
smallECCEncoder encCC({connectCountToCollector2x, 2'b00}, connectCountToCollectorECC2x);

wire[1:0] connectCountECC;
wire[1:0] connectCountECCCheck;
smallECCEncoder decCC({connectCount, 2'b00}, connectCountECCCheck);

assign collectorECC = connectCountECC != connectCountECCCheck;
wire[1:0] paddingECC;
DUAL_CLOCK_MEMORY_M20K_NO_ECC #(.WIDTH(6+2+2), .DEPTH_LOG2(`ADDR_WIDTH)) collectorMemory (
    // Write Side
    .wrclk(clk2x),
    .writeEnable(writeToCollector2x),
    .writeAddr(addrToCollector2x),
    .dataIn({connectCountToCollector2x, 2'b00, connectCountToCollectorECC2x}),
    
    // Read Side
    .rdclk(clk),
    .readEnable(1'b1),
    .readAddr(curBotIndex),
    .dataOut({connectCount, paddingECC, connectCountECC})
);

reg[`ADDR_WIDTH-1:0] curBotIndex_D; always @(posedge clk) if(!freezeCore) curBotIndex_D <= curBotIndex;
wire resultValid_Pre; always @(posedge clk) resultValid <= resultValid_Pre;
wire[EXTRA_DATA_WIDTH-1:0] extraDataOut_Pre; always @(posedge clk) extraDataOut <= extraDataOut_Pre;

wire[1:0] isValidMemECC;
smallECCEncoder encIsValidECC({isBotValid, extraDataIn}, isValidMemECC);


wire[1:0] isValidMemECCOut;
wire[1:0] isValidMemECCOutCheck;
smallECCEncoder decIsValidECC({resultValid_Pre, extraDataOut_Pre}, isValidMemECCOutCheck);

assign isBotValidECC = isValidMemECCOut != isValidMemECCOutCheck;

MEMORY_M20K_NO_ECC #(.WIDTH(1+EXTRA_DATA_WIDTH+2), .DEPTH_LOG2(`ADDR_WIDTH)) isValidMemory (
    .clk(clk),
    
    // Write Side
    .writeEnable(!freezeCore),
    .writeAddr(curBotIndex_D),
    .dataIn({isBotValid, extraDataIn, isValidMemECC}),
    
    // Read Side
    .readEnable(!freezeCore), // Use the force-to-zero to zero out these outputs when frozen
    .readAddr(curBotIndex),
    .dataOut({resultValid_Pre, extraDataOut_Pre, isValidMemECCOut})
);

endmodule
