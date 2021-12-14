// Address width of the pipeline, supported values 12 (4096 addresses) and 13 (8192 addresses)
`define ADDR_WIDTH 13
// depth of the address buffer
`define ADDR_DEPTH (1 << `ADDR_WIDTH)
// number of index offset bits. These represent the size of the result blocks used in the collection module. 
`define OUTPUT_INDEX_OFFSET_BITS 10
// at the time that index i is input, the result of i + `ADDR_DEPTH - `OUTPUT_INDEX_OFFSET is output
`define OUTPUT_INDEX_OFFSET (1 << `OUTPUT_INDEX_OFFSET_BITS)
 // number of clock cycles of read latency. 
`define OUTPUT_READ_LATENCY 10
