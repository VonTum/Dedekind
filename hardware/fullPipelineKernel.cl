// Implemented in HDL
ulong2 fullPipeline(ulong2 mbfUppers, ulong2 mbfLowers, bool startNewTop);

// Attribute to fix report warning that it is missing
__attribute__((uses_global_work_offset(0)))
kernel void fullPipelineKernel(global const ulong2 * restrict mbfLUT, global const uint2 * restrict jobsIn, global ulong2 * restrict processedPCoeffsOut, uint workGroupSize) {
  for (uint i = 0; i < workGroupSize / 2; i++) { // 2 mbfs per iteration
    uint2 curJob = __pipelined_load(&jobsIn[i]);
    bool startNewTop = (curJob.x & 0x80000000u) != 0x00000000u; // First bit describes if the selected mbf is a top marking the start of a new bot series
    // Have to cut off the first bit, so  we don't have incorrect addresses
    // Load the mbf from memory. Pipelined, because accessed mbfs are nonconsecutive. No cache either because it could never be large enough to be functional
    ulong2 mbfA = __pipelined_load(&mbfLUT[curJob.x & 0x7FFFFFFFu]);
    ulong2 mbfB = __pipelined_load(&mbfLUT[curJob.y & 0x7FFFFFFFu]);
    ulong2 uppers;
    ulong2 lowers;
    uppers.x = mbfA.x;
    uppers.y = mbfB.x;
    lowers.x = mbfA.y;
    lowers.y = mbfB.y;
    ulong2 result = fullPipeline(uppers, lowers, startNewTop);
    __pipelined_store(&processedPCoeffsOut[i], result);
  }
}

