#define CTEST_ENABLED
#include "../ctest/ctest.h"

#include "cprint.h"
#include <math.h>
#include <stdalign.h>
#include <stdlib.h>
#include "../allocator/libinclude.h"

unsigned long strlen(const char *s)
{
    char *p;
    for (p = (char *)s; *p; p++);
    return p - s;
}

/* This counts the number of args */
#define NARGS_SEQ(                                                             \
    _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16,     \
    _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, \
    _32, _33, _34, _35, _36, _37, _38, _39, _40, _41, _42, _43, _44, _45, _46, \
    _47, _48, _49, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _60, _61, \
    _62, _63, _64, _65, _66, _67, _68, _69, _70, _71, _72, _73, _74, _75, _76, \
    _77, _78, _79, _80, _81, _82, _83, _84, _85, _86, _87, _88, _89, _90, _91, \
    _92, _93, _94, _95, _96, _97, _98, _99, _100, _101, _102, _103, _104,      \
    _105, _106, _107, _108, _109, _110, _111, _112, _113, _114, _115, _116,    \
    _117, _118, _119, _120, _121, _122, _123, _124, _125, _126, _127, _128, N, \
    ...)                                                                       \
    N
#define NARGS(...)                                                             \
    NARGS_SEQ(nill, ##__VA_ARGS__, 127, 126, 125, 124, 123, 122, 121, 120,     \
              119, 118, 117, 116, 115, 114, 113, 112, 111, 110, 109, 108, 107, \
              106, 105, 104, 103, 102, 101, 100, 99, 98, 97, 96, 95, 94, 93,   \
              92, 91, 90, 89, 88, 87, 86, 85, 84, 83, 82, 81, 80, 79, 78, 77,  \
              76, 75, 74, 73, 72, 71, 70, 69, 68, 67, 66, 65, 64, 63, 62, 61,  \
              60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45,  \
              44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29,  \
              28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13,  \
              12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

typedef struct var_args_t
{
    char *format;
    size_t num_args;
    void *params;
} var_args;

var_args *_construct_var_args(char *format, size_t s, ...)
{
    var_args *res = (var_args *)cmalloc(sizeof(var_args) + (s * sizeof(void *)));

    res->format = format;
    res->num_args = s;
    res->params = &res->params + sizeof(void *);
    void **params = (void **)res->params;
    va_list ap;
    va_start(ap, s);
    for (int i = 0; i < s; i++) {
        params[i] = va_arg(ap, void*);
    }

    va_end(ap);
    return res;
}

#define construct_var_args(format, ...)                                        \
    _construct_var_args(format, NARGS(__VA_ARGS__), __VA_ARGS__)

void *next_param(var_args *a, size_t s)
{
    void *current_addr = a->params;
    a->params += s;
    return current_addr;
}

#define get_param(args, type) next_param(args, sizeof(type))

typedef enum cprint_type_t {
    UNDEFINED = 0,
    BUFFER = (int)'b',    // arbitrary buffer of bytes..
    CHARACTER = (int)'c', // printable character tokens.
    FLOAT = (int)'f',     // fractions
    INTEGER = (int)'i',   // whole numbers
    REFERENCE = (int)'r', // reference to another type desc
    STRING = (int)'s',    // string of characters
} cprint_type;

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
    iter_stack *new_stack = (iter_stack *)cmalloc(allocation_step);
    *new_stack = (iter_stack){(uintptr_t)new_stack + allocation_step,
                              (uintptr_t)new_stack};
    return new_stack;
}

static inline int is_empty(iter_stack *st)
{
    return st->tail_addr < ((uintptr_t)st + 64);
}

static inline void *top(iter_stack *st) { return (void *)st->tail_addr; }

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

var_args *var_stack_push(iter_stack *stack, char *format, void *param)
{
    var_args *item = push(&stack, sizeof(var_args));
    *item = (var_args){format, 0, param};
    return item;
}

var_args *var_stack_pop(iter_stack *stack)
{
    return (var_args *)(is_empty(stack) ? 0 : pop(stack, sizeof(var_args)));
}

var_args *var_stack_top(iter_stack *stack)
{
    return (var_args *)(is_empty(stack) ? 0 : top(stack));
}

