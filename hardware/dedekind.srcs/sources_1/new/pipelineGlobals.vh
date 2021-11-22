// 
`define ADDR_WIDTH 13
// at the time that index i is input, the result of i + (1 << ADDR_WIDTH) - OUTPUT_INDEX_OFFSET is output
`define OUTPUT_INDEX_OFFSET 1024
 // number of clock cycles of read latency. 
`define OUTPUT_READ_LATENCY 10
