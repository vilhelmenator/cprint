#ifndef _CPRINT_H
#define _CPRINT_H
#include "cprint_tables.h"

#include "../ctest/ctest.h"
CLOGGER(_CPRINT_H, 4096)

// Extended floating point struct for extended math ops
typedef struct exfloat_t
{
    uint64_t f;
    int16_t e;
    int16_t s; // sign
} exfloat;

// extended div operation to cache an approximate multiplier
typedef struct div_e_t
{
    uint64_t m;
    int32_t a;
    int32_t s;
} div_e;

static inline int calc_digit_len(uint64_t value)
{
    uint32_t num_digits = 1;
    if (value > 0) {
        uint32_t num_lz = __builtin_clzll(value);
        num_digits = max_digit_num[num_lz];
        if (value < decimal_max[num_digits]) {
            num_digits--;
        }
    }
    return num_digits;
}

uint64_t format_int(char *output, uint64_t value)
{
    const int32_t num_digits = calc_digit_len(value);
    const char *src_ptr = output;
    int8_t *num_str = 0;
    uint64_t new_val = value;
    output += (num_digits);
start:
    switch ((output - src_ptr)) {
    case 0:
        break;
    case 1:
        num_str = (int8_t *)&digits[value * 4] + 3;
        *--output = num_str[0];
        break;
    case 2:
        num_str = (int8_t *)&digits[value * 4] + 2;
        *--output = num_str[1];
        *--output = num_str[0];
        break;
    case 3:
        num_str = (int8_t *)&digits[value * 4] + 1;
        *--output = num_str[2];
        *--output = num_str[1];
        *--output = num_str[0];
        break;
    case 4:
        num_str = (int8_t *)&digits[value * 4];
        output -= 4;
        (*(uint32_t *)output) = *(uint32_t *)num_str;
        break;
    default:
        new_val = divpow10(value, 4);
        num_str = (int8_t *)&digits[(value - new_val * 10000) * 4];
        value = new_val;
        output -= 4;
        (*(uint32_t *)output) = *(uint32_t *)num_str;
        goto start;
    }
    return num_digits;
}

uint64_t format_sint(char *output, int64_t value)
{
    if (value < 0) {
        *--output = '-';
    }
    return format_int(output, llabs(value)) + (value < 0);
}

#define D_1_LOG2_10 0.30102999566398114 // 1/lg(10)
#define D_1_LOG10_2 = 0x4d104d427de7fbcc
typedef enum float_class_t {
    FLTEX_DEFAULT = 0,
    FLTEX_SDEFAULT = 1,
    FLTEX_POS_INF = 2,
    FLTEX_NEG_INF,
    FLTEX_POS_ZERO,
    FLTEX_NEG_ZERO,
    FLTEX_NAN,
} float_class;

static inline exfloat cached_power(int index)
{
    uint64_t *lookup = power10_lookup[index + power10_zero_offset];
    return (exfloat) { lookup[0], lookup[1], 0 };
}

static exfloat double2exfloat(double d)
{
    uint64_t bits = *((uint64_t *)&d);
    switch (bits) {
    case 0x0:
        return (exfloat) { 0, 0, FLTEX_POS_ZERO };
    case 0x8000000000000000:
        return (exfloat) { 0, 0, FLTEX_NEG_ZERO };
    case 0x7FF0000000000000:
        return (exfloat) { 0, FLTEX_POS_INF };
    case 0xFFF0000000000000:
        return (exfloat) { 0, FLTEX_NEG_INF };
    case 0x7FF0000000000001:
    case 0x7FF8000000000001:
    case 0x7FFFFFFFFFFFFFFF:
    case 0xFFF0000000000001:
    case 0xFFF8000000000001:
    case 0xFFFFFFFFFFFFFFFF:
        return (exfloat) { 0, 0, FLTEX_NAN };
    default:
        break;
    }
    const int32_t bits_diff = 64 - 53;
    exfloat fp;
    fp.e = (((int32_t)((bits >> 52) & 0x7ff) - 1023) - 52) - bits_diff;
    fp.f = (0x10000000000000 | (bits & 0xfffffffffffff)) << bits_diff;
    fp.s = d < 0;
    return fp;
}

