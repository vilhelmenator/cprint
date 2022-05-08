#define CTEST_ENABLED
#include "cprint.h"
#include <math.h>
#include <stdalign.h>

int32_t clzll(uint64_t ll)
{
    return ll == 0? 64 : __builtin_clzll(ll);
}

// extended div operation to cache an approximate multiplier
typedef struct div_e_t
{
    uint64_t m;
    int32_t a;
    int32_t s;
} div_e;

// division by a constant utility.
/*
div_e magicu2(uint64_t d)
{
    int64_t p;
    uint64_t p64, q, r, delta;
    div_e magu;
    magu.a = 0;
    p = 63;
    q = 0x7FFFFFFFFFFFFFFF / d;
    r = 0x7FFFFFFFFFFFFFFF - q * d;
    do {
        p = p + 1;
        if (p == 64)
            p64 = 1;
        else
            p64 = 2 * p64;
        if (r + 1 >= d - r) {
            if (q >= 0x7FFFFFFFFFFFFFFF)
                magu.a = 1;
            q = 2 * q + 1; // Update q.
            r = 2 * r + 1 - d; // Update r.
        } else {
            if (q >= 0x8000000000000000)
                magu.a = 1;
            q = 2 * q;
            r = 2 * r + 1;
        }
        delta = d - 1 - r;
    } while (p < 64 && p64 < delta);
    magu.m = q + 1;
    magu.s = p - 64;
    return magu;
}

uint64_t div64(uint64_t num, div_e *di)
{
    uint64_t q = mulhiu64(di->m, num);
    return (di->a ? ((num - q) >> 1) + q : q) >> di->s;
}

static inline uint64_t udiv128by64to64(uint64_t u1, uint64_t u0, uint64_t v,
                                            uint64_t *r) {
#if defined(X86_64)
    uint64_t result;
    __asm__("divq %[v]" : "=a"(result), "=d"(*r) : [v] "r"(den), "a"(numlo), "d"(numhi));
    return result;
#else
  const unsigned n_udword_bits = sizeof(uint64_t) * 8;
  const unsigned n_uword_bits = n_udword_bits >> 1;
  const uint64_t b = (1ULL << (n_udword_bits >> 1)); // Number base (32 bits)
  uint64_t un1, un0;                                // Norm. dividend LSD's
  uint64_t vn1, vn0;                                // Norm. divisor digits
  uint64_t q1, q0;                                  // Quotient digits
  uint64_t un64, un21, un10;                        // Dividend digit pairs
  uint64_t rhat;                                    // A remainder
  int32_t s;                                       

  s = __builtin_clzll(v);
  if (s > 0) {
    // Normalize the divisor.
    v = v << s;
    un64 = (u1 << s) | (u0 >> (n_udword_bits - s));
    un10 = u0 << s; // Shift dividend left
  } else {
    // Avoid undefined behavior of (u0 >> 64).
    un64 = u1;
    un10 = u0;
  }

  // Break divisor up into two 32-bit digits.
  vn1 = v >> (n_udword_bits / 2);
  vn0 = v & 0xFFFFFFFF;

  // Break right half of dividend into two digits.
  un1 = un10 >> (n_udword_bits / 2);
  un0 = un10 & 0xFFFFFFFF;

  // Compute the first quotient digit, q1.
  q1 = un64 / vn1;
  rhat = un64 % vn1;

  // q1 has at most error 2. No more than 2 iterations.
  while (q1 >= b || q1 * vn0 > b * rhat + un1) {
    q1 = q1 - 1;
    rhat = rhat + vn1;
    if (rhat >= b)
      break;
  }

  un21 = un64 * b + un1 - q1 * v;

  // Compute the second quotient digit.
  q0 = un21 / vn1;
  rhat = un21 % vn1;

  // q0 has at most error 2. No more than 2 iterations.
  while (q0 >= b || q0 * vn0 > b * rhat + un0) {
    q0 = q0 - 1;
    rhat = rhat + vn1;
    if (rhat >= b)
      break;
  }

  *r = (un21 * b + un0 - q0 * v) >> s;
  return q1 * b + q0;
#endif
}

static div_e u64_gen(uint64_t d) {

    div_e result;
    uint32_t floor_log_2_d = 63 - __builtin_clzll(d);
    result.s = (uint8_t)floor_log_2_d;
    result.a = 1;
    // Power of 2
    if ((d & (d - 1)) == 0) {
        result.m = 0;
        
    } else {
        uint64_t proposed_m, rem;
        uint64_t divident = (uint64_t)1 << floor_log_2_d;
        proposed_m = udiv128by64to64(divident, 0, d, &rem);
        if (d - rem >= divident) {
            proposed_m += proposed_m;
            const uint64_t twice_rem = rem + rem;
            if (twice_rem >= d || twice_rem < rem) proposed_m += 1;
            result.a = 1;
        }
        result.m = 1 + proposed_m;
    }
    return result;
}
*/
/*
unsigned long long udivdi3(unsigned long long u,
    unsigned long long v)
{
    unsigned long long u0, u1, v1, q0, q1, k, n;
    if (v >> 32 == 0) {
        if (u >> 32 < v)
            return DIVU(u, v)
                & 0xFFFFFFFF;
        else {
            u1 = u >> 32;
            u0 = u & 0xFFFFFFFF; // halves.
            q1 = DIVU(u1, v) // First quotient digit.
                & 0xFFFFFFFF;
            k = u1 - q1 * v; // First remainder, < v.
            q0 = DIVU((k << 32) + u0, v) // 2nd quot. digit. & 0xFFFFFFFF;
                return (q1 << 32)
                + q0;
        }
    }
    n = nlz64(v);           // Here v >= 2**32. // 0<=n<=31.
    v1 = (v << n) >> 32;    // Normalize the divisor
                            // so its MSB is 1.
    u1 = u >> 1;            // To ensure no overflow.
    q1 = DIVU(u1, v1)       // Get quotient from
        & 0xFFFFFFFF;       // divide unsigned insn.
    q0 = (q1 << n) >> 31;   // Undo normalization and
    if (q0 != 0)            // division of u by 2.
        q0 = q0 - 1;        // Make q0 correct or
    if ((u - q0 * v) >= v)  // too small by 1.
        q0 = q0 + 1;        // Now q0 is correct.
    
    return q0;
}
uint64_t divlu(uint64_t a, uint64_t b, uint64_t z)
{
    uint32_t shift = __builtin_clzll(divisor.s.high) - __builtin_clzll(dividend.s.high);
    // aligne devior with dvident.
    divisor.all <<= shift;
    quotient.s.high = 0;
    quotient.s.low = 0;
    // shift and subtract reverse
    for (; shift >= 0; --shift) {
        quotient.s.low <<= 1;
        // subtract dvisor from partial dividend
        // shift down to store the sign bit.
        const ti_int s = (divisor.all - dividend.all - 1) >> (n_utword_bits - 1);
        // store 1? in quotient
        quotient.s.low |= s & 1;
        // subtract if s is set
        dividend.all -= divisor.all & s;
        // shift divisor up.
        divisor.all >>= 1;
    }
    for(int i = 1; i < shift; i++)
    {
        t = (x) >>  (n_utword_bits - 1);
    }
}

#define RECIPROCAL_TABLE_ITEM(d)                                               \
  (unsigned short)(0x7fd00 / (0x100 | (unsigned char)d))

#define REPEAT4(x)                                                             \
  RECIPROCAL_TABLE_ITEM((x) + 0), RECIPROCAL_TABLE_ITEM((x) + 1),              \
      RECIPROCAL_TABLE_ITEM((x) + 2), RECIPROCAL_TABLE_ITEM((x) + 3)

#define REPEAT32(x)                                                            \
  REPEAT4((x) + 4 * 0), REPEAT4((x) + 4 * 1), REPEAT4((x) + 4 * 2),            \
      REPEAT4((x) + 4 * 3), REPEAT4((x) + 4 * 4), REPEAT4((x) + 4 * 5),        \
      REPEAT4((x) + 4 * 6), REPEAT4((x) + 4 * 7)

#define REPEAT256()                                                            \
  REPEAT32(32 * 0), REPEAT32(32 * 1), REPEAT32(32 * 2), REPEAT32(32 * 3),      \
      REPEAT32(32 * 4), REPEAT32(32 * 5), REPEAT32(32 * 6), REPEAT32(32 * 7)

// Reciprocal lookup table of 512 bytes.
static unsigned short kReciprocalTable[] = {REPEAT256()};

// Computes the reciprocal (2^128 - 1) / d - 2^64 for normalized d.
// Based on Algorithm 2 from "Improved division by invariant integers".
static inline du_int reciprocal2by1(du_int d) {

  const du_int d9 = d >> 55;
  const su_int v0 = kReciprocalTable[d9 - 256];

  const du_int d40 = (d >> 24) + 1;
  const du_int v1 = (v0 << 11) - (su_int)(v0 * v0 * d40 >> 40) - 1;

  const du_int v2 = (v1 << 13) + (v1 * (0x1000000000000000 - v1 * d40) >> 47);

  const du_int d0 = d & 1;
  const du_int d63 = (d >> 1) + d0;
  const du_int e = ((v2 >> 1) & (0 - d0)) - v2 * d63;
  const du_int v3 = (((tu_int)(v2)*e) >> 65) + (v2 << 31);

  const du_int v4 = v3 - ((((tu_int)(v3)*d) + d) >> 64) - d;
  return v4;
}

// Based on Algorithm 2 from "Improved division by invariant integers".
static inline du_int udivrem2by1(utwords dividend, du_int divisor,
                                 du_int reciprocal, du_int *remainder) {
  utwords quotient;
  quotient.all = (tu_int)(reciprocal)*dividend.s.high;
  quotient.all += dividend.all;

  ++quotient.s.high;

  *remainder = dividend.s.low - quotient.s.high * divisor;

  if (*remainder > quotient.s.low) {
    --quotient.s.high;
    *remainder += divisor;
  }

  if (*remainder >= divisor) {
    ++quotient.s.high;
    *remainder -= divisor;
  }

  return quotient.s.high;
}

// Effects: if rem != 0, *rem = a % b
// Returns: a / b

tu_int __udivmodti4(tu_int a, tu_int b, tu_int *rem) {
  const unsigned n_utword_bits = sizeof(tu_int) * CHAR_BIT;
  utwords dividend;
  dividend.all = a;
  utwords divisor;
  divisor.all = b;
  utwords quotient;
  utwords remainder;
  if (divisor.all > dividend.all) {
    if (rem)
      *rem = dividend.all;
    return 0;
  }
  // When the divisor fits in 64 bits, we can use an optimized path.
  if (divisor.s.high == 0) {

    const du_int left_shift = __builtin_clzll(divisor.s.low);
    const du_int right_shift = (64 - left_shift) % 64;
    const du_int right_shift_mask = (du_int)(left_shift == 0) - 1;
    utwords normalized_quotient;
    normalized_quotient.s.low =
        (dividend.s.high << left_shift) |
        ((dividend.s.low >> right_shift) & right_shift_mask);
    normalized_quotient.s.high =
        (dividend.s.high >> right_shift) & right_shift_mask;
    const du_int normalized_divisor = divisor.s.low << left_shift;
    const du_int reciprocal = reciprocal2by1(normalized_divisor);
    du_int normalized_remainder;
    quotient.s.high = udivrem2by1(normalized_quotient, normalized_divisor,
                                  reciprocal, &normalized_remainder);
    normalized_quotient.s.high = normalized_remainder;
    normalized_quotient.s.low = dividend.s.low << left_shift;
    remainder.s.high = 0;
    quotient.s.low = udivrem2by1(normalized_quotient, normalized_divisor,
                                 reciprocal, &remainder.s.low);
    remainder.s.low >>= left_shift;
    if (rem)
      *rem = remainder.all;
    return quotient.all;
  }
  // 0 <= shift <= 63.
  si_int shift =
      __builtin_clzll(divisor.s.high) - __builtin_clzll(dividend.s.high);
  divisor.all <<= shift;
  quotient.s.high = 0;
  quotient.s.low = 0;
  for (; shift >= 0; --shift) {
    quotient.s.low <<= 1;

    const ti_int s = (divisor.all - dividend.all - 1) >> (n_utword_bits - 1);
    quotient.s.low |= s & 1;
    dividend.all -= divisor.all & s;

    divisor.all >>= 1;
  }
  if (rem)
    *rem = dividend.all;
  return quotient.all;
}
*/


