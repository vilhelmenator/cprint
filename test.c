#define CTEST_ENABLED
#include "cprint.h"
#include <math.h>




int file_read_line(FILE *fd, char *buff, size_t s)
{
    return getline(&buff, &s, fd) != -1;
}

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

int cprintf(char* buff, size_t size, var_args* args) 
{
    //
    iter_stack* istack = 0;
    int32_t error_code = 0;
    char* o_buff = buff;
    int8_t ch = 0;
    /*
        a compact alternative to printf.  

        The format buffer has a limit on size.

        A.  Construct a scehma buffer from the format description.
        B.  Are we building a binary buffer or a textual one.
    */
parse:
    ch = *args->format;
    {
        if((buff - o_buff) >= size)
        {
            goto error;
        }

        if(ch == '%')
        {
            ch = *++args->format;
            goto tokenize;
        }
        else if(ch == '\0')
        {
            if(istack != 0)
            {
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
            
        }
        *buff++ = ch;
        ch = *++args->format;
        goto parse; // loop
    }
tokenize:
    {
        cprint_type ct = UNDEFINED;
        switch(ch)
        {
            case 'b':
            case 'c':
            case 'f':
            case 'i':
            case 'r':
                ct = (cprint_type)ch;
                ch = *++args->format;
                break;
            default:
                goto error;
        }

        int8_t val = 0;
        if(abs((int32_t)ch - (int32_t)'0') < 10)
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
                    break;
                }
            case CHARACTER:
                {
                    int8_t c = *(int8_t*)get_param(args, int32_t);
                    buff += format_char(buff, c);
                    break;
                }
            case FLOAT:
                {
                    double d = *(double*)get_param(args, double);
                    buff += format_float64(buff, d);
                    break;
                }
            case INTEGER:
                {
                    int64_t d = *(int64_t*)get_param(args, int64_t);
                    buff += format_int64(buff, d);
                    break;
                }
            case REFERENCE:
                {
                    char* new_format = (char*)get_param(args, char*);
                    void* new_params = get_param(args, void*);
                    // 2 va args
                    if(!istack)
                    {
                        istack = create_iter_stack();
                    }
                    var_stack_push(istack, args->format, args->params);
                    break;
                }
            default:
                break;
        }
        // parse the va arg
        goto parse;
    }
error:

end:
    if(istack)
    {
        free(istack);
    }
    return buff - o_buff;
}

int main()
{
    printf("%d\n", 0xfffffb4d);
    char buffA[1024];
    char buffB[1024];
    int max = 0;
    int count = 0;
    int accum = 0;
    START_TEST(stream, {});
    struct local_args
    {
        double d;
        double f;
        int a;
    };
    struct local_args arguments = {2.0, 2.0, 2323};
    var_args arg = {"%f %f %i\n", &arguments};
    int32_t offset = cprintf(buffA, 1024, &arg);
    buffA[offset] = 0;
    printf("%s", buffA);
    float outy = 0.0f;
    str_to_float("0.1e-1", &outy);
    printf("%e\n", outy);
    int64_t d = 0;
    MEASURE_MS(stream, str_to_int, {
        for(int i = 0; i < 10000000; i++)
        {   
            int c = format_int(buffA, i);
            buffA[c] = 0;
            str_to_int(buffA, &d, 10);
            accum++;
        }
    });
    
    char* end;
    MEASURE_MS(stream, strtol, {
        for(int i = 0; i < 10000000; i++)
        {
            int c = format_int(buffA, i);
            buffA[c] = 0;
            int64_t d = strtol(buffA, &end, 10);
        }
    });
    MEASURE_MS(stream, str_to_flt, {
        for(int i = 0; i < 10000000; i++)
        {
            double d = 0.0;
            str_to_double("0.0101e-13", &d);
            accum++;
        }
    });
    
    MEASURE_MS(stream, strtod, {
        for(int i = 0; i < 10000000; i++)
        {
            double d = strtod("0.0101e-13", &end);
            accum++;
        }
    });
    MEASURE_MS(stream, format_float_, {
        for(int i = 0; i < 10000000; i++)
        {
            format_float64(buffA, 10000000.0/i);
            accum++;
        }
    });
    MEASURE_MS(stream, format_snprintf_float_, {
        for(int i = 0; i < 10000000; i++)
        {
            sprintf(buffA, "%f", 10000000.0/i);   
            accum++;
        }
    });
    
    MEASURE_MS(stream, format_int_4_, {
        for(int i = 0; i < 10000000; i++)
        {
            count = format_int(buffA, i);
            accum++;
        }
    });

    MEASURE_MS(stream, format_snprintf, {
        for(int i = 0; i < 10000000; i++)
        {
            sprintf(buffA, "%d", i);   
        }
    });
    
    
    END_TEST(stream, {});
   
    return 0;
}