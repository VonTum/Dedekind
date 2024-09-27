#include <immintrin.h>
#include <stdint.h>
// Iterates through 32 bits for up to 32 given bitsets of size 64*stride. 
// Expects a function of the form: void(uint32_t bits, size_t bitIndex, size_t blockIndex)
// It will be called 64*stride times per block. 
template<typename Func>
inline void transpose_per_32_bits(const uint64_t* bitBuffer, size_t stride, size_t numBitsets, Func func) {
    __m256i strideAVX = _mm256_set1_epi64x(stride*4);
    __m256i maxIndex = _mm256_set1_epi64x(numBitsets * stride);

    // Dumb cast for type checking
    const long long int* bitBufferLLI = reinterpret_cast<const long long int*>(bitBuffer);

    for(size_t blockIndex = 0; blockIndex < (numBitsets + 31) / 32; blockIndex++) {
        for(size_t strideSection = 0; strideSection < stride; strideSection++) {
            __m256i indices = _mm256_set_epi64x(stride*(blockIndex * 32 + 3) + strideSection, stride*(blockIndex * 32 + 2) + strideSection, stride*(blockIndex * 32 + 1) + strideSection, strideSection + stride * blockIndex * 32);
            // Gather a full 64-bit wide column of our matrix
            __m256i g0 = _mm256_mask_i64gather_epi64(_mm256_setzero_si256(), bitBufferLLI, indices, _mm256_cmpgt_epi64(maxIndex, indices), 8);
            indices = _mm256_add_epi64(indices, strideAVX);
            __m256i g1 = _mm256_mask_i64gather_epi64(_mm256_setzero_si256(), bitBufferLLI, indices, _mm256_cmpgt_epi64(maxIndex, indices), 8);
            indices = _mm256_add_epi64(indices, strideAVX);
            __m256i g2 = _mm256_mask_i64gather_epi64(_mm256_setzero_si256(), bitBufferLLI, indices, _mm256_cmpgt_epi64(maxIndex, indices), 8);
            indices = _mm256_add_epi64(indices, strideAVX);
            __m256i g3 = _mm256_mask_i64gather_epi64(_mm256_setzero_si256(), bitBufferLLI, indices, _mm256_cmpgt_epi64(maxIndex, indices), 8);
            indices = _mm256_add_epi64(indices, strideAVX);
            __m256i g4 = _mm256_mask_i64gather_epi64(_mm256_setzero_si256(), bitBufferLLI, indices, _mm256_cmpgt_epi64(maxIndex, indices), 8);
            indices = _mm256_add_epi64(indices, strideAVX);
            __m256i g5 = _mm256_mask_i64gather_epi64(_mm256_setzero_si256(), bitBufferLLI, indices, _mm256_cmpgt_epi64(maxIndex, indices), 8);
            indices = _mm256_add_epi64(indices, strideAVX);
            __m256i g6 = _mm256_mask_i64gather_epi64(_mm256_setzero_si256(), bitBufferLLI, indices, _mm256_cmpgt_epi64(maxIndex, indices), 8);
            indices = _mm256_add_epi64(indices, strideAVX);
            __m256i g7 = _mm256_mask_i64gather_epi64(_mm256_setzero_si256(), bitBufferLLI, indices, _mm256_cmpgt_epi64(maxIndex, indices), 8);

            __m256i shuffleMask = _mm256_broadcastsi128_si256(_mm_set_epi8(15, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 0));

            // Shuffle so that the two bits per 128 bit lane are together. 
            __m256i shuf0 = _mm256_shuffle_epi8(g0, shuffleMask);
            __m256i shuf1 = _mm256_shuffle_epi8(g1, shuffleMask);
            __m256i shuf2 = _mm256_shuffle_epi8(g2, shuffleMask);
            __m256i shuf3 = _mm256_shuffle_epi8(g3, shuffleMask);
            __m256i shuf4 = _mm256_shuffle_epi8(g4, shuffleMask);
            __m256i shuf5 = _mm256_shuffle_epi8(g5, shuffleMask);
            __m256i shuf6 = _mm256_shuffle_epi8(g6, shuffleMask);
            __m256i shuf7 = _mm256_shuffle_epi8(g7, shuffleMask);

            // Now do a 16-wide transpose per 128 bit lane. 
            __m256i unp0_A = _mm256_unpacklo_epi16(shuf0, shuf1);
            __m256i unp1_A = _mm256_unpackhi_epi16(shuf0, shuf1);
            __m256i unp2_A = _mm256_unpacklo_epi16(shuf2, shuf3);
            __m256i unp3_A = _mm256_unpackhi_epi16(shuf2, shuf3);
            __m256i unp4_A = _mm256_unpacklo_epi16(shuf4, shuf5);
            __m256i unp5_A = _mm256_unpackhi_epi16(shuf4, shuf5);
            __m256i unp6_A = _mm256_unpacklo_epi16(shuf6, shuf7);
            __m256i unp7_A = _mm256_unpackhi_epi16(shuf6, shuf7);

            __m256i unp0_B = _mm256_unpacklo_epi32(unp0_A, unp2_A);
            __m256i unp1_B = _mm256_unpackhi_epi32(unp0_A, unp2_A);
            __m256i unp2_B = _mm256_unpacklo_epi32(unp1_A, unp3_A);
            __m256i unp3_B = _mm256_unpackhi_epi32(unp1_A, unp3_A);
            __m256i unp4_B = _mm256_unpacklo_epi32(unp4_A, unp6_A);
            __m256i unp5_B = _mm256_unpackhi_epi32(unp4_A, unp6_A);
            __m256i unp6_B = _mm256_unpacklo_epi32(unp5_A, unp7_A);
            __m256i unp7_B = _mm256_unpackhi_epi32(unp5_A, unp7_A);

            __m256i output[8];
            output[0] = _mm256_unpacklo_epi64(unp0_B, unp4_B);
            output[1] = _mm256_unpackhi_epi64(unp0_B, unp4_B);
            output[2] = _mm256_unpacklo_epi64(unp1_B, unp5_B);
            output[6] = _mm256_unpackhi_epi64(unp1_B, unp5_B);
            output[4] = _mm256_unpacklo_epi64(unp2_B, unp6_B);
            output[5] = _mm256_unpackhi_epi64(unp2_B, unp6_B);
            output[3] = _mm256_unpacklo_epi64(unp3_B, unp7_B);
            output[7] = _mm256_unpackhi_epi64(unp3_B, unp7_B);

            // So the output now looks like 32 bytes per vector register, where each byte has the bits for 8 different BFs. 
            // We shift them left by 1 8x, and movemask to get 8 of the bits at a time. 
            for(int i = 0; i < 8; i++) {
                __m256i part = output[i];
                size_t bitSection = strideSection * 64 + i * 8;

                func(_mm256_movemask_epi8(_mm256_slli_epi16(part, 7)), bitSection+0, blockIndex);
                func(_mm256_movemask_epi8(_mm256_slli_epi16(part, 6)), bitSection+1, blockIndex);
                func(_mm256_movemask_epi8(_mm256_slli_epi16(part, 5)), bitSection+2, blockIndex);
                func(_mm256_movemask_epi8(_mm256_slli_epi16(part, 4)), bitSection+3, blockIndex);
                func(_mm256_movemask_epi8(_mm256_slli_epi16(part, 3)), bitSection+4, blockIndex);
                func(_mm256_movemask_epi8(_mm256_slli_epi16(part, 2)), bitSection+5, blockIndex);
                func(_mm256_movemask_epi8(_mm256_slli_epi16(part, 1)), bitSection+6, blockIndex);
                func(_mm256_movemask_epi8(part),                       bitSection+7, blockIndex);
            }
        }
    }
}
