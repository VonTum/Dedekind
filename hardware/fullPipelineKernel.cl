unsigned long fullPipeline(unsigned long botUpper, unsigned long botLower, bool startNewTop);

// Using HDL library components
kernel void fullPipelineKernel(global unsigned long * restrict botUpperIn, global unsigned long * restrict botLowerIn, global unsigned long * restrict out, int N) {
  bool startNewTop = true;
  for (int k =0; k < N; k++) {
    unsigned long botUpper = botUpperIn[k];
    unsigned long botLower = botLowerIn[k];
    out[k] = fullPipeline(botUpper, botLower, startNewTop);
    startNewTop = false;
  }
}
