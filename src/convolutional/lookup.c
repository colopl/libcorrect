#include "correct/convolutional/lookup.h"

void fill_table(unsigned int rate,
                unsigned int order,
                const polynomial_t *poly,
                unsigned int *table) {
    uint64_t table_size = 1UL << order;
    for (shift_register_t i = 0; i < table_size; i++) {
        unsigned int out = 0;
        unsigned int mask = 1;
        for (unsigned int j = 0; j < rate; j++) {
            out |= ((unsigned int)(popcount(i & poly[j])) % 2) ? mask : 0;
            mask <<= 1;
        }
        table[i] = out;
    }
}

pair_lookup_t pair_lookup_create(unsigned int rate,
                                unsigned int order,
                                const unsigned int *table) {
    pair_lookup_t pairs;

    uint64_t pairs_size = 1UL << (order - 1);
    pairs.keys = malloc(sizeof(unsigned int) * pairs_size);
    pairs.outputs = calloc((size_t)pairs_size + 1, sizeof(unsigned int));
    unsigned int *inv_outputs = calloc(1U << (rate * 2), sizeof(unsigned int));
    unsigned int output_counter = 1;

    for (uint64_t i = 0; i < pairs_size; i++) {
        // first get the concatenated pair of outputs
        unsigned int out = table[i * 2 + 1];
        out <<= rate;
        out |= table[i * 2];

        // does this concatenated output exist in the outputs table yet?
        if (!inv_outputs[out]) {
            // doesn't exist, allocate a new key
            inv_outputs[out] = output_counter;
            pairs.outputs[output_counter] = out;
            output_counter++;
        }
        // set the opaque key for the ith shift register state to the concatenated output entry
        pairs.keys[i] = inv_outputs[out];
    }
    pairs.outputs_len = output_counter;
    
    pairs.output_mask = (1U << rate) - 1U;
    pairs.output_width = rate;
    pairs.distances = calloc(pairs.outputs_len, sizeof(distance_pair_t));
    free(inv_outputs);
    return pairs;
}

void pair_lookup_destroy(pair_lookup_t pairs) {
    free(pairs.keys);
    free(pairs.outputs);
    free(pairs.distances);
}

void pair_lookup_fill_distance(pair_lookup_t pairs, distance_t *distances) {
    for (unsigned int i = 1; i < pairs.outputs_len; i += 1) {
        output_pair_t concat_out = pairs.outputs[i];
        unsigned int i_0 = concat_out & pairs.output_mask;
        concat_out >>= pairs.output_width;
        unsigned int i_1 = concat_out;

        pairs.distances[i] = (distances[i_1] << 16) | distances[i_0];
    }
}
