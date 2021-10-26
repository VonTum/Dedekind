function automatic hasVar;
    input integer index;
    input integer var;
    
    begin
        hasVar = (index & (1 << var)) != 0;
    end
endfunction

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

`define VAR_SWAP_INLINE(VAR_A, VAR_B, SOURCE_WIRES, DESTINATION_WIRES) generate for(genvar outI = 0; outI < 128; outI = outI + 1) assign DESTINATION_WIRES[outI] = SOURCE_WIRES[getInIndex(outI, VAR_A, VAR_B)]; endgenerate
