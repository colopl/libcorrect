#include "correct/convolutional/lookup.h"

void fill_table(unsigned int rate,
                unsigned int order,
                const polynomial_t *poly,
                unsigned int *table) {
    uint64_t table_size = 1ULL << order;
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

void pair_lookup_destroy(pair_lookup_t *pairs) {
    if (pairs) {
        if (pairs->keys) {
            free(pairs->keys);
        }

        if (pairs->outputs) {
            free(pairs->outputs);
        }

        if (pairs->distances) {
            free(pairs->distances);
        }
        
        free(pairs);
    }
}

pair_lookup_t *pair_lookup_create(unsigned int rate, unsigned int order, const unsigned int *table) {
    unsigned int *inv_outputs = (unsigned int *)calloc((size_t)1 << (rate * 2), sizeof(unsigned int));
    if (!inv_outputs) {
        return NULL;
    }

    pair_lookup_t *pairs = (pair_lookup_t *)calloc(1, sizeof(pair_lookup_t));
    if (!pairs) {
        goto bailout;
    }

    size_t pairs_size = (size_t)1 << (order - 1);
    
    pairs->keys = malloc(sizeof(unsigned int) * pairs_size);
    if (!pairs->keys) {
        goto bailout;
    }

    pairs->outputs = calloc(pairs_size + 1, sizeof(unsigned int));
    if (!pairs->outputs) {
        goto bailout;
    }

    unsigned int output_counter = 1;

    for (size_t i = 0; i < pairs_size; i++) {
        unsigned int out = table[i * 2 + 1];
        out <<= rate;
        out |= table[i * 2];

        if (!inv_outputs[out]) {
            inv_outputs[out] = output_counter;
            pairs->outputs[output_counter] = out;
            output_counter++;
        }
        pairs->keys[i] = inv_outputs[out];
    }
    pairs->outputs_len = output_counter;
    
    pairs->output_mask = (1U << rate) - 1U;
    pairs->output_width = rate;
    pairs->distances = calloc(pairs->outputs_len, sizeof(distance_pair_t));\
    if (!pairs->distances) {
        goto bailout;
    }

    free(inv_outputs);

    return pairs;

    bailout:
        if (inv_outputs) {
            free(inv_outputs);
        }

        if (pairs) {
            if (pairs->keys) {
                free(pairs->keys);
            }
            if (pairs->outputs) {
                free(pairs->outputs);
            }
            free(pairs);
        }

        return NULL;
}

void pair_lookup_fill_distance(pair_lookup_t *pairs, distance_t *distances) {
    for (unsigned int i = 1; i < pairs->outputs_len; i += 1) {
        output_pair_t concat_out = pairs->outputs[i];
        unsigned int i_0 = concat_out & pairs->output_mask;
        concat_out >>= pairs->output_width;
        unsigned int i_1 = concat_out;

        pairs->distances[i] = (distances[i_1] << 16) | distances[i_0];
    }
}
