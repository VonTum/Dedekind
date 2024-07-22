
double estimateDedekindNumber(int n);

void naiveD10Estimation();

template<unsigned int Variables>
void estimateNextDedekindNumber();


extern template void estimateNextDedekindNumber<1>();
extern template void estimateNextDedekindNumber<2>();
extern template void estimateNextDedekindNumber<3>();
extern template void estimateNextDedekindNumber<4>();
extern template void estimateNextDedekindNumber<5>();
extern template void estimateNextDedekindNumber<6>();
extern template void estimateNextDedekindNumber<7>();
extern template void estimateNextDedekindNumber<8>();
extern template void estimateNextDedekindNumber<9>();

void codeGenGetSignature();

template<unsigned int Variables>
void makeSignatureStatistics();

extern template void makeSignatureStatistics<1>();
extern template void makeSignatureStatistics<2>();
extern template void makeSignatureStatistics<3>();
extern template void makeSignatureStatistics<4>();
extern template void makeSignatureStatistics<5>();
extern template void makeSignatureStatistics<6>();
extern template void makeSignatureStatistics<7>();
extern template void makeSignatureStatistics<8>();
extern template void makeSignatureStatistics<9>();