typedef enum cprint_type_t
{
    UNDEFINED = 0,
    BUFFER      = (int)'b',     // arbitrary buffer of bytes..
    CHARACTER   = (int)'c',     // printable character tokens.
    FLOAT       = (int)'f',     // fractions
    INTEGER     = (int)'i',     // whole numbers
    REFERENCE   = (int)'r',     // reference to another type desc
} cprint_type;

typedef struct var_args_t
{
    char* format;
    void* params;
} var_args;

void* next_param(var_args*a, size_t s)
{
    void* current_addr = a->params;
    a->params += s;
    return current_addr;
}
#define get_param(args, type) next_param(args, sizeof(type))

// recursion utils
const static uint32_t allocation_step = 4096;
static inline uintptr_t align_up64(uintptr_t sz)
{
    const uintptr_t mask = 63;
    return (sz + mask) & ~mask;
}

typedef struct iter_stack_t
{
    uintptr_t end_addr;
    uintptr_t tail_addr;
} iter_stack;

static inline iter_stack *create_iter_stack()
{
    iter_stack *new_stack = (iter_stack *)malloc(allocation_step);
    *new_stack = (iter_stack) { (uintptr_t)new_stack + allocation_step, (uintptr_t)new_stack };
    return new_stack;
}

static inline int is_empty(iter_stack *st)
{
    return st->tail_addr < ((uintptr_t)st + 64);
}

static inline void *top(iter_stack *st)
{
    return (void *)st->tail_addr;
}

static inline void *push(iter_stack **st, size_t item_size)
{
    size_t cache_aligned = align_up64(item_size);
    if (((*st)->tail_addr + cache_aligned) > (*st)->end_addr) {
        size_t tail_delta = (*st)->tail_addr - (uintptr_t)*st;
        size_t next_size = ((*st)->end_addr - (uintptr_t)*st) + allocation_step;
        *st = (iter_stack *)realloc(*st, next_size);
        (*st)->end_addr = (uintptr_t)*st + next_size;
        (*st)->tail_addr = (uintptr_t)*st + tail_delta;
    }
    (*st)->tail_addr += cache_aligned;
    return (void *)(*st)->tail_addr;
}

static inline void *pop(iter_stack *st, size_t item_size)
{
    void *t = top(st);
    size_t cache_aligned = align_up64(item_size);
    st->tail_addr -= cache_aligned;
    return t;
}

var_args* var_stack_push(iter_stack* stack, char* format, void* param)
{
    var_args * item = push(&stack, sizeof(var_args));
    *item = (var_args){format, param};
    return item;
}

var_args* var_stack_pop(iter_stack* stack)
{
    return (var_args*)(is_empty(stack) ? 0 : pop(stack, sizeof(var_args)));
}

