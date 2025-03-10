#include <stdlib.h>
#include <string.h>

#include "fec_shim.h"

typedef struct {
    correct_reed_solomon *rs;
    unsigned int msg_length;
    unsigned int block_length;
    unsigned int num_roots;
    uint8_t *msg_out;
    unsigned int pad;
    uint8_t *erasures;
} reed_solomon_shim;

void free_rs_char(void *rs) {
    reed_solomon_shim *shim = (reed_solomon_shim *)rs;

    if (!shim) {
        return;
    }
 
    if (shim->rs) {
        correct_reed_solomon_destroy(shim->rs);
    }

    if (shim->msg_out) {
        free(shim->msg_out);
    }

    if (shim->erasures) {
        free(shim->erasures);
    }
    
    free(shim);
}

void *init_rs_char(int symbol_size, int primitive_polynomial, int first_consecutive_root, int root_gap, int number_roots, unsigned int pad) {
    if (symbol_size != 8) {
        return NULL;
    }

    reed_solomon_shim *shim = (reed_solomon_shim *)malloc(sizeof(reed_solomon_shim));
    if (!shim) {
        return NULL;
    }

    shim->pad = pad;
    shim->block_length = 255 - pad;
    shim->num_roots = (unsigned int)number_roots;
    shim->msg_length = shim->block_length - (unsigned int)number_roots;
    shim->rs = correct_reed_solomon_create((uint16_t)primitive_polynomial, (uint8_t)first_consecutive_root, (uint8_t)root_gap, (size_t)number_roots);
    if (!shim->rs) {
        free_rs_char(shim);
        return NULL;        
    }

    shim->msg_out = (uint8_t *)malloc(shim->block_length);
    if (!shim->msg_out) {
        free_rs_char(shim);
        return NULL; 
    }

    shim->erasures = (uint8_t *)malloc((size_t)number_roots);
    if (!shim->msg_out) {
        free_rs_char(shim);
        return NULL; 
    }

    return shim;
}

void encode_rs_char(void *rs, const unsigned char *msg, unsigned char *parity) {
    reed_solomon_shim *shim = (reed_solomon_shim *)rs;
    correct_reed_solomon_encode(shim->rs, msg, shim->msg_length, shim->msg_out);
    memcpy(parity, shim->msg_out + shim->msg_length, shim->num_roots);
}

int decode_rs_char(void *rs, unsigned char *block, int *erasure_locations, int num_erasures) {
    reed_solomon_shim *shim = (reed_solomon_shim *)rs;

    for (int i = 0; i < num_erasures; i++) {
        shim->erasures[i] = (uint8_t)((uint8_t)erasure_locations[i] - shim->pad);
    }

    return (int)correct_reed_solomon_decode_with_erasures(shim->rs, block, shim->block_length, shim->erasures, (size_t)num_erasures, block);
}

typedef struct {
    correct_convolutional *conv;
    unsigned int rate;
    unsigned int order;
    uint8_t *buf;
    size_t buf_len;
    uint8_t *read_iter;
    uint8_t *write_iter;
} convolutional_shim;

static correct_convolutional_polynomial_t r12k7[] = {V27POLYA, V27POLYB};

static correct_convolutional_polynomial_t r12k9[] = {V29POLYA, V29POLYB};

static correct_convolutional_polynomial_t r13k9[] = {V39POLYA, V39POLYB,
                                                     V39POLYC};

static correct_convolutional_polynomial_t r16k15[] = {
    V615POLYA, V615POLYB, V615POLYC, V615POLYD, V615POLYE, V615POLYF};

/* Common methods */
static void delete_viterbi(void *vit) {
    convolutional_shim *shim = (convolutional_shim *)vit;

    if (!shim) {
        return;
    }

    if (shim->buf) {
        free(shim->buf);
    }

    if (shim->conv) {
        correct_convolutional_destroy(shim->conv);
    }

    free(shim);
}

static void *create_viterbi(unsigned int num_decoded_bits, unsigned int rate, unsigned int order, correct_convolutional_polynomial_t *poly) {
    convolutional_shim *shim = (convolutional_shim *)malloc(sizeof(convolutional_shim));
    if (!shim) {
        return NULL;
    }

    size_t num_decoded_bytes = (num_decoded_bits % 8) ? (num_decoded_bits / 8 + 1) : num_decoded_bits / 8;

    shim->rate = rate;
    shim->order = order;
    shim->buf = (uint8_t *)malloc(num_decoded_bytes);
    if (!shim->buf) {
        delete_viterbi(shim);
        return NULL;
    }

    shim->buf_len = num_decoded_bytes;
    shim->conv = correct_convolutional_create(rate, order, poly);
    if (!shim->conv) {
        delete_viterbi(shim);
        return NULL;
    }

    shim->read_iter = shim->buf;
    shim->write_iter = shim->buf;

    return shim;
}

static void init_viterbi(void *vit) {
    convolutional_shim *shim = (convolutional_shim *)vit;
    shim->read_iter = shim->buf;
    shim->write_iter = shim->buf;
}

