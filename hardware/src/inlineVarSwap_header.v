function automatic hasVar;
    input integer binaryNum;
    input integer varIdx;
    
    begin
        hasVar = (binaryNum & (1 << varIdx)) != 0;
    end
endfunction

// Returns the index from where the wire connecting to the out index should come if VAR_A and VAR_B are swapped. 
function automatic integer getInIndex;
    input integer outIndex;
    input integer VAR_A;
    input integer VAR_B;
    begin
        if(hasVar(outIndex, VAR_A) & !hasVar(outIndex, VAR_B))
            getInIndex = outIndex - (1 << VAR_A) + (1 << VAR_B);
        else if(!hasVar(outIndex, VAR_A) & hasVar(outIndex, VAR_B)) 
            getInIndex = outIndex + (1 << VAR_A) - (1 << VAR_B);
        else
            getInIndex = outIndex;
    end
endfunction

`define VAR_SWAP_INLINE_WITHIN_GENERATE(VAR_A, VAR_B, SOURCE_WIRES, DESTINATION_WIRES) for(genvar outI = 0; outI < 128; outI = outI + 1) begin assign DESTINATION_WIRES[outI] = SOURCE_WIRES[getInIndex(outI, VAR_A, VAR_B)]; end
`define VAR_SWAP_INLINE(VAR_A, VAR_B, SOURCE_WIRES, DESTINATION_WIRES) generate `VAR_SWAP_INLINE_WITHIN_GENERATE(VAR_A, VAR_B, SOURCE_WIRES, DESTINATION_WIRES) endgenerate
`define VAR_SWAP_INLINE_EXTRA_WIRES_WITHIN_GENERATE(VAR_A, VAR_B, SOURCE_WIRES, DESTINATION_WIRES, DESTINATION_EXTRA_WIRES_NAME) wire[127:0] DESTINATION_EXTRA_WIRES_NAME; `VAR_SWAP_INLINE_WITHIN_GENERATE(VAR_A, VAR_B, SOURCE_WIRES, DESTINATION_EXTRA_WIRES_NAME); assign DESTINATION_WIRES = DESTINATION_EXTRA_WIRES_NAME;
`define VAR_SWAP_INLINE_EXTRA_WIRES(VAR_A, VAR_B, SOURCE_WIRES, DESTINATION_WIRES, DESTINATION_EXTRA_WIRES_NAME) generate `VAR_SWAP_INLINE_EXTRA_WIRES_WITHIN_GENERATE(VAR_A, VAR_B, SOURCE_WIRES, DESTINATION_WIRES, SOURCE_EXTRA_WIRES, DESTINATION_EXTRA_WIRES_NAME) endgenerate
