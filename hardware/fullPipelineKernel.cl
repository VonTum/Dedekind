ulong fullPipeline(ulong botUpper, ulong botLower, bool startNewTop);

// Using HDL library components
kernel void fullPipelineKernel(global const ulong2 * restrict mbfLUT, global const uint * restrict jobsIn, global ulong * restrict processedPCoeffsOut, uint workGroupSize) {
  for (uint i = 0; i < workGroupSize; i++) {
    uint curJob = __pipelined_load(jobsIn + i);
    bool startNewTop = (curJob & 0x80000000u) != 0x00000000u; // First bit describes if the selected mbf is a top marking the start of a new bot series
    uint mbfID = curJob & 0x7FFFFFFFu; // Have to cut off the first bit, so  we don't have incorrect addresses
    ulong2 mbf = __pipelined_load(mbfLUT + mbfID); // Load the mbf from memory. Pipelined, because accessed mbfs are nonconsecutive. No cache either because it could never be large enough to be functional
    ulong upper = mbf.x;
    ulong lower = mbf.y;
    ulong result = fullPipeline(upper, lower, startNewTop);
    __pipelined_store(processedPCoeffsOut + i, result);
  }
}

