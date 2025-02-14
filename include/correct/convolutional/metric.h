#ifndef CORRECT_CONVOLUTIONAL_METRIC_H
#define CORRECT_CONVOLUTIONAL_METRIC_H

#include "correct/convolutional.h"

// measure the hamming distance of two bit strings
// implemented as population count of x XOR y
static inline distance_t metric_distance(unsigned int x, unsigned int y) {
    return (distance_t)popcount(x ^ y);
}

static inline distance_t metric_soft_distance_linear(unsigned int hard_x, const uint8_t *soft_y, size_t len) {
    distance_t dist = 0;

    for (size_t i = 0; i < len; i++) {
        uint8_t soft_x = (hard_x & 1) ? 0xff : 0;
        hard_x >>= 1;
        
        uint16_t diff;
        if (soft_y[i] >= soft_x) {
            diff = soft_y[i] - soft_x;
        } else {
            diff = soft_x - soft_y[i];
        }
        
        uint32_t new_dist = (uint32_t)dist + diff;
        if (new_dist > UINT16_MAX) {
            dist = UINT16_MAX;
        } else {
            dist = (distance_t)new_dist;
        }
    }

    return dist;
}

distance_t metric_soft_distance_quadratic(unsigned int hard_x, const uint8_t *soft_y, size_t len);

#endif  /* CORRECT_CONVOLUTIONAL_METRIC_H */
