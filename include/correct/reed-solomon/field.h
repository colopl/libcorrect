#ifndef CORRECT_REED_SOLOMON_FIELD_H
#define CORRECT_REED_SOLOMON_FIELD_H

#include "correct/reed-solomon.h"

/*
field_t *field_create(field_operation_t primitive_poly);
void field_destroy(field_t *field);
field_element_t field_add(field_element_t l, field_element_t r);
field_element_t field_sub(field_element_t l, field_element_t r);
field_element_t field_sum(field_element_t elem, unsigned int n);
field_element_t field_mul(field_t *field, field_element_t l, field_element_t r);
field_element_t field_div(field_t *field, field_element_t l, field_element_t r);
field_logarithm_t field_mul_log(field_logarithm_t l, field_logarithm_t r);
field_logarithm_t field_div_log(field_logarithm_t l, field_logarithm_t r);
field_element_t field_mul_log_element(field_t *field, field_logarithm_t l, field_logarithm_t r);
field_element_t field_pow(field_t *field, field_element_t elem, int pow);
*/

static inline field_element_t field_mul_log_element(const field_t *field, field_logarithm_t l, field_logarithm_t r) {
    // like field_mul_log, but returns a field_element_t
    // because we are doing lookup here, we can safely skip the wrapover check
    field_operation_t res = (field_operation_t)l + (field_operation_t)r;
    return field->exp[res];
}

static inline void field_init(field_t *field, field_operation_t primitive_poly) {
    field->exp = (field_element_t *)malloc(512 * sizeof(field_element_t));
    field->log = (field_logarithm_t *)malloc(256 * sizeof(field_logarithm_t));

    if (!field->exp || !field->log) {
        free(field->exp);
        free(field->log);
        field->exp = NULL;
        field->log = NULL;
        return;
    }

    field_operation_t element = 1;
    field->exp[0] = (field_element_t)element;
    field->log[0] = (field_logarithm_t)0;  // really, it's undefined. we shouldn't ever access this

    field_element_t *exp_ptr = field->exp + 1;
    for (field_operation_t i = 1; i < 512; i++) {
        element = element * 2;
        element = (element > 255) ? (element ^ primitive_poly) : element;
        *exp_ptr++ = (field_element_t)element;

        if (i < 256) {
            field->log[element] = (field_logarithm_t)i;
        }
    }
}

static inline void field_destroy(field_t *field) {
    if (field) {
        if (field->exp) {
            free(field->exp);
        }

        if (field->log) {
            free(field->log);
        }

        free(field);
    }
}

static inline field_t *field_create(field_operation_t primitive_poly) {
    field_t *field = (field_t *)malloc(sizeof(field_t));
    if (!field) {
        return NULL;
    }

    field_init(field, primitive_poly);
    if (!field->exp || !field->log) {
        free(field);
        return NULL;
    }

    return field;
}

static inline field_element_t field_add(field_element_t l, field_element_t r) {
    return l ^ r;
}

static inline field_element_t field_sub(field_element_t l, field_element_t r) {
    return l ^ r;
}

static inline field_element_t field_sum(field_element_t elem, unsigned int n) {
    // we'll do a closed-form expression of the sum, although we could also
    //   choose to call field_add n times

    // since the sum is actually the bytewise XOR operator, this suggests two
    // kinds of values: n odd, and n even

    // if you sum once, you have coeff
    // if you sum twice, you have coeff XOR coeff = 0
    // if you sum thrice, you are back at coeff
    // an even number of XORs puts you at 0
    // an odd number of XORs puts you back at your value

    // so, just throw away all the even n
    return (n & 1) ? elem : 0;
}

static inline field_element_t field_mul(field_t *field, field_element_t l, field_element_t r) {
    if (!field || !l || !r) {
        return 0;
    }

    // if coeff exceeds 255, we would normally have to wrap it back around
    // alpha^255 = 1; alpha^256 = alpha^255 * alpha^1 = alpha^1
    // however, we've constructed exponentiation table so that
    //   we can just directly lookup this result
    // the result must be clamped to [0, 511]
    // the greatest we can see at this step is alpha^255 * alpha^255
    //   = alpha^510
    return field->exp[(field_operation_t)field->log[l] + (field_operation_t)field->log[r]];
}

static inline field_element_t field_div(field_t *field, field_element_t l, field_element_t r) {
    if (!field || !l || !r) {
        return 0;
    }

    // division as subtraction of logarithms

    // if rcoeff is larger, then log[l] - log[r] wraps under
    // so, instead, always add 255. in some cases, we'll wrap over, but
    // that's ok because the exp table runs up to 511.
    field_operation_t res = (field_operation_t)(255 + (field_operation_t)field->log[l] - (field_operation_t)field->log[r]);
    return field->exp[res];
}

static inline field_logarithm_t field_mul_log(field_logarithm_t l, field_logarithm_t r) {
    // this function performs the equivalent of field_mul on two logarithms
    // we save a little time by skipping the lookup step at the beginning
    field_operation_t res = (field_operation_t)l + (field_operation_t)r;

    // because we arent using the table, the value we return must be a valid logarithm
    // which we have decided must live in [0, 255] (they are 8-bit values)
    // ensuring this makes it so that multiple muls will not reach past the end of the
    // exp table whenever we finally convert back to an element
    return (field_logarithm_t)((res > 255) ? (res - 255) : res);
}

static inline field_logarithm_t field_div_log(field_logarithm_t l, field_logarithm_t r) {
    // like field_mul_log, this performs field_div without going through a field_element_t
    field_operation_t res = (field_operation_t)(255 + (field_operation_t)l - (field_operation_t)r);
    return (field_logarithm_t)((res > 255) ? (res - 255) : res);
}

static inline field_element_t field_pow(field_t *field, field_element_t elem, int pow) {
    if (!field || !elem) {
        return 0;
    }

    // take the logarithm, multiply, and then "exponentiate"
    // n.b. the exp table only considers powers of alpha, the primitive element
    // but here we have an arbitrary coeff
    field_logarithm_t log = field->log[elem];
    int res_log = log * pow;
    int mod = res_log % 255;
    if (mod < 0) {
        mod += 255;
    }

    return field->exp[mod];
}

#endif  /* CORRECT_REED_SOLOMON_FIELD_H */
