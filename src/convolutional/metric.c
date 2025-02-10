#include "correct/convolutional/metric.h"

// measure the square of the euclidean distance between x and y
// since euclidean dist is sqrt(a^2 + b^2 + ... + n^2), the square is just
//    a^2 + b^2 + ... + n^2
distance_t metric_soft_distance_quadratic(unsigned int hard_x, const uint8_t *soft_y, size_t len) {
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
        
        uint32_t squared = (uint32_t)diff * diff;
        
        squared >>= 3;

        if (squared > (uint32_t)(65535U - dist)) {
            dist = 65535U;
            break;
        }
        
        dist += (distance_t)squared;
    }

    return dist;
}