var_args* var_stack_top(iter_stack* stack)
{
    return (var_args*)(is_empty(stack) ? 0 : top(stack));
}

int bnprintf(char* buff, size_t size, const char* format, ...) 
{
    return 0;
}

int cnprintf(char* buff, size_t size, const char* format, ...) 
{
    return 0;
}


int cprintf(char* buff, size_t size, var_args* initial_args) 
{
    //
    iter_stack* istack = 0;
    int32_t error_code = 0;
    char* o_buff = buff;
    int8_t ch = 0;
    int8_t val = 0;
    var_args* args = initial_args;
    /*
        a compact alternative to printf.  

        The format buffer has a limit on size.

        A.  Construct a scehma buffer from the format description.
        B.  Are we building a binary buffer or a textual one.
    */
parse:
    ch = *args->format;
    {
        // if we are trying to read beyond our buffer
        if((buff - o_buff) >= size)
        {
            goto error;
        }
        // we are instructed to read a particular type
        if(ch == '%')
        {
            // 
            ch = *++args->format;
            goto tokenize;
        }
        else if(ch == '\0')
        {
            // we are at an end of a string.
            if(istack != 0)
            {
                // we have nested formats
                args = var_stack_pop(istack);
                if(args == 0)
                {
                    goto end;
                }
            }
            else
            {
                goto end;
            }
            // don't put the null terminator at the end
            goto parse; // loop    
        }
        // pluck our character into the bufer and continue
        *buff++ = ch;
        ch = *++args->format;
        goto parse; // loop
    }
tokenize:
    {   
        // what type do we want
        cprint_type ct = ch;
        // is it of a particular size
        // if there is no number, we default to alignment
        // % -- start reading type info
        // %x|{y[z:w]} x : the type to read. {f i c s r b}
        //  - optional y z w:
        //      - y: the size of the element in bytes
        //      - z: the number of elements to reads
        //      - w: the character to insert between each element. ignored when formatting binary.
        //          # controls representation layout.
        //  params n,format,buffer
        //  "%f4[: #.3#e#\n] %f8[2:#] %c2[120:#] %r[2: #\n]",
        //   #.#
        ch = *++args->format;
        if(isnum(ch))
        {
            if(str_to_int8((char*)args->format, &val) != 0)
            {
                goto error;
            }
        }
        switch(ct)
        {
            case BUFFER:
                {
                    void* buffer_ptr = get_param(args, void*);
                    int32_t buffer_size = *(int32_t*)get_param(args, int32_t);
                    buff += format_buffer(buff, buffer_ptr, buffer_size, 1, 1);
                    goto parse;
                }
            case CHARACTER:
                {
                    int8_t c = *(int8_t*)get_param(args, int32_t);
                    buff += format_char(buff, c);
                    goto parse;
                }
            case FLOAT:
                {
                    double d = *(double*)get_param(args, double);
                    buff += format_float64(buff, d);
                    goto parse;
                }
            case INTEGER:
                {
                    int64_t d = *(int64_t*)get_param(args, int64_t);
                    buff += format_int64(buff, d);
                    goto parse;
                }
            case REFERENCE:
                {
                    char* new_format = *(char**)get_param(args, char*);
                    void* new_params = *(void**)get_param(args, void*);
                    if(new_params != 0)
                    {
                        if(!istack)
                        {
                            istack = create_iter_stack();
                            var_stack_push(istack, args->format, args->params);
                        }
                        args = var_stack_push(istack, new_format, new_params);
                    }
                    
                    goto parse;
                }
            default:
                goto error;
        }   
    }
error:
    // print some descriptive error
end:
    if(istack)
    {
        free(istack);
    }
    return buff - o_buff;
}

