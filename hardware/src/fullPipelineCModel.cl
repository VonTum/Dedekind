ulong2 fullPipeline(ulong2 mbfUppers, ulong2 mbfLowers, bool startNewTop) {
  ulong2 result;
  result.x = mbfUppers.x + mbfLowers.x;
  result.y = mbfUppers.y + mbfLowers.y;
  return result;
}
