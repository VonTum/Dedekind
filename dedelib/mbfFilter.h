
#include <vector>
#include <string>

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

template<unsigned int Variables>
void estimateDPlusOneWithPairs(const std::vector<std::string>& args);
extern template void estimateDPlusOneWithPairs<1>(const std::vector<std::string>& args);
extern template void estimateDPlusOneWithPairs<2>(const std::vector<std::string>& args);
extern template void estimateDPlusOneWithPairs<3>(const std::vector<std::string>& args);
extern template void estimateDPlusOneWithPairs<4>(const std::vector<std::string>& args);
extern template void estimateDPlusOneWithPairs<5>(const std::vector<std::string>& args);
extern template void estimateDPlusOneWithPairs<6>(const std::vector<std::string>& args);
extern template void estimateDPlusOneWithPairs<7>(const std::vector<std::string>& args);
extern template void estimateDPlusOneWithPairs<8>(const std::vector<std::string>& args);
extern template void estimateDPlusOneWithPairs<9>(const std::vector<std::string>& args);
