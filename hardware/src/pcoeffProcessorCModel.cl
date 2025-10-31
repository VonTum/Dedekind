ulong2 pcoeffProcessor(ulong2 mbfA, ulong2 mbfB, bool startNewTop) {
  ulong2 result;
  result.x = mbfA.x + mbfB.x;
  result.y = mbfA.y + mbfB.y;
  return result;
}