int bnprintf(char *buff, size_t size, const char *format, ...) { return 0; }

int cnprintf(char *buff, size_t size, const char *format, ...) { return 0; }

int var_args_test(char *buff, ...)
{
    int nargs = NARGS(...);
    printf("%d\n", nargs);
    return 0;
}

int cprintf(char *buff, size_t size, var_args *initial_args)
{
    //
    iter_stack *istack = 0;
    int32_t error_code = 0;
    char *o_buff = buff;
    int8_t ch = 0;
    int8_t val = 0;
    var_args *args = initial_args;
    /*
        a compact alternative to printf.

        The format buffer has a limit on size.

        A.  Construct a schema buffer from the format description.
        B.  Are we building a binary buffer or a textual one.
    */
parse:
    ch = *args->format;
    {
        // if we are trying to read beyond our buffer
        if ((buff - o_buff) >= size) {
            goto error;
        }
        // we are instructed to read a particular type
        if (ch == '%') {
            //
            ch = *++args->format;
            goto tokenize;
        } else if (ch == '\0') {
            // we are at an end of a string.
            if (istack != 0) {
                // we have nested formats
                args = var_stack_pop(istack);
                if (args == 0) {
                    goto end;
                }
            } else {
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
tokenize : {
    // what type do we want
    cprint_type ct = ch;
    // is it of a particular size
    // if there is no number, we default to alignment
    // % -- start reading type info
    // %x|{y[z:w]} x : the type to read. {f i c s r b}
    //  - optional y z w:
    //      - y: the size of the element in bytes
    //      - z: the number of elements to reads
    //      - w: the character to insert between each element. ignored when
    //      formatting binary.
    //          # controls representation layout.
    //  params n,format,buffer
    //  "%f4[: #.3#e#\n] %f8[2:#] %c2[120:#] %r[2: #\n]",
    //   #.#
    int width = 0, precision = -1;

    // Parse width (digits before '.' or type)
    ch = *++args->format;
    while (ch >= '0' && ch <= '9') {
        width = width * 10 + (ch - '0');
        ch = *++args->format;
    }

    // Parse precision (if present)
    if (ch == '.') {
        precision = 0;
        ch = *++args->format;
        while (ch >= '0' && ch <= '9') {
            precision = precision * 10 + (ch - '0');
            ch = *++args->format;
        }
    }
    ct = ch;
    ch = *++args->format;
    
    switch (ct) {
    case BUFFER: {
        void *buffer_ptr = get_param(args, void *);
        int32_t buffer_size = *(int32_t *)get_param(args, int32_t);
        buff += format_buffer(buff, buffer_ptr, buffer_size, 1, 1);
        goto parse;
    }
    case CHARACTER: {
        int8_t c = *(int8_t *)get_param(args, int32_t);
        buff += format_char(buff, c);
        goto parse;
    }
    case FLOAT: {
        double d = *(double *)get_param(args, double);
        buff += format_float64_width(buff, d, width, precision);
        goto parse;
    }
    case INTEGER: {
        int64_t d = *(int64_t *)get_param(args, int64_t);
        buff += format_int64_width(buff, d, width);
        goto parse;
    }
    case REFERENCE: {
        char *new_format = *(char **)get_param(args, char *);
        void *new_params = *(void **)get_param(args, void *);
        if (new_params != 0) {
            if (!istack) {
                istack = create_iter_stack();
                var_stack_push(istack, args->format, args->params);
            }
            args = var_stack_push(istack, new_format, new_params);
        }

        goto parse;
    }
    case STRING: {
        char *str = *(char **)get_param(args, char *);
        size_t len = strlen(str);
        buff += format_str_width(buff, str, len, width);
        goto parse;
    }
    default:
        goto error;
    }
}
error:
    // print some descriptive error
end:
    if (istack) {
        cfree(istack);
    }
    return buff - o_buff;
}

int main()
{
    const char *hex = "0123456789abcdefABCDEF";
    // var_args *va = construct_var_args("%d", 1, 2, 3, 4);
    // printf("%zu\n", va->num_args);
    // return 0;
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

    char *sample_floats[] = {"",          // invalid
                             " ",         // invalid
                             "e",         // invalid
                             "0.",        // invalid
                             "1. ",       // invalid
                             " 1.e ",     // invalid
                             "0.e",       // invalid
                             "e123",      // invalid
                             "e-",        // invalid
                             "e+",        // invalid
                             "1.2r",      // invalid
                             "sasdfasdf", // invalid
                             "\0",        // invalid
                             "1.e+1",     // valid
                             "1e1",
                             "123123123123123123123",
                             "0.00000000000000000000000000000000000",
                             "112312312",
                             "1e+2",
                             "1e2",
                             "1.e",
                             "-1,e+1212"};

    char *sample_ints[] = {"",          // invalid
                           " ",         // invalid
                           "01",        // invalid
                           "-256.",     // invalid
                           " 12",       // invalid
                           "\0",        // invalid
                           "z",         // invalid
                           "sasdfasdf", // invalid
                           "123123123123123123123",
                           "000000000000000000000000000000000000",
                           "112312312",
                           "0x1",
                           "0x0",
                           "0xff",
                           "ffab01"};
    int num_sample_floats = sizeof(sample_floats) / sizeof(char *);
    int num_sample_ints = sizeof(sample_ints) / sizeof(char *);

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
    int c = format_str(buffA, (char *)hex, 24);
    buffA[c] = 0;
    printf("%s\n", buffA);
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
    var_args arg = {"%f %f %i %r\n", 4, &arguments};
    int32_t offset = cprintf(buffA, 1024, &arg);

    printf("%s", buffA);

    int64_t d = 0;
    MEASURE_TIME(format, str_to_int, {
        for (int i = 0; i < 10000000; i++) {
            int c = format_int(buffA, i);
            buffA[c] = 0;
            str_to_int(buffA, &d, 10);
            __asm__ __volatile__("");
        }
    });

    char *end;
    MEASURE_TIME(format, strtol, {
        for (int i = 0; i < 10000000; i++) {
            int c = format_int(buffA, i);
            buffA[c] = 0;
            int64_t d = strtol(buffA, &end, 10);
            __asm__ __volatile__("");
        }
    });
    double f = 0.0;
    MEASURE_TIME(format, str_to_double, {
        for (int i = 0; i < 10000000; i++) {
            str_to_double(sample_floats[i % num_sample_floats], &f);
            // prevent the optimizer from tossing these loops out.
            __asm__ __volatile__("");
        }
    });

    MEASURE_TIME(format, strtod, {
        for (int i = 0; i < 10000000; i++) {
            double d = strtod(sample_floats[i % num_sample_floats], &end);
            // prevent the optimizer from tossing these loops out.
            __asm__ __volatile__("");
        }
    });
    MEASURE_TIME(format, format_float, {
        for (int i = 0; i < 10000000; i++) {
            format_float64(buffA, 10000000.0 / i);
            // prevent the optimizer from tossing these loops out.
            __asm__ __volatile__("");
        }
    });
    MEASURE_TIME(format, format_snprintf_float, {
        for (int i = 0; i < 10000000; i++) {
            sprintf(buffA, "%f", 10000000.0 / i);
            // prevent the optimizer from tossing these loops out.
            __asm__ __volatile__("");
        }
    });

    MEASURE_TIME(format, format_int, {
        for (int i = 0; i < 10000000; i++) {
            format_int(buffA, i);
            // prevent the optimizer from tossing these loops out.
            __asm__ __volatile__("");
        }
    });

    MEASURE_TIME(format, format_snprintf_int, {
        for (int i = 0; i < 10000000; i++) {
            sprintf(buffA, "%d", i);
            // prevent the optimizer from tossing these loops out.
            __asm__ __volatile__("");
        }
    });

    MEASURE_TIME(format, format_str, {
        for (int i = 0; i < 10000000; i++) {
            format_str(buffA, (char *)hex, strlen(hex));
            // prevent the optimizer from tossing these loops out.
            __asm__ __volatile__("");
        }
    });
    MEASURE_TIME(format, format_snprintf_str, {
        for (int i = 0; i < 10000000; i++) {
            sprintf(buffA, "%s", hex);
            // prevent the optimizer from tossing these loops out.
            __asm__ __volatile__("");
        }
    });
    END_TEST(format, {});

    return 0;
}