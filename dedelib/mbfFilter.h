

template<unsigned int Variables>
void testFilterTreePerformance();

extern template void testFilterTreePerformance<1>();
extern template void testFilterTreePerformance<2>();
extern template void testFilterTreePerformance<3>();
extern template void testFilterTreePerformance<4>();
extern template void testFilterTreePerformance<5>();
extern template void testFilterTreePerformance<6>();
extern template void testFilterTreePerformance<7>();
extern template void testFilterTreePerformance<8>();
extern template void testFilterTreePerformance<9>();

template<unsigned int Variables>
void testTreeLessFilterTreePerformance();
extern template void testTreeLessFilterTreePerformance<1>();
extern template void testTreeLessFilterTreePerformance<2>();
extern template void testTreeLessFilterTreePerformance<3>();
extern template void testTreeLessFilterTreePerformance<4>();
extern template void testTreeLessFilterTreePerformance<5>();
extern template void testTreeLessFilterTreePerformance<6>();
extern template void testTreeLessFilterTreePerformance<7>();
extern template void testTreeLessFilterTreePerformance<8>();
extern template void testTreeLessFilterTreePerformance<9>();
