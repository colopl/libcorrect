#include "correct/reed-solomon/reed-solomon.h"

// coeff must be of size nroots + 1
// e.g. 2 roots (x + alpha)(x + alpha^2) yields a poly with 3 terms x^2 + g0*x + g1
static polynomial_t *reed_solomon_build_generator(field_t *field, unsigned int nroots, field_element_t first_consecutive_root, unsigned int root_gap, field_element_t *roots) {
    // generator has order 2*t
    // of form (x + alpha^1)(x + alpha^2)...(x - alpha^2*t)
    for (unsigned int i = 0; i < nroots; i++) {
        roots[i] = field->exp[(root_gap * (i + first_consecutive_root)) % 255];
    }
    return polynomial_create_from_roots(field, nroots, roots);
}

void correct_reed_solomon_destroy(correct_reed_solomon *rs) {
    if (rs) {
        if (rs->field) {
            field_destroy(rs->field);
        }

        if (rs->generator) {
            polynomial_destroy(rs->generator);
        }

        if (rs->generator_roots) {
            free(rs->generator_roots);
        }

        if (rs->generator_root_exp) {
            for (unsigned int i = 0; i < rs->min_distance; i++) {
                free(rs->generator_root_exp[i]);
            }

            free(rs->generator_root_exp);
        }

        if (rs->encoded_polynomial) {
            polynomial_destroy(rs->encoded_polynomial);
        }

        if (rs->encoded_remainder) {
            polynomial_destroy(rs->encoded_remainder);
        }

        if (rs->syndromes) {
            free(rs->syndromes);
        }

        if (rs->modified_syndromes) {
            free(rs->modified_syndromes);
        }

        if (rs->received_polynomial) {
            polynomial_destroy(rs->received_polynomial);
        }

        if (rs->error_locator) {
            polynomial_destroy(rs->error_locator);
        }

        if (rs->error_locator_log) {
            polynomial_destroy(rs->error_locator_log);
        }

        if (rs->erasure_locator) {
            polynomial_destroy(rs->erasure_locator);
        }

        if (rs->error_roots) {
            free(rs->error_roots);
        }

        if (rs->error_vals) {
            free(rs->error_vals);
        }

        if (rs->error_locations) {
            free(rs->error_locations);
        }

        if (rs->element_exp) {
            for (field_operation_t i = 0; i < 256; i++) {
                free(rs->element_exp[i]);
            }

            free(rs->element_exp);
        }

        if (rs->last_error_locator) {
            polynomial_destroy(rs->last_error_locator);
        }

        if (rs->error_evaluator) {
            polynomial_destroy(rs->error_evaluator);
        }

        if (rs->error_locator_derivative) {
            polynomial_destroy(rs->error_locator_derivative);
        }

        if (rs->init_from_roots_scratch[0]) {
            polynomial_destroy(rs->init_from_roots_scratch[0]);
        }

        if (rs->init_from_roots_scratch[1]) {
            polynomial_destroy(rs->init_from_roots_scratch[1]);
        }

        free(rs);
    }
}

correct_reed_solomon *correct_reed_solomon_create(field_operation_t primitive_polynomial, field_logarithm_t first_consecutive_root, field_logarithm_t generator_root_gap, size_t num_roots) {
    correct_reed_solomon *rs = (correct_reed_solomon *)calloc(1, sizeof(correct_reed_solomon));
    if (!rs) {
        return NULL;
    }

    rs->field = field_create(primitive_polynomial);
    if (!rs->field) {
        correct_reed_solomon_destroy(rs);
        return NULL;
    }

    rs->block_length = 255;
    rs->min_distance = num_roots;
    rs->message_length = rs->block_length - rs->min_distance;

    rs->first_consecutive_root = first_consecutive_root;
    rs->generator_root_gap = generator_root_gap;

    rs->generator_roots = (field_element_t *)malloc(rs->min_distance * sizeof(field_element_t));
    if (!rs->generator_roots) {
        correct_reed_solomon_destroy(rs);
        return NULL;
    }

    rs->generator = reed_solomon_build_generator(rs->field, (unsigned int)rs->min_distance, rs->first_consecutive_root, rs->generator_root_gap, rs->generator_roots);
    if (!rs->generator) {
        correct_reed_solomon_destroy(rs);
        return NULL;
    }

    rs->encoded_polynomial = polynomial_create((unsigned int)(rs->block_length - 1));
    if (!rs->encoded_polynomial) {
        correct_reed_solomon_destroy(rs);
        return NULL;
    }
    rs->encoded_remainder = polynomial_create((unsigned int)(rs->block_length - 1));
    if (!rs->encoded_remainder) {
        correct_reed_solomon_destroy(rs);
        return NULL;
    }

    rs->has_init_decode = false;

    return rs;
}
