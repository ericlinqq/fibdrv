#ifndef _BIGNUM_H_
#define _BIGNUM_H_

#include <linux/types.h>

#if (defined(__GNUC__) || defined(__clang__)) && \
    (defined(__LP64__) || defined(__x86_64__) || defined(__aarch64__))
#define BN_BIT 64
#define _clz(x) __builtin_clzll((x))
typedef u_int64_t bn_data;
typedef __int128_t bn_data_tmp;
#else
#define BN_BIT 32
#define _clz(x) __builtin_clz((x))
typedef u_int32_t bn_data;
typedef u_int64_t bn_data_tmp;
#endif

static inline unsigned int clz(bn_data x)
{
    return x ? _clz(x) : BN_BIT;
}


typedef struct {
    bn_data *num;
    unsigned int size;
    unsigned int sign : 1;
} bn, bn_t[1];


void bn_init(bn *p);

void bn_free(bn *p);

/* C = A + B */
void bn_add(bn *c, const bn *a, const bn *b);

/* C = A - B */
void bn_sub(bn *c, const bn *a, const bn *b);

/* C = A * B */
void bn_mult(bn *c, const bn *a, const bn *b);

void bn_lshift(bn *src, unsigned int shift);

char *bn_to_string(const bn *p);

void bn_fib(bn *p, long long k);
void bn_fib_fdoubling(bn *p, long long k);

#endif
