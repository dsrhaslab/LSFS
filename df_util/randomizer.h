//
// Created by danielsf97 on 6/25/20.
//

#ifndef P2PFS_RANDOMIZER_H
#define P2PFS_RANDOMIZER_H

#include <random>

inline float random_float(float low, float high) {
    thread_local static std::random_device rd;
    thread_local static std::mt19937 rng(rd());
    thread_local std::uniform_real_distribution<float> urd;
    return urd(rng, decltype(urd)::param_type{low,high});
}

#endif //P2PFS_RANDOMIZER_H
