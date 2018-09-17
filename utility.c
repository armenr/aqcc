#include "aqcc.h"

_Noreturn void error(const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    char *str = vformat(msg, args);
    va_end(args);

    // fprintf(stderr, "[ERROR] %s\n", str);
    printf("[ERROR] %s\n", str);
    // fprintf(stderr, "[DEBUG] %s, %d\n", __FILE__, __LINE__);
    exit(EXIT_FAILURE);
}

_Noreturn void error_unexpected_token_kind(int expect_kind, Token *got)
{
    error(":%d:%d: unexpected token: expect %s, got %s", got->line, got->column,
          token_kind2str(expect_kind), token_kind2str(got->kind));
}

_Noreturn void error_unexpected_token_str(char *expect_str, Token *got)
{
    error(":%d:%d: unexpected token: expect %s, got %s", got->line, got->column,
          expect_str, token_kind2str(got->kind));
}

void warn(const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    char *str = vformat(msg, args);
    va_end(args);

    // fprintf(stderr, "[WARN]  %s\n", str);
    printf("[WARN]  %s\n", str);
    // fprintf(stderr, "[DEBUG] %s, %d\n", __FILE__, __LINE__);
}

void *safe_malloc(int size)
{
    void *ptr;

    ptr = malloc(size);
    if (ptr == NULL) error("malloc failed.");
    return ptr;
}

char *new_str(const char *src)
{
    char *ret = safe_malloc(strlen(src) + 1);
    strcpy(ret, src);
    return ret;
}

int *new_int(int src)
{
    int *ret = safe_malloc(sizeof(int));
    *ret = src;
    return ret;
}

char *vformat(const char *src, va_list ap)
{
    char buf[512];  // TODO: enough length?
    vsprintf(buf, src, ap);

    char *ret = safe_malloc(strlen(buf) + 1);
    strcpy(ret, buf);
    return ret;
}

char *format(const char *src, ...)
{
    va_list args;
    va_start(args, src);
    char *ret = vformat(src, args);
    va_end(args);
    return ret;
}

int unescape_char(int src)
{
    static int table[128];
    if (table[0] == 0) {
        memset(table, 255, sizeof(table));

        table['n'] = '\n';
        table['r'] = '\r';
        table['t'] = '\t';
        table['0'] = '\0';
        table['a'] = '\a';
        table['b'] = '\b';
        table['v'] = '\v';
        table['f'] = '\f';
    }

    int ch = table[src];
    return ch == -1 ? src : ch;
}

char *escape_string(char *str, int size)
{
    StringBuilder *sb = new_string_builder();
    for (int i = 0; i < size; i++) {
        char ch = str[i];

        switch (ch) {
            case '\n':
                string_builder_append(sb, '\\');
                string_builder_append(sb, 'n');
                break;

            case '\r':
                string_builder_append(sb, '\\');
                string_builder_append(sb, 'n');
                break;

            case '\t':
                string_builder_append(sb, '\\');
                string_builder_append(sb, 't');
                break;

            case '\0':
                string_builder_append(sb, '\\');
                string_builder_append(sb, '0');
                break;

            case '\a':
                string_builder_append(sb, '\\');
                string_builder_append(sb, 'a');
                break;

            case '\b':
                string_builder_append(sb, '\\');
                string_builder_append(sb, 'b');
                break;

            case '\v':
                string_builder_append(sb, '\\');
                string_builder_append(sb, 'v');
                break;

            case '\f':
                string_builder_append(sb, '\\');
                string_builder_append(sb, 'f');
                break;

            case '"':
                string_builder_append(sb, '\\');
                string_builder_append(sb, '"');
                break;

            default:
                string_builder_append(sb, ch);
                break;
        }
    }

    return string_builder_get(sb);
}

char *make_label_string()
{
    static int count;
    return format(".L%d", count++);
}

int alignment_of(Type *type)
{
    switch (type->kind) {
        case TY_INT:
            return 4;
        case TY_CHAR:
            return 1;
        case TY_PTR:
            return 8;
        case TY_ARY:
            return alignment_of(type->ary_of);
        case TY_UNION:
        case TY_STRUCT: {
            int alignment = 0;
            for (int i = 0; i < vector_size(type->members); i++) {
                StructMember *member =
                    (StructMember *)vector_get(type->members, i);
                alignment = max(alignment, alignment_of(member->type));
            }
            return alignment;
        }
    }
    assert(0);
}

int min(int a, int b) { return a < b ? a : b; }

int max(int a, int b) { return a < b ? b : a; }

int roundup(int n, int b) { return (n + b - 1) & ~(b - 1); }

char *read_entire_file(char *filepath)
{
    FILE *fh = fopen(filepath, "r");
    if (fh == NULL) error("no such file: '%s'", filepath);

    // read the file all
    StringBuilder *sb = new_string_builder();
    int ch;
    while ((ch = fgetc(fh)) != EOF) string_builder_append(sb, ch);
    return string_builder_get(sb);

    fclose(fh);
}