static exfloat float2exfloat(float d)
{
    uint32_t bits = *((uint32_t *)&d);
    switch (bits) {
    case 0x0:
        return (exfloat) { 0, 0, FLTEX_POS_ZERO };
    case 0x80000000:
        return (exfloat) { 0, 0, FLTEX_NEG_ZERO };
    case 0x7F800000:
        return (exfloat) { 0, 0, FLTEX_POS_INF };
    case 0xFF800000:
        return (exfloat) { 0, 0, FLTEX_NEG_INF };
    case 0xFFC00001:
    case 0xFF800001:
    case 0x7FFFFFFF:
    case 0xFFF00001:
    case 0xFFF80001:
    case 0xFFFFFFFF:
        return (exfloat) { 0, 0, FLTEX_NAN };
    default:
        break;
    }
    const int32_t bits_diff = 64 - 24;
    exfloat fp;
    fp.e = (((int32_t)((bits >> 23) & 0xff) - 127) - 23) - bits_diff;
    fp.f = ((uint64_t)(0x800000 | (bits & 0x7fffff))) << bits_diff;
    fp.s = d < 0;
    return fp;
}

static inline int k_comp(int e, int alpha, int gamma)
{
    return ceil((alpha - e + 63) * D_1_LOG2_10);
}

static exfloat exfloat_construct(uint64_t f, int32_t e, int8_t s)
{
    exfloat fp;

    int32_t shift = __builtin_clzll(1 | f);
    fp.e = power10_lookup[e + power10_zero_offset][1] + 63 - shift;
    fp.f = f << shift;
    fp.s = s;
    return fp;
}

static float exfloat2float(exfloat exf)
{
    uint32_t result = 0;
    if (exf.e > 127) {
        result = 0x7F800000;
    } else if (exf.e < -126) {
        result = 0xFF800000;
    } else {
        switch (exf.s) {
        case 0:
        case 1: {
            const int32_t bits_diff = 64 - 24;
            int32_t e = exf.e;
            uint64_t f = exf.f;
            f >>= (bits_diff);
            e += (bits_diff) + 23 + 127;
            e = e & 0xff;
            e = (e << 23) | (0x7fffff & f);
            if (exf.s) {
                e = e | 0x80000000;
            }
            result = e;
            break;
        }
        case FLTEX_POS_INF:
            result = 0x7F800000;
            break;
        case FLTEX_NEG_INF:
            result = 0xFF800000;
            break;
        case FLTEX_POS_ZERO:
            result = 0x0;
            break;
        case FLTEX_NEG_ZERO:
            result = 0x80000000;
            break;
        default:
            result = 0xFFFFFFFF;
            break;
        }
    }
    return *(float *)(uint32_t *)&result;
}

static double exfloat2double(exfloat exf)
{
    uint64_t result = 0;
    if (exf.e > 1023) {
        result = 0x7FF0000000000000;
    } else if (exf.e < -1022) {
        result = 0x7FF0000000000000;
    }
    switch (exf.s) {
    case 0:
    case 1: {
        const int32_t bits_diff = 64 - 53;
        uint64_t e = exf.e;
        uint64_t f = exf.f;
        f >>= bits_diff;
        e += bits_diff + 52 + 1023;
        e = e & 0x7ff;
        f = e << 52 | (0xfffffffffffff & f);
        if (exf.s) {
            f = f | 0x8000000000000000;
        }
        result = f;
        break;
    }
    case FLTEX_POS_INF:
        result = 0x7FF0000000000000;
        break;
    case FLTEX_NEG_INF:
        result = 0x7FF0000000000000;
        break;
    case FLTEX_POS_ZERO:
        result = 0;
        break;
    case FLTEX_NEG_ZERO:
        result = 0x8000000000000000;
        break;
    default:
        result = 0xFFFFFFFFFFFFFFFF;
        break;
    }
    return *(double *)(uint64_t *)&result;
}