int main()
{
    const char * hex = "0123456789abcdefABCDEF";
    /*

    [x] string to int
    [x] hex string to int
    [x] cprintf references
    [ ] cprintf buffers
    [ ] cprintf strings
    [ ] cprintf floats
    [ ] cprintf binary out
    [ ] cprintf binary from text
    [ ] perf compare
        - string to type/type to string
        - printf vs cprintf
    
    */
    
    char* sample_floats [] = {
        "",     // invalid
        " ",    // invalid
        "e",    // invalid
        "0.",   // invalid
        "1. ",   // invalid
        " 1.e ",  // invalid
        "0.e",  // invalid
        "e123", // invalid
        "e-",   // invalid
        "e+",   // invalid
        "1.2r", // invalid
        "sasdfasdf", // invalid
        "\0",       // invalid
        "1.e+1",    // valid
        "1e1",
        "123123123123123123123",
        "0.00000000000000000000000000000000000",
        "112312312",
        "1e+2",
        "1e2",
        "1.e",
        "-1,e+1212"
    };

    char* sample_ints [] = {
        "",     // invalid
        " ",    // invalid
        "01",    // invalid
        "-256.",   // invalid
        " 12",   // invalid
        "\0",  // invalid
        "z", // invalid
        "sasdfasdf", // invalid
        "123123123123123123123",
        "000000000000000000000000000000000000",
        "112312312",
        "0x1",
        "0x0",
        "0xff",
        "ffab01"
    };
    int num_sample_floats = sizeof(sample_floats)/sizeof(char*);
    int num_sample_ints = sizeof(sample_ints)/sizeof(char*);
    
    /*
    int result = 0;
    for(int i = 0; i < num_sample_floats; i++)
    {
        double d = 0.0;
        str_to_double(sample_floats[i], &d);
        printf("%g\n", d);
    }
    printf("\n");
    for(int i = 0; i < num_sample_ints; i++)
    {
        int64_t d = 0;
        str_to_int(sample_ints[i], &d, 19);
        printf("%lld\n", d);
    }
    */
    char buffA[1024];
    char buffB[1024];
    int max = 0;
    int count = 0;
    int accum = 0;

    START_TEST(format, {});
    /*
    TEST(format, str_to_double, {
        int result = 0;
        for(int i = 0; i < 14; i++)
        {
            double d = 0.0;
            result |= str_to_double(invalid_floats[i], &d);
        }
        EXPECT(result == 0);
    });
    */
   /*
    struct local_args
    {
        double d;
        double f;
        int64_t a;
        char*r;
        void*ref_p;
    };

    struct rlocal_args
    {
        double d;
        double f;
        double e;
    };

    struct rlocal_args rarguments = {1.2, 2.3, 3.4};
    struct local_args arguments = {2.0, 2.0, 2323, "%f %f %f\n", &rarguments};

    var_args arg = {"%f %f %i %r\n", &arguments};
    int32_t offset = cprintf(buffA, 1024, &arg);
    buffA[offset] = 0;
    printf("%s", buffA);
    float outy = 0.0f;
    str_to_float("0.1e-1", &outy);
    printf("%e\n", outy);
    */
    int64_t d = 0;
    MEASURE_MS(format, str_to_int, {
        for(int i = 0; i < 10000000; i++)
        {   
            int c = format_int(buffA, i);
            buffA[c] = 0;
            str_to_int(buffA, &d, 10);
            __asm__ __volatile__(""); 
        }
    });
    
    char* end;
    MEASURE_MS(format, strtol, {
        for(int i = 0; i < 10000000; i++)
        {
            int c = format_int(buffA, i);
            buffA[c] = 0;
            int64_t d = strtol(buffA, &end, 10);
            __asm__ __volatile__(""); 
        }
    });
    double f = 0.0;
    MEASURE_MS(format, str_to_double, {
        for(int i = 0; i < 10000000; i++)
        {
            str_to_double(sample_floats[i%num_sample_floats], &f);
            // prevent the optimizer from tossing these loops out.
            __asm__ __volatile__(""); 
        }
    });
    
    MEASURE_MS(format, strtod, {
        for(int i = 0; i < 10000000; i++)
        {
            double d = strtod(sample_floats[i%num_sample_floats], &end);
            // prevent the optimizer from tossing these loops out.
            __asm__ __volatile__(""); 
        }
    });
    MEASURE_MS(format, format_float, {
        for(int i = 0; i < 10000000; i++)
        {
            format_float64(buffA, 10000000.0/i);
            // prevent the optimizer from tossing these loops out.
            __asm__ __volatile__(""); 
        }
    });
    MEASURE_MS(format, format_snprintf_float, {
        for(int i = 0; i < 10000000; i++)
        {
            sprintf(buffA, "%f", 10000000.0/i);   
            // prevent the optimizer from tossing these loops out.
            __asm__ __volatile__(""); 
        }
    });
    
    MEASURE_MS(format, format_int, {
        for(int i = 0; i < 10000000; i++)
        {
            format_int(buffA, i);
            // prevent the optimizer from tossing these loops out.
            __asm__ __volatile__(""); 
        }
    });
    
    MEASURE_MS(format, format_snprintf, {
        for(int i = 0; i < 10000000; i++)
        {
            sprintf(buffA, "%d", i);   
            // prevent the optimizer from tossing these loops out.
            __asm__ __volatile__(""); 
        }
    });
    
    END_TEST(format, {});
   
    return 0;
}