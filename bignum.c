#include "bignum.h"
#include <linux/minmax.h>
#include <linux/slab.h>


void bn_init(bn *p)
{
    if (!p)
        return;
    p->sign = 0;
    p->size = 1;
    p->num = kzalloc(sizeof(bn_data) * p->size, GFP_KERNEL);
}


void bn_free(bn *p)
{
    if (!p)
        return;
    kfree(p->num);
    p->num = NULL;
}

/* data loss is ignored when shrinking size */
static void bn_resize(bn *p, unsigned int size)
{
    if (!p)
        return;
    if (p->size == size)
        return;

    p->num = krealloc(p->num, sizeof(bn_data) * size, GFP_KERNEL);
    if (!p->num)
        return;

    if (size > p->size)
        memset(p->num + p->size, 0, sizeof(bn_data) * (size - p->size));

    p->size = size;
}

static void bn_cpy(bn *dest, const bn *src)
{
    bn_resize(dest, src->size);
    dest->sign = src->sign;
    memcpy(dest->num, src->num, sizeof(bn_data) * src->size);
}

static unsigned int bn_clz(const bn *p)
{
    unsigned int count = 0;
    for (int i = p->size - 1; i >= 0; i--) {
        if (p->num[i]) {
            count += clz(p->num[i]);
            return count;
        } else {
            count += BN_BIT;
        }
    }
    return count;
}

static inline unsigned int bn_digit(const bn *p)
{
    return BN_BIT * p->size - bn_clz(p);
}

/* Compare |A| and |B|*/
static int bn_cmp(const bn *a, const bn *b)
{
    if (a->size > b->size)
        return 1;
    if (a->size < b->size)
        return -1;
    for (int i = a->size - 1; i >= 0; i--) {
        if (a->num[i] > b->num[i])
            return 1;
        if (a->num[i] < b->num[i])
            return -1;
    }
    return 0;
}


/* |C| = |A| + |B| */
static void _bn_add(bn *c, const bn *a, const bn *b)
{
    unsigned int digit = max(bn_digit(a), bn_digit(b)) + 1;
    digit = DIV_ROUND_UP(digit, BN_BIT) + !digit;
    bn_resize(c, digit);

    u_bn_data_tmp carry = 0;
    for (int i = 0; i < c->size; i++) {
        bn_data tmp1 = (i < a->size) ? a->num[i] : 0;
        bn_data tmp2 = (i < b->size) ? b->num[i] : 0;
        carry += (u_bn_data_tmp) tmp1 + tmp2;
        c->num[i] = carry;
        carry >>= BN_BIT;
    }
}


/* |C| = |A| - |B|, Assume |A| > |B| */
static void _bn_sub(bn *c, const bn *a, const bn *b)
{
    unsigned int csize = max(a->size, b->size);
    bn_resize(c, csize);

    bn_data_tmp carry = 0;
    for (int i = 0; i < c->size; i++) {
        bn_data tmp1 = (i < a->size) ? a->num[i] : 0;
        bn_data tmp2 = (i < b->size) ? b->num[i] : 0;
        carry = (bn_data_tmp) tmp1 - tmp2 - carry;
        c->num[i] =
            carry + (-(bn_data_tmp) (carry < 0) & ((bn_data_tmp) 1 << BN_BIT));
        carry = (carry < 0);
    }

    /* Remove leading zeros */
    csize = bn_clz(c) / BN_BIT;
    if (csize == c->size)  // c is 0
        --csize;
    bn_resize(c, c->size - csize);
}

/* C = A + B */
void bn_add(bn *c, const bn *a, const bn *b)
{
    /* both positive or negative */
    if (a->sign == b->sign) {
        _bn_add(c, a, b);
        c->sign = a->sign;
        return;
    }
    /* Make sure a > 0 and b < 0 */
    if (a->sign)
        swap(a, b);

    // compare |a| and |b|
    int cmp = bn_cmp(a, b);

    switch (cmp) {
    case 1:
        // |a| > |b| and b < 0, c = a - |b|
        _bn_sub(c, a, b);
        c->sign = 0;
        break;
    case -1:
        // |a| < |b| and b < 0, c = -(|b| - a)
        _bn_sub(c, b, a);
        c->sign = 1;
        break;
    case 0:
        // |a| == |b|
        bn_resize(c, 1);
        c->num[0] = 0;
        c->sign = 0;
        break;
    default:
        return;
    }
}