void erase_backslash_newline(char *src)
{
    char *r = src, *w = src;
    while (*r != '\0') {
        if (*r == '\\' && *(r + 1) == '\n')
            r += 2;
        else
            *w++ = *r++;
    }
    *w = '\0';
}

Vector *read_tokens_from_filepath(char *filepath)
{
    char *src = read_entire_file(filepath);
    return read_all_tokens(src, filepath);
}

Vector *read_asm_from_filepath(char *filepath)
{
    char *src = read_entire_file(filepath);
    return read_all_asm(src, filepath);
}

int is_register_code(Code *code)
{
    if (code == NULL) return 0;
    return !(code->kind & INST_) &&
           code->kind & (REG_8 | REG_16 | REG_32 | REG_64);
}

int reg_of_nbyte(int nbyte, int reg)
{
    switch (nbyte) {
        case 1:
            return (reg & 31) | REG_8;
        case 2:
            return (reg & 31) | REG_16;
        case 4:
            return (reg & 31) | REG_32;
        case 8:
            return (reg & 31) | REG_64;
    }
    assert(0);
}

Code *nbyte_reg(int nbyte, int reg)
{
    return new_code(reg_of_nbyte(nbyte, reg));
}

Code *str2reg(char *src)
{
    Map *map = new_map();

    map_insert(map, "%al", nbyte_reg(1, 0));
    map_insert(map, "%dil", nbyte_reg(1, 1));
    map_insert(map, "%sil", nbyte_reg(1, 2));
    map_insert(map, "%dl", nbyte_reg(1, 3));
    map_insert(map, "%cl", nbyte_reg(1, 4));
    map_insert(map, "%r8b", nbyte_reg(1, 5));
    map_insert(map, "%r9b", nbyte_reg(1, 6));
    map_insert(map, "%r10b", nbyte_reg(1, 7));
    map_insert(map, "%r11b", nbyte_reg(1, 8));
    map_insert(map, "%r12b", nbyte_reg(1, 9));
    map_insert(map, "%r13b", nbyte_reg(1, 10));
    map_insert(map, "%r14b", nbyte_reg(1, 11));
    map_insert(map, "%r15b", nbyte_reg(1, 12));

    map_insert(map, "%ax", nbyte_reg(2, 0));
    map_insert(map, "%di", nbyte_reg(2, 1));
    map_insert(map, "%si", nbyte_reg(2, 2));
    map_insert(map, "%dx", nbyte_reg(2, 3));
    map_insert(map, "%cx", nbyte_reg(2, 4));
    map_insert(map, "%r8w", nbyte_reg(2, 5));
    map_insert(map, "%r9w", nbyte_reg(2, 6));
    map_insert(map, "%r10w", nbyte_reg(2, 7));
    map_insert(map, "%r11w", nbyte_reg(2, 8));
    map_insert(map, "%r12w", nbyte_reg(2, 9));
    map_insert(map, "%r13w", nbyte_reg(2, 10));
    map_insert(map, "%r14w", nbyte_reg(2, 11));
    map_insert(map, "%r15w", nbyte_reg(2, 12));

    map_insert(map, "%eax", nbyte_reg(4, 0));
    map_insert(map, "%edi", nbyte_reg(4, 1));
    map_insert(map, "%esi", nbyte_reg(4, 2));
    map_insert(map, "%edx", nbyte_reg(4, 3));
    map_insert(map, "%ecx", nbyte_reg(4, 4));
    map_insert(map, "%r8d", nbyte_reg(4, 5));
    map_insert(map, "%r9d", nbyte_reg(4, 6));
    map_insert(map, "%r10d", nbyte_reg(4, 7));
    map_insert(map, "%r11d", nbyte_reg(4, 8));
    map_insert(map, "%r12d", nbyte_reg(4, 9));
    map_insert(map, "%r13d", nbyte_reg(4, 10));
    map_insert(map, "%r14d", nbyte_reg(4, 11));
    map_insert(map, "%r15d", nbyte_reg(4, 12));

    map_insert(map, "%rax", nbyte_reg(8, 0));
    map_insert(map, "%rdi", nbyte_reg(8, 1));
    map_insert(map, "%rsi", nbyte_reg(8, 2));
    map_insert(map, "%rdx", nbyte_reg(8, 3));
    map_insert(map, "%rcx", nbyte_reg(8, 4));
    map_insert(map, "%r8", nbyte_reg(8, 5));
    map_insert(map, "%r9", nbyte_reg(8, 6));
    map_insert(map, "%r10", nbyte_reg(8, 7));
    map_insert(map, "%r11", nbyte_reg(8, 8));
    map_insert(map, "%r12", nbyte_reg(8, 9));
    map_insert(map, "%r13", nbyte_reg(8, 10));
    map_insert(map, "%r14", nbyte_reg(8, 11));
    map_insert(map, "%r15", nbyte_reg(8, 12));

    map_insert(map, "%rip", RIP());
    map_insert(map, "%rbp", RBP());
    map_insert(map, "%rsp", RSP());

    KeyValue *kv = map_lookup(map, src);
    assert(kv != NULL);
    return kv_value(kv);
}