static inline exfloat multiply_exfloat(exfloat *x, exfloat *y)
{
    exfloat r;
    r.f = mulhiu64(y->f, x->f) + 1;
    r.e = x->e + y->e + 64;
    r.s = x->s * y->s;
    return r;
}
#define TEN7 10000000
#define TEN6 1000000
#define TEN5 100000
#define TEN4 10000
#define TEN3 1000
#define TEN2 100
#define TEN1 10

static int cut(exfloat D, uint32_t parts[3])
{
    uint64_t tmp = mulhiu64(power10_divs[7] << D.e, D.f);
    parts[2] = (D.f - tmp * (TEN7 >> D.e)) << D.e;
    parts[0] = mulhiu64(power10_divs[7], tmp);
    parts[1] = tmp - parts[0] * TEN7;
    int num_digits = 14;
    num_digits += calc_digit_len(parts[0]);
    if (parts[1] == 9999999) {
        parts[1] = 0;
        parts[2] = 0;
        parts[0]++;
    }
    return num_digits;
}

static int32_t format_float_grisu(exfloat w, char *buffer)
{
    uint32_t ps[3];
    int q = 64, alpha = 0, gamma = 3;
    int mk = k_comp(w.e + q, alpha, gamma);
    exfloat c_mk = cached_power(mk);
    exfloat D = multiply_exfloat(&w, &c_mk);

    int num_digits = cut(D, ps);
    int e = -mk + num_digits - 1;

    if (w.s) {
        *buffer++ = '-';
    }
    char *buffer_start = buffer;
    // are we outside a nice range?
    if (abs(e) > 7) {
        *buffer++ = '.';
        int cl = format_int(buffer, ps[0]);
        // swap the comma and the first digit.
        char temp = *buffer;
        *buffer = *(buffer - 1);
        *(buffer - 1) = temp;
        // add our sci notation
        buffer += cl;
        *buffer++ = 'e';
        if (e < 0) {
            *buffer++ = '-';
            e = -e;
        }

        buffer += format_int(buffer, e);
    } else {
        // in a nice simple range.
        if (e < 0) {
            // 0.xxxxxxx
            *buffer++ = '0';
            *buffer++ = '.';
            for (int i = 1; i < -e; i++) {
                *buffer++ = '0';
            }
            for (int i = 0; i < 1; i++) {
                buffer += format_int(buffer, ps[i]);
            }
        } else {
            // numbers larger than 10.
            int cl = calc_digit_len(ps[0]);
            int pow10 = cl - (e + 1);
            uint32_t parta = 0;
            uint32_t partb = 0;
            switch (pow10) {
            case 1:
                parta = divpow10(ps[0], 1);
                partb = ps[0] - parta * 10;
                break;
            case 2:
                parta = divpow10(ps[0], 2);
                partb = ps[0] - parta * 100;
                break;
            case 3:
                parta = divpow10(ps[0], 3);
                partb = ps[0] - parta * 1000;
                break;
            case 4:
                parta = divpow10(ps[0], 4);
                partb = ps[0] - parta * 10000;
                break;
            default:
                parta = divpow10(ps[0], 5);
                partb = ps[0] - parta * 100000;
                break;
            }
            buffer += format_int(buffer, parta);
            *buffer++ = '.';
            buffer += format_int(buffer, partb);
        }
    }

    return buffer - buffer_start + w.s;
}

static inline int32_t format_float_ex(exfloat w, char *buffer)
{
    if (w.s >= FLTEX_POS_INF) {
        switch (w.s) {
        case FLTEX_NEG_ZERO:
            *buffer++ = '-';
            *buffer++ = '0';
            return 2;
        case FLTEX_POS_ZERO:
            *buffer++ = '0';
            return 1;
        case FLTEX_NEG_INF:
            *buffer++ = '-';
            *buffer++ = 'I';
            *buffer++ = 'N';
            *buffer++ = 'F';
            return 4;
        case FLTEX_POS_INF:
            *buffer++ = 'I';
            *buffer++ = 'N';
            *buffer++ = 'F';
            return 3;
        default:
            *buffer++ = 'N';
            *buffer++ = 'A';
            *buffer++ = 'N';
            return 3;
        };
    }
    return format_float_grisu(w, buffer);
}

int32_t format_float32(char *buffer, float w)
{
    exfloat fw = float2exfloat(w);
    return format_float_ex(fw, buffer);
}

int32_t format_float64(char *buffer, double w)
{
    exfloat dw = double2exfloat(w);
    return format_float_ex(dw, buffer);
}

static size_t format_data(char *buffer, void *p, uint8_t s, size_t r,
    size_t c)
{
    size_t num_cols = c;
    size_t num_chars = s * c * r * 2; // each bytes needs 2 hex digits
    size_t num_additional_chars = ((c - 1) * r) + r - 1; // space and newlines
    size_t num_digits = num_chars + num_additional_chars;
    const char *hex_digits = "000102030405060708090A0B0C0D0E0F"
                             "101112131415161718191A1B1C1D1E1F"
                             "202122232425262728292A2B2C2D2E2F"
                             "303132333435363738393A3B3C3D3E3F"
                             "404142434445464748494A4B4C4D4E4F"
                             "505152535455565758595A5B5C5D5E5F"
                             "606162636465666768696A6B6C6D6E6F"
                             "707172737475767778797A7B7C7D7E7F"
                             "808182838485868788898A8B8C8D8E8F"
                             "909192939495969798999A9B9C9D9E9F"
                             "A0A1A2A3A4A5A6A7A8A9AAABACADAEAF"
                             "B0B1B2B3B4B5B6B7B8B9BABBBCBDBEBF"
                             "C0C1C2C3C4C5C6C7C8C9CACBCCCDCECF"
                             "D0D1D2D3D4D5D6D7D8D9DADBDCDDDEDF"
                             "E0E1E2E3E4E5E6E7E8E9EAEBECEDEEEF"
                             "F0F1F2F3F4F5F6F7F8F9FAFBFCFDFEFF";
    uint8_t *cptr = (uint8_t *)p;
    uint8_t *charv = 0;
print_hex:
    buffer += (s * 2);
    for (int i = 0; i < s; i++) {
        charv = (uint8_t *)&hex_digits[*cptr++];
        *--buffer = charv[1];
        *--buffer = charv[0];
    }
    // next column
    if (--num_cols > 0) {
        buffer += (s * 2);
        *buffer++ = ' ';
        goto print_hex;
    }
    // next row
    if (--r > 0) {
        num_cols = c;
        buffer += (s * 2);
        *buffer++ = '\n';
        goto print_hex;
    }
    return num_digits;
}

size_t format_ptr(char *buffer, void *p)
{
    return format_data(buffer, p, sizeof(void *), 1, 1);
}

size_t format_buffer(char *buffer, void *p, uint8_t s, size_t r, size_t c)
{
    return format_data(buffer, p, s, r | 1, c | 1);
}

size_t format_str(char *buffer, char *p, size_t l)
{
    if (l < 8) {
        for (int i = 0; i < l; i++) {
            *buffer++ = *(p + i);
        }
    } else {
        size_t word_slizes = l >> 3;
        size_t remainder = l - (word_slizes << 3);
        int32_t i = 0;
        for (i = 0; i < (l - remainder); i += 8) {
            uint64_t chunk = *(uint64_t *)p;
            *(uint64_t *)buffer = chunk;
            buffer += 8;
            p += 8;
        }
        // remaining bytes
        for (; i < l; i++) {
            *buffer++ = *(p + i);
        }
    }
    return l;
}

size_t format_char(char *buffer, char c)
{
    *buffer++ = c;
    return 1;
}

size_t format_int8(char *buffer, int8_t c) { return format_sint(buffer, c); }

size_t format_uint8(char *buffer, uint8_t c) { return format_int(buffer, c); }

size_t format_int16(char *buffer, int16_t c) { return format_sint(buffer, c); }

size_t format_uint16(char *buffer, uint16_t c) { return format_int(buffer, c); }

size_t format_int32(char *buffer, int32_t c) { return format_sint(buffer, c); }

size_t format_uint32(char *buffer, uint32_t c) { return format_int(buffer, c); }

size_t format_int64(char *buffer, int64_t c) { return format_sint(buffer, c); }

size_t format_uint64(char *buffer, uint64_t c) { return format_int(buffer, c); }

static inline int32_t isnum(int8_t ch)
{
    int32_t delta = ch - (int32_t)'0';
    if (delta >= 0 && delta < 10) {
        return 1;
    }
    return 0;
}

static inline int prelude(char **buffer)
{
    // eat up any whitespace
    while (**buffer == ' ') {
        (*buffer)++;
    }
    if (**buffer == '-') {
        (*buffer)++;
        return -1;
    } else if (**buffer == '+') {
        (*buffer)++;
    }
    return 1;
}

static int32_t str_to_int(char *buffer, int64_t *out, size_t max_digits)
{
    int32_t sign = prelude(&buffer);
    int64_t out_val = 0;
    int32_t num_chars = 0;
    char ch = *buffer;
    for (int32_t i = 0; i < max_digits; i++) {
        if (isnum(ch)) {
            int32_t val = ch - (int32_t)'0';
            out_val = (out_val * 10) + val;
            ch = *++buffer;
            num_chars++;

        } else {
            break;
        }
    }

    if (num_chars == 0) {
        return -1;
    }
    out_val *= sign;
    *out = out_val;
    return 0;
}

static int32_t str_to_exfloat(char *buffer, exfloat *out)
{
    int32_t sign = prelude(&buffer);
    int32_t comma_pos = -1;
    char ch = *buffer;
    uint64_t fractional_val = 0;
    int32_t num_chars = 0;
    int32_t num_chars_read = 0;
    int64_t exponent = 0;
    int32_t offset = 0;
    exfloat exval;
    do {
        int32_t val = ch - (int32_t)'0';
        if (val >= 0 && val < 10) {
            if (num_chars < 19) {
                fractional_val = (fractional_val * 10) + val;
                if (fractional_val) {
                    num_chars++;
                }
            } else {
                exponent++;
            }
            num_chars_read++;
        } else {
            if (ch == '\0' || ch == ' ') {
                if (num_chars_read) {
                    if (fractional_val == 0) {
                        *out = (exfloat) { 0, 0, sign < 0 ? FLTEX_NEG_ZERO : FLTEX_POS_ZERO };
                        return 0;
                    }
                    goto gen_float;
                } else {
                    goto err;
                }
            } else if (ch == '.' || ch == ',') {
                if (comma_pos > 0) {
                    goto err;
                }
                comma_pos = num_chars_read;
            } else if (ch == 'e' || ch == 'E') {
                goto read_exponent;
            } else {
                goto err;
            }
        }
        ch = *++buffer;
    } while (1);

    if (fractional_val == 0) {
        *out = (exfloat) { 0, 0, sign < 0 ? FLTEX_NEG_ZERO : FLTEX_POS_ZERO };
        return 0;
    }
read_exponent:
    if (str_to_int(++buffer, (int64_t *)&exponent, 4) != 0) {
        goto err;
    }
    if (exponent > 1023) {
        *out = (exfloat) { 0, 0, sign < 0 ? FLTEX_NEG_INF : FLTEX_POS_INF };
        return 0;
    }
gen_float:
    if (comma_pos >= 0) {
        exponent -= (num_chars_read - comma_pos);
    }
    exval = exfloat_construct(fractional_val, 0, sign == -1);
    exfloat c_mk = cached_power(exponent);
    exfloat D = multiply_exfloat(&exval, &c_mk);

    // normalize the results.
    int32_t shift = __builtin_clzll(D.f);
    D.f <<= shift;
    D.e -= shift;

    *out = D;
    return 0;
err:
    return -1;
}

static int32_t str_to_float(char *buffer, float *out)
{
    exfloat exval;
    if (str_to_exfloat(buffer, &exval) == 0) {
        *out = exfloat2float(exval);
        return 0;
    }
    return -1;
}

static int32_t str_to_double(char *buffer, double *out)
{
    exfloat exval;
    if (str_to_exfloat(buffer, &exval) == 0) {
        *out = exfloat2double(exval);
        return 0;
    }
    return -1;
}

int32_t str_to_int8(char *buffer, int8_t *out)
{
    return str_to_int(buffer, (int64_t *)out, max_digit_num[56]);
}

#endif //_CPRINT_H