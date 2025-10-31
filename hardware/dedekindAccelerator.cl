// Implemented in HDL
ulong2 pcoeffProcessor(ulong2 mbfA, ulong2 mbfB, bool startNewTop);

uint bitReverse4(uint v) {
  return ((v & 0x1) << 3) | ((v & 0x2) << 1) | ((v & 0x4) >> 1) | ((v & 0x8) >> 3);
}
// Very primitive shuffle for balancing out the load across a job. This divides the job into 16 chunks and intersperses them
uint shuffleIndex(uint idx, uint workGroupSize) {
  uint chunkOffset = workGroupSize >> 4;
  uint mod = idx & 0xF;
  uint div = idx >> 4;
  return chunkOffset * bitReverse4(mod) + div;
}
// Shuffles blocks of 16 elements instead of individual elements. For better memory utilization
uint shuffleClusters(uint idx, uint workGroupSize) {
  uint block = idx >> 4;
  uint idxInBlock = idx & 0xF;
  uint numberOfBlocks = workGroupSize >> 4;
  return (shuffleIndex(block, numberOfBlocks) << 4) | idxInBlock;
}

// Attribute to fix report warning that it is missing
__attribute__((uses_global_work_offset(0)))
kernel void dedekindAccelerator(
      global const ulong2 * restrict mbfLUTA,
      global const ulong2 * restrict mbfLUTB,
      global const uint2 * restrict jobsIn,
      global ulong2 * restrict processedPCoeffsOut,
      uint workGroupSize
) {
  for (uint i = 0; i < workGroupSize / 2; i++) { // 2 mbfs per iteration
    uint shuffledI = shuffleClusters(i, workGroupSize / 2);
    uint2 curJob = __fpga_reg(__burst_coalesced_load(&jobsIn[shuffledI]));
    bool startNewTop = (curJob.x & 0x80000000u) != 0x00000000u; // First bit describes if the selected mbf is a top marking the start of a new bot series
    // Have to cut off the first bit, so  we don't have incorrect addresses
    // Load the mbf from memory. Pipelined, because accessed mbfs are nonconsecutive. No cache either because it could never be large enough to be functional
    ulong2 mbfA = __burst_coalesced_load(&mbfLUTA[curJob.x & 0x7FFFFFFFu]);
    ulong2 mbfB = __burst_coalesced_load(&mbfLUTB[curJob.y & 0x7FFFFFFFu]);
    
    ulong2 result = pcoeffProcessor(__fpga_reg(mbfA), __fpga_reg(mbfB), __fpga_reg(startNewTop));
    processedPCoeffsOut[shuffledI] = __fpga_reg(result);
  }
}