/* C = A - B */
void bn_sub(bn *c, const bn *a, const bn *b)
{
    bn tmp = *b;
    tmp.sign ^= 1;
    bn_add(c, a, &tmp);
}


/* C = A * B */
void bn_mult(bn *c, const bn *a, const bn *b)
{
    unsigned int csize = a->size + b->size;

    bn *tmp = NULL;
    bn_t out;
    if (c == a || c == b) {
        tmp = c;
        c = out;
        bn_init(c);
    }
    memset(c->num, 0, sizeof(bn_data) * c->size);
    bn_resize(c, csize);



    for (int i = 0; i < a->size; i++) {
        u_bn_data_tmp carry = 0;
        for (int j = 0; j < b->size; j++) {
            u_bn_data_tmp t = (u_bn_data_tmp) c->num[i + j] +
                              (u_bn_data_tmp) a->num[i] * b->num[j] + carry;
            c->num[i + j] = (bn_data) t;
            carry = t >> BN_BIT;
        }
        c->num[i + b->size] = (bn_data) carry;
    }

    while (csize > 1 && c->num[csize - 1] == 0)
        csize--;
    if (csize != c->size)
        bn_resize(c, csize);

    c->sign = a->sign ^ b->sign;

    if (tmp) {
        bn_free(tmp);
        *tmp = *c;
    }
}

void bn_lshift(bn *src, unsigned int shift)
{
    shift %= BN_BIT;
    if (!shift)
        return;

    unsigned int lzeros = bn_clz(src);

    bn_resize(src, src->size + (shift > lzeros));

    for (int i = src->size - 1; i > 0; i--)
        src->num[i] =
            src->num[i] << shift | src->num[i - 1] >> (BN_BIT - shift);
    src->num[0] <<= shift;
}

char *bn_to_string(const bn *p)
{
    /* log10(x) = log2(x) / log2(10) ~= log2(x) / 3.32 */
    unsigned int len = BN_BIT * p->size / 3 + 2;
    char *s = kmalloc(sizeof(char) * len, GFP_KERNEL);
    memset(s, '0', len - 1);
    s[len - 1] = '\0';

    for (int i = p->size - 1; i >= 0; i--) {
        for (bn_data n = (bn_data) 1 << (BN_BIT - 1); n; n >>= 1) {
            int carry = !!(n & p->num[i]);
            for (int j = len - 2; j >= 0; j--) {
                s[j] += s[j] - '0' + carry;  // multiply by 2
                carry = (s[j] > '9');
                s[j] -= 10 & -carry;
            }
        }
    }

    // leading zeros
    char *s_tmp;
    for (s_tmp = s; *s_tmp == '0' && *(s_tmp + 1) != '\0'; s_tmp++, len--)
        ;
    if (p->sign) {
        *(--s_tmp) = '-';
        len++;
    }

    memmove(s, s_tmp, len);
    return s;
}

void bn_fib(bn *p, long long k)
{
    p->sign = 0;
    bn_resize(p, 1);
    if (k <= 2) {
        p->num[0] = !!k;
        return;
    }

    bn_t a, b;
    bn_init(a);
    bn_init(b);
    a->num[0] = 0;
    b->num[0] = 1;

    for (long long i = 2; i < k; i++) {
        bn_add(p, a, b);
        bn_cpy(a, p);
        swap(*a, *b);
    }
    bn_add(p, a, b);
    bn_free(a);
    bn_free(b);
}

void bn_fib_fdoubling(bn *p, long long k)
{
    p->sign = 0;
    bn_resize(p, 1);
    if (k <= 2) {
        p->num[0] = !!k;
        return;
    }

    bn *a = p;
    bn_t b, c, d;
    bn_init(b);
    bn_init(c);
    bn_init(d);
    a->num[0] = 0;
    b->num[0] = 1;

    for (unsigned long long h = 1ULL << (63 - __builtin_clzll(k)); h; h >>= 1) {
        bn_cpy(c, b);
        bn_lshift(c, 1);
        bn_sub(c, c, a);
        bn_mult(c, c, a);

        bn_mult(a, a, a);
        bn_mult(b, b, b);
        bn_cpy(d, a);
        bn_add(d, d, b);

        if (h & k) {
            bn_cpy(a, d);
            bn_add(b, c, d);
        } else {
            bn_cpy(a, c);
            bn_cpy(b, d);
        }
    }
    bn_free(b);
    bn_free(c);
    bn_free(d);
}
