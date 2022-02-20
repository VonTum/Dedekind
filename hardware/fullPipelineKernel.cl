unsigned long fullPipeline(unsigned long botUpper, unsigned long botLower, bool startNewTop);

// Using HDL library components
kernel void fullPipelineKernel(global const unsigned long * restrict mbfLUTUpper, global const unsigned long * restrict mbfLUTLower, global const unsigned int * restrict jobsIn, global unsigned long * restrict processedPCoeffsOut, int workGroupSize) {
  for (int i = 0; i < workGroupSize; i++) {
    unsigned int curJob = jobsIn[i];
    bool startNewTop = curJob >> 31; // First bit describes if the selected mbf is a top marking the start of a new bot series
    unsigned int mbfID = curJob & 0x7FFFFFFF; // Have to cut off the first bit, so  we don't have incorrect addresses
    unsigned long mbfUpper = mbfLUTUpper[mbfID];
    unsigned long mbfLower = mbfLUTLower[mbfID];
    processedPCoeffsOut[i] = fullPipeline(mbfUpper, mbfLower, startNewTop);
  }
}