static void update_viterbi_blk(void *vit, const unsigned char *encoded_soft, unsigned int num_encoded_groups) {
    convolutional_shim *shim = (convolutional_shim *)vit;

    // don't overwrite our buffer
    size_t rem = (size_t)((shim->buf + shim->buf_len) - shim->write_iter);
    size_t rem_bits = 8 * rem;
    // this math isn't very clear
    // here we sort of do the opposite of what liquid-dsp does
    size_t n_write_bits = num_encoded_groups - (shim->order - 1);
    if (n_write_bits > rem_bits) {
        size_t reduction = n_write_bits - rem_bits;
        num_encoded_groups -= (unsigned int)reduction;
        n_write_bits -= reduction;
    }

    // what if n_write_bits isn't a multiple of 8?
    // libcorrect can't start and stop at arbitrary indices...
    correct_convolutional_decode_soft(shim->conv, encoded_soft, num_encoded_groups * shim->rate, shim->write_iter);
    shim->write_iter += n_write_bits / 8;
}

static void chainback_viterbi(void *vit, unsigned char *decoded, unsigned int num_decoded_bits) {
    convolutional_shim *shim = (convolutional_shim *)vit;

    // num_decoded_bits not a multiple of 8?
    // this is a similar problem to update_viterbi_blk
    // although here we could actually resolve a non-multiple of 8
    size_t rem = (size_t)(shim->write_iter - shim->read_iter);
    size_t rem_bits = 8 * rem;

    if (num_decoded_bits > rem_bits) {
        num_decoded_bits = (unsigned int)rem_bits;
    }

    size_t num_decoded_bytes = (num_decoded_bits % 8) ? (num_decoded_bits / 8 + 1) : num_decoded_bits / 8;
    memcpy(decoded, shim->read_iter, num_decoded_bytes);

    shim->read_iter += num_decoded_bytes;
}

/* Rate 1/2, k = 7 */
void *create_viterbi27(int num_decoded_bits) {
    return create_viterbi((unsigned int)num_decoded_bits, 2, 7, r12k7);
}

void delete_viterbi27(void *vit) {
    delete_viterbi(vit);
}

int init_viterbi27(void *vit, int _) {
    (void) _;

    init_viterbi(vit);
    return 0;
}

int update_viterbi27_blk(void *vit, unsigned char *encoded_soft, int num_encoded_groups) {
    update_viterbi_blk(vit, encoded_soft, (unsigned int)num_encoded_groups);
    return 0;
}

int chainback_viterbi27(void *vit, unsigned char *decoded, unsigned int num_decoded_bits, unsigned int _) {
    (void) _;

    chainback_viterbi(vit, decoded, num_decoded_bits);
    return 0;
}

/* Rate 1/2, k = 9 */
void *create_viterbi29(int num_decoded_bits) {
    return create_viterbi((unsigned int)num_decoded_bits, 2, 9, r12k9);
}

void delete_viterbi29(void *vit) {
    delete_viterbi(vit);
}

int init_viterbi29(void *vit, int _) {
    (void) _;

    init_viterbi(vit);
    return 0;
}

int update_viterbi29_blk(void *vit, unsigned char *encoded_soft, int num_encoded_groups) {
    update_viterbi_blk(vit, encoded_soft, (unsigned int)num_encoded_groups);
    return 0;
}

int chainback_viterbi29(void *vit, unsigned char *decoded, unsigned int num_decoded_bits, unsigned int _) {
    (void) _;

    chainback_viterbi(vit, decoded, num_decoded_bits);
    return 0;
}

/* Rate 1/3, k = 9 */
void *create_viterbi39(int num_decoded_bits) {
    return create_viterbi((unsigned int)num_decoded_bits, 3, 9, r13k9);
}

void delete_viterbi39(void *vit) {
    delete_viterbi(vit);
}

int init_viterbi39(void *vit, int _) {
    (void) _;

    init_viterbi(vit);
    return 0;
}

int update_viterbi39_blk(void *vit, unsigned char *encoded_soft, int num_encoded_groups) {
    update_viterbi_blk(vit, encoded_soft, (unsigned int)num_encoded_groups);
    return 0;
}

int chainback_viterbi39(void *vit, unsigned char *decoded, unsigned int num_decoded_bits, unsigned int _) {
    (void) _;

    chainback_viterbi(vit, decoded, num_decoded_bits);
    return 0;
}

/* Rate 1/6, k = 15 */
void *create_viterbi615(int num_decoded_bits) {
    return create_viterbi((unsigned int)num_decoded_bits, 6, 15, r16k15);
}

void delete_viterbi615(void *vit) {
    delete_viterbi(vit);
}

int init_viterbi615(void *vit, int _) {
    (void) _;

    init_viterbi(vit);
    return 0;
}

int update_viterbi615_blk(void *vit, unsigned char *encoded_soft, int num_encoded_groups) {
    update_viterbi_blk(vit, encoded_soft, (unsigned int)num_encoded_groups);
    return 0;
}

int chainback_viterbi615(void *vit, unsigned char *decoded, unsigned int num_decoded_bits, unsigned int _) {
    (void) _;

    chainback_viterbi(vit, decoded, num_decoded_bits);
    return 0;
}
