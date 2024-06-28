//
//  regexp.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//

#include "ecc.h"

#define DUMP_REGEXP 0

enum eccrxopcode_t
{
    ECC_RXOP_OVER = 0,
    ECC_RXOP_NLOOKAHEAD = 1,
    ECC_RXOP_LOOKAHEAD = 2,
    ECC_RXOP_START,
    ECC_RXOP_END,
    ECC_RXOP_LINESTART,
    ECC_RXOP_LINEEND,
    ECC_RXOP_BOUNDARY,

    ECC_RXOP_SPLIT,
    ECC_RXOP_REFERENCE,
    ECC_RXOP_REDO,
    ECC_RXOP_SAVE,
    ECC_RXOP_NSAVE,
    ECC_RXOP_ANY,
    ECC_RXOP_ONEOF,
    ECC_RXOP_NEITHEROF,
    ECC_RXOP_INRANGE,
    ECC_RXOP_INRANGECASE,
    ECC_RXOP_DIGIT,
    ECC_RXOP_SPACE,
    ECC_RXOP_WORD,
    ECC_RXOP_BYTES,
    ECC_RXOP_JUMP,
    ECC_RXOP_MATCH,
};

typedef enum eccrxopcode_t eccrxopcode_t;
typedef struct eccrxparser_t eccrxparser_t;

struct eccregexnode_t
{
    char* bytes;
    int16_t offset;
    uint8_t opcode;
    uint8_t depth;
};

struct eccrxparser_t
{
    const char* c;
    const char* end;
    uint16_t count;
    int ignoreCase;
    int multiline;
    int disallowQuantifier;
};

static void regextypefn_mark(eccobject_t *object);
static void regextypefn_capture(eccobject_t *object);
static void regextypefn_finalize(eccobject_t *object);
static eccregexnode_t *ecc_regexp_node(eccrxopcode_t opcode, long offset, const char *bytes);
static void ecc_regexp_toss(eccregexnode_t *node);
static uint16_t ecc_regexp_nlen(eccregexnode_t *n);
static eccregexnode_t *ecc_regexp_join(eccregexnode_t *a, eccregexnode_t *b);
static int ecc_regexp_accept(eccrxparser_t *p, char c);
static eccrxopcode_t ecc_regexp_escape(eccrxparser_t *p, int16_t *offset, char buffer[5]);
static eccrxopcode_t ecc_regexp_character(ecctextstring_t text, int16_t *offset, char buffer[12], int ignoreCase);
static eccregexnode_t *ecc_regexp_characternode(ecctextstring_t text, int ignoreCase);
static eccregexnode_t *ecc_regexp_term(eccrxparser_t *p, eccobjerror_t **error);
static eccregexnode_t *ecc_regexp_alternative(eccrxparser_t *p, eccobjerror_t **error);
static eccregexnode_t *ecc_regexp_disjunction(eccrxparser_t *p, eccobjerror_t **error);
static eccregexnode_t *ecc_regexp_pattern(eccrxparser_t *p, eccobjerror_t **error);
static void ecc_regexp_clear(eccrxstate_t *const s, const char *c, uint8_t *bytes);
static int ecc_regexp_forkmatch(eccrxstate_t *const s, eccregexnode_t *n, ecctextstring_t text, int16_t offset);
static int ecc_regexp_match(eccrxstate_t *const s, eccregexnode_t *n, ecctextstring_t text);
static eccvalue_t objregexpfn_constructor(ecccontext_t *context);
static eccvalue_t objregexpfn_toString(ecccontext_t *context);
static eccvalue_t objregexpfn_exec(ecccontext_t *context);
static eccvalue_t objregexpfn_test(ecccontext_t *context);

eccobject_t* ECC_Prototype_Regexp = NULL;
eccobjfunction_t* ECC_CtorFunc_Regexp = NULL;

const eccobjinterntype_t ECC_Type_Regexp = {
    .text = &ECC_ConstString_RegexpType,
    .mark = regextypefn_mark,
    .capture = regextypefn_capture,
    .finalize = regextypefn_finalize,
};

static void regextypefn_mark(eccobject_t* object)
{
    eccobjregexp_t* self = (eccobjregexp_t*)object;

    ecc_mempool_markvalue(ecc_value_fromchars(self->pattern));
    ecc_mempool_markvalue(ecc_value_fromchars(self->source));
}

static void regextypefn_capture(eccobject_t* object)
{
    eccobjregexp_t* self = (eccobjregexp_t*)object;
    ++self->pattern->refcount;
    ++self->source->refcount;
}

static void regextypefn_finalize(eccobject_t* object)
{
    eccobjregexp_t* self = (eccobjregexp_t*)object;
    eccregexnode_t* n = self->program;
    while(n->opcode != ECC_RXOP_OVER)
    {
        if(n->bytes)
            free(n->bytes), n->bytes = NULL;

        ++n;
    }
    free(self->program), self->program = NULL;
    --self->pattern->refcount;
    --self->source->refcount;
}

#if DUMP_REGEXP
static void printNode(eccregexnode_t* n)
{
    switch(n->opcode)
    {
        case ECC_RXOP_OVER:
            fprintf(stderr, "over ");
            break;
        case ECC_RXOP_NLOOKAHEAD:
            fprintf(stderr, "!lookahead ");
            break;
        case ECC_RXOP_LOOKAHEAD:
            fprintf(stderr, "lookahead ");
            break;
        case ECC_RXOP_START:
            fprintf(stderr, "start ");
            break;
        case ECC_RXOP_END:
            fprintf(stderr, "end ");
            break;
        case ECC_RXOP_LINESTART:
            fprintf(stderr, "line start ");
            break;
        case ECC_RXOP_LINEEND:
            fprintf(stderr, "line end ");
            break;
        case ECC_RXOP_BOUNDARY:
            fprintf(stderr, "boundary ");
            break;
        case ECC_RXOP_SPLIT:
            fprintf(stderr, "split ");
            break;
        case ECC_RXOP_REFERENCE:
            fprintf(stderr, "reference ");
            break;
        case ECC_RXOP_REDO:
            fprintf(stderr, "redo ");
            break;
        case ECC_RXOP_SAVE:
            fprintf(stderr, "save ");
            break;
        case ECC_RXOP_NSAVE:
            fprintf(stderr, "!save ");
            break;
        case ECC_RXOP_ANY:
            fprintf(stderr, "any ");
            break;
        case ECC_RXOP_ONEOF:
            fprintf(stderr, "one of ");
            break;
        case ECC_RXOP_NEITHEROF:
            fprintf(stderr, "neither of ");
            break;
        case ECC_RXOP_INRANGE:
            fprintf(stderr, "in range ");
            break;
        case ECC_RXOP_INRANGECASE:
            fprintf(stderr, "in range (ignore case) ");
            break;
        case ECC_RXOP_DIGIT:
            fprintf(stderr, "digit ");
            break;
        case ECC_RXOP_SPACE:
            fprintf(stderr, "space ");
            break;
        case ECC_RXOP_WORD:
            fprintf(stderr, "word ");
            break;
        case ECC_RXOP_BYTES:
            fprintf(stderr, "bytes ");
            break;
        case ECC_RXOP_JUMP:
            fprintf(stderr, "jump ");
            break;
        case ECC_RXOP_MATCH:
            fprintf(stderr, "match ");
            break;
    }
    fprintf(stderr, "%d", n->offset);
    if(n->bytes)
    {
        if(n->opcode == ECC_RXOP_REDO)
        {
            char* c = n->bytes + 2;
            fprintf(stderr, " {%u-%u}", n->bytes[0], n->bytes[1]);
            if(n->bytes[2])
            {
                fprintf(stderr, " lazy");
            }
            if(*(c + 1))
            {
                fprintf(stderr, " clear:");
            }
            while(*(++c))
            {
                fprintf(stderr, "%u,", *c);
            }
        }
        else if(n->opcode != ECC_RXOP_REDO)
        {
            fprintf(stderr, " `%s`", n->bytes);
        }
    }
    putc('\n', stderr);
}
#endif

//MARK: parsing

static eccregexnode_t* ecc_regexp_node(eccrxopcode_t opcode, long offset, const char* bytes)
{
    eccregexnode_t* n;
    n = (eccregexnode_t*)calloc(2, sizeof(*n));

    if(offset && bytes)
    {
        n[0].bytes = (char*)calloc(offset + 1, 1);
        memcpy(n[0].bytes, bytes, offset);
    }
    n[0].offset = offset;
    n[0].opcode = opcode;

    return n;
}

static void ecc_regexp_toss(eccregexnode_t* node)
{
    eccregexnode_t* n = node;

    if(!node)
        return;

    while(n->opcode != ECC_RXOP_OVER)
    {
        free(n->bytes), n->bytes = NULL;
        ++n;
    }

    free(node), node = NULL;
}

static uint16_t ecc_regexp_nlen(eccregexnode_t* n)
{
    uint16_t len = 0;
    if(n)
    {
        while(n[++len].opcode != ECC_RXOP_OVER)
        {
        }
    }
    return len;
}

static eccregexnode_t* ecc_regexp_join(eccregexnode_t* a, eccregexnode_t* b)
{
    uint16_t lena = 0, lenb = 0;

    if(!a)
        return b;
    else if(!b)
        return a;

    while(a[++lena].opcode != ECC_RXOP_OVER)
        ;
    while(b[++lenb].opcode != ECC_RXOP_OVER)
        ;

    if(lena == 1 && lenb == 1 && a[lena - 1].opcode == ECC_RXOP_BYTES && b->opcode == ECC_RXOP_BYTES)
    {
        eccregexnode_t* c = a + lena - 1;

        c->bytes = (char*)realloc(c->bytes, c->offset + b->offset + 1);
        memcpy(c->bytes + c->offset, b->bytes, b->offset + 1);
        c->offset += b->offset;
        free(b->bytes), b->bytes = NULL;
    }
    else
    {
        a = (eccregexnode_t*)realloc(a, sizeof(*a) * (lena + lenb + 1));
        memcpy(a + lena, b, sizeof(*a) * (lenb + 1));
    }
    free(b), b = NULL;
    return a;
}

static int ecc_regexp_accept(eccrxparser_t* p, char c)
{
    if(*p->c == c)
    {
        ++p->c;
        return 1;
    }
    return 0;
}

static eccrxopcode_t ecc_regexp_escape(eccrxparser_t* p, int16_t* offset, char buffer[5])
{
    *offset = 1;
    buffer[0] = *(p->c++);

    switch(buffer[0])
    {
        case 'D':
            {
                *offset = 0;
            }
            /* fallthrough */
        case 'd':
            return ECC_RXOP_DIGIT;
        case 'S':
            {
                *offset = 0;
            }
            /* fallthrough */
        case 's':
            return ECC_RXOP_SPACE;

        case 'W':
            {
                *offset = 0;
            }
            /* fallthrough */
        case 'w':
            return ECC_RXOP_WORD;

        case 'b':
            buffer[0] = '\b';
            break;
        case 'f':
            buffer[0] = '\f';
            break;
        case 'n':
            buffer[0] = '\n';
            break;
        case 'r':
            buffer[0] = '\r';
            break;
        case 't':
            buffer[0] = '\t';
            break;
        case 'v':
            buffer[0] = '\v';
            break;
        case 'c':
            {
                if(tolower(*p->c) >= 'a' && tolower(*p->c) <= 'z')
                    buffer[0] = *(p->c++) % 32;
            }
            break;

        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
            {
                buffer[0] -= '0';
                if(*p->c >= '0' && *p->c <= '7')
                {
                    buffer[0] = buffer[0] * 8 + *(p->c++) - '0';
                    if(*p->c >= '0' && *p->c <= '7')
                    {
                        buffer[0] = buffer[0] * 8 + *(p->c++) - '0';
                        if(*p->c >= '0' && *p->c <= '7')
                        {
                            if((int)buffer[0] * 8 + *p->c - '0' <= 0xFF)
                                buffer[0] = buffer[0] * 8 + *(p->c++) - '0';
                        }
                    }

                    if(buffer[0])
                        *offset = ecc_strbuf_writecodepoint(buffer, ((uint8_t*)buffer)[0]);
                }
            }
            break;

        case 'x':
            {
                if(isxdigit(p->c[0]) && isxdigit(p->c[1]))
                {
                    *offset = ecc_strbuf_writecodepoint(buffer, ecc_astlex_uint8hex(p->c[0], p->c[1]));
                    p->c += 2;
                }
            }
            break;

        case 'u':
            {
                if(isxdigit(p->c[0]) && isxdigit(p->c[1]) && isxdigit(p->c[2]) && isxdigit(p->c[3]))
                {
                    *offset = ecc_strbuf_writecodepoint(buffer, ecc_astlex_uint16hex(p->c[0], p->c[1], p->c[2], p->c[3]));
                    p->c += 4;
                }
            }
            break;

        default:
            break;
    }

    return ECC_RXOP_BYTES;
}

static eccrxopcode_t ecc_regexp_character(ecctextstring_t text, int16_t* offset, char buffer[12], int ignoreCase)
{
    if(ignoreCase)
    {
        char* split = ecc_textbuf_tolower(text, buffer);
        char* check = ecc_textbuf_toupper(text, split);
        ptrdiff_t length = check - buffer;
        int codepoints = 0;

        *check = '\0';
        while(check-- > buffer)
            if((*check & 0xc0) != 0x80)
                codepoints++;

        if(codepoints == 2 && memcmp(buffer, split, split - buffer))
        {
            *offset = length;
            return ECC_RXOP_ONEOF;
        }
    }
    *offset = text.length;
    memcpy(buffer, text.bytes, text.length);
    return ECC_RXOP_BYTES;
}

static eccregexnode_t* ecc_regexp_characternode(ecctextstring_t text, int ignoreCase)
{
    char buffer[12];
    int16_t offset;
    eccrxopcode_t opcode = ecc_regexp_character(text, &offset, buffer, ignoreCase);

    return ecc_regexp_node(opcode, offset, buffer);
}

static eccregexnode_t* ecc_regexp_term(eccrxparser_t* p, eccobjerror_t** error)
{
    eccregexnode_t* n;
    ecctextstring_t text;

    p->disallowQuantifier = 0;

    if(p->c >= p->end - 1)
        return NULL;
    else if(ecc_regexp_accept(p, '^'))
    {
        p->disallowQuantifier = 1;
        return ecc_regexp_node(p->multiline ? ECC_RXOP_LINESTART : ECC_RXOP_START, 0, NULL);
    }
    else if(ecc_regexp_accept(p, '$'))
    {
        p->disallowQuantifier = 1;
        return ecc_regexp_node(p->multiline ? ECC_RXOP_LINEEND : ECC_RXOP_END, 0, NULL);
    }
    else if(ecc_regexp_accept(p, '\\'))
    {
        switch(*p->c)
        {
            case 'b':
                ++p->c;
                p->disallowQuantifier = 1;
                return ecc_regexp_node(ECC_RXOP_BOUNDARY, 1, NULL);

            case 'B':
                ++p->c;
                p->disallowQuantifier = 1;
                return ecc_regexp_node(ECC_RXOP_BOUNDARY, 0, NULL);

            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
            {
                int c = *(p->c++) - '0';
                while(isdigit(*(p->c)))
                    c = c * 10 + *(p->c++) - '0';

                return ecc_regexp_node(ECC_RXOP_REFERENCE, c, NULL);
            }

            default:
            {
                eccrxopcode_t opcode;
                int16_t offset;
                char buffer[5];

                opcode = ecc_regexp_escape(p, &offset, buffer);
                if(opcode == ECC_RXOP_BYTES)
                {
                    text = ecc_textbuf_make(buffer, offset);
                    return ecc_regexp_characternode(text, p->ignoreCase);
                }
                else
                    return ecc_regexp_node(opcode, offset, NULL);
            }
        }
    }
    else if(ecc_regexp_accept(p, '('))
    {
        unsigned char count = 0;
        char modifier = '\0';

        if(ecc_regexp_accept(p, '?'))
        {
            if(*p->c == '=' || *p->c == '!' || *p->c == ':')
                modifier = *(p->c++);
        }
        else
        {
            count = ++p->count;
            if((int)count * 2 + 1 > 0xff)
            {
                *error = ecc_error_syntaxerror(ecc_textbuf_make(p->c, 1), ecc_strbuf_create("too many captures"));
                return NULL;
            }
        }

        n = ecc_regexp_disjunction(p, error);
        if(!ecc_regexp_accept(p, ')'))
        {
            if(!*error)
                *error = ecc_error_syntaxerror(ecc_textbuf_make(p->c, 1), ecc_strbuf_create("expect ')'"));

            return NULL;
        }

        switch(modifier)
        {
            case '\0':
                return ecc_regexp_join(ecc_regexp_node(ECC_RXOP_SAVE, count * 2, NULL), ecc_regexp_join(n, ecc_regexp_node(ECC_RXOP_SAVE, count * 2 + 1, NULL)));

            case '=':
            {
                int c = ecc_regexp_nlen(n);
                p->disallowQuantifier = 1;
                return ecc_regexp_join(ecc_regexp_node(ECC_RXOP_LOOKAHEAD, c + 2, NULL), ecc_regexp_join(n, ecc_regexp_node(ECC_RXOP_MATCH, 0, NULL)));
            }

            case '!':
            {
                int i = 0, c = ecc_regexp_nlen(n);
                p->disallowQuantifier = 1;
                for(; i < c; ++i)
                    if(n[i].opcode == ECC_RXOP_SAVE)
                        n[i].opcode = ECC_RXOP_NSAVE;

                return ecc_regexp_join(ecc_regexp_node(ECC_RXOP_NLOOKAHEAD, c + 2, NULL), ecc_regexp_join(n, ecc_regexp_node(ECC_RXOP_MATCH, 0, NULL)));
            }
            case ':':
                return n;
        }
    }
    else if(ecc_regexp_accept(p, '.'))
        return ecc_regexp_node(ECC_RXOP_ANY, 0, NULL);
    else if(ecc_regexp_accept(p, '['))
    {
        eccrxopcode_t opcode;
        int16_t offset;

        int isnot = ecc_regexp_accept(p, '^'), length = 0, lastLength, range = -1;
        char buffer[255];
        n = NULL;

        while(*p->c != ']' || range >= 0)
        {
            lastLength = length;

            if(ecc_regexp_accept(p, '\\'))
            {
                opcode = ecc_regexp_escape(p, &offset, buffer + length);
                if(opcode == ECC_RXOP_BYTES)
                    length += offset;
                else
                {
                    if(isnot )
                        n = ecc_regexp_join(ecc_regexp_node(ECC_RXOP_NLOOKAHEAD, 3, NULL), ecc_regexp_join(ecc_regexp_node(opcode, offset, NULL), ecc_regexp_join(ecc_regexp_node(ECC_RXOP_MATCH, 0, NULL), n)));
                    else
                        n = ecc_regexp_join(ecc_regexp_node(ECC_RXOP_SPLIT, 3, NULL), ecc_regexp_join(ecc_regexp_node(opcode, offset, NULL), ecc_regexp_join(ecc_regexp_node(ECC_RXOP_JUMP, ecc_regexp_nlen(n) + 2, NULL), n)));
                }
            }
            else
            {
                opcode = ECC_RXOP_BYTES;
                buffer[length++] = *(p->c++);
            }

            if(range >= 0)
            {
                if(opcode == ECC_RXOP_BYTES)
                {
                    ecctextstring_t innertxt = ecc_textbuf_make(buffer + range, length - range);
                    ecctextchar_t from, to;

                    from = ecc_textbuf_nextcharacter(&innertxt);
                    ecc_textbuf_advance(&innertxt, 1);
                    to = ecc_textbuf_nextcharacter(&innertxt);

                    if(from.codepoint > to.codepoint)
                    {
                        ecc_regexp_toss(n);
                        *error = ecc_error_syntaxerror(ecc_textbuf_make(p->c - length - range, length - range),
                                                             ecc_strbuf_create("range out of order in character class"));
                        return NULL;
                    }

                    if(isnot )
                        n = ecc_regexp_join(ecc_regexp_node(ECC_RXOP_NLOOKAHEAD, 3, NULL),
                                 ecc_regexp_join(ecc_regexp_node(p->ignoreCase ? ECC_RXOP_INRANGECASE : ECC_RXOP_INRANGE, length - range, buffer + range), ecc_regexp_join(ecc_regexp_node(ECC_RXOP_MATCH, 0, NULL), n)));
                    else
                        n = ecc_regexp_join(ecc_regexp_node(ECC_RXOP_SPLIT, 3, NULL),
                                 ecc_regexp_join(ecc_regexp_node(p->ignoreCase ? ECC_RXOP_INRANGECASE : ECC_RXOP_INRANGE, length - range, buffer + range), ecc_regexp_join(ecc_regexp_node(ECC_RXOP_JUMP, ecc_regexp_nlen(n) + 2, NULL), n)));

                    length = range;
                }
                range = -1;
            }
            if(opcode == ECC_RXOP_BYTES && *p->c == '-')
            {
                buffer[length++] = *(p->c++);
                range = lastLength;
            }
            if((p->c >= p->end) || (length >= (int)sizeof(buffer)))
            {
                *error = ecc_error_syntaxerror(ecc_textbuf_make(p->c - 1, 1), ecc_strbuf_create("expect ']'"));
                return NULL;
            }
        }
        if(p->ignoreCase)
        {
            char casebuffer[6];
            ecctextstring_t single;
            ecctextchar_t c;
            text = ecc_textbuf_make(buffer + length, length);

            while(text.length)
            {
                c = ecc_textbuf_prevcharacter(&text);
                single = text;
                single.length = c.units;

                offset = ecc_textbuf_tolower(single, casebuffer) - casebuffer;
                if(memcmp(text.bytes, casebuffer, offset))
                {
                    memcpy(buffer + length, casebuffer, offset);
                    length += offset;
                }

                offset = ecc_textbuf_toupper(single, casebuffer) - casebuffer;
                if(memcmp(text.bytes, casebuffer, offset))
                {
                    memcpy(buffer + length, casebuffer, offset);
                    length += offset;
                }
            }
        }

        buffer[length] = '\0';
        ecc_regexp_accept(p, ']');
        return ecc_regexp_join(n, ecc_regexp_node(isnot ? ECC_RXOP_NEITHEROF : ECC_RXOP_ONEOF, length, buffer));
    }
    else if(*p->c && strchr("*+?)}|", *p->c))
        return NULL;

    text = ecc_textbuf_make(p->c, (int32_t)(p->end - p->c));
    text.length = ecc_textbuf_character(text).units;
    p->c += text.length;
    return ecc_regexp_characternode(text, p->ignoreCase);
}

static eccregexnode_t* ecc_regexp_alternative(eccrxparser_t* p, eccobjerror_t** error)
{
    eccregexnode_t *n = NULL, *t = NULL;

    for(;;)
    {
        if(!(t = ecc_regexp_term(p, error)))
            break;

        if(!p->disallowQuantifier)
        {
            int noop = 0, lazy = 0;

            uint8_t quantifier = ecc_regexp_accept(p, '?') ? '?' : ecc_regexp_accept(p, '*') ? '*' : ecc_regexp_accept(p, '+') ? '+' : ecc_regexp_accept(p, '{') ? '{' : '\0', min = 1, max = 1;

            switch(quantifier)
            {
                case '?':
                    min = 0, max = 1;
                    break;

                case '*':
                    min = 0, max = 0;
                    break;

                case '+':
                    min = 1, max = 0;
                    break;

                case '{':
                {
                    if(isdigit(*p->c))
                    {
                        min = *(p->c++) - '0';
                        while(isdigit(*p->c))
                            min = min * 10 + *(p->c++) - '0';
                    }
                    else
                    {
                        *error = ecc_error_syntaxerror(ecc_textbuf_make(p->c, 1), ecc_strbuf_create("expect number"));
                        goto error;
                    }

                    if(ecc_regexp_accept(p, ','))
                    {
                        if(isdigit(*p->c))
                        {
                            max = *(p->c++) - '0';
                            while(isdigit(*p->c))
                                max = max * 10 + *(p->c++) - '0';

                            if(!max)
                                noop = 1;
                        }
                        else
                            max = 0;
                    }
                    else if(!min)
                        noop = 1;
                    else
                        max = min;

                    if(!ecc_regexp_accept(p, '}'))
                    {
                        *error = ecc_error_syntaxerror(ecc_textbuf_make(p->c, 1), ecc_strbuf_create("expect '}'"));
                        goto error;
                    }
                    break;
                }
            }

            lazy = ecc_regexp_accept(p, '?');
            if(noop)
            {
                ecc_regexp_toss(t);
                continue;
            }

            if(max != 1)
            {
                eccregexnode_t* redo;
                uint16_t index, count = ecc_regexp_nlen(t) - (lazy ? 1 : 0), length = 3;
                char buffer[count + length];

                for(index = (lazy ? 1 : 0); index < count; ++index)
                    if(t[index].opcode == ECC_RXOP_SAVE)
                        buffer[length++] = (uint8_t)t[index].offset;

                buffer[0] = min;
                buffer[1] = max;
                buffer[2] = lazy;
                buffer[length] = '\0';

                redo = ecc_regexp_node(ECC_RXOP_REDO, -ecc_regexp_nlen(t), NULL);
                redo->bytes = (char*)malloc(length + 1);
                memcpy(redo->bytes, buffer, length + 1);

                t = ecc_regexp_join(t, redo);
            }

            if(min == 0)
            {
                if(lazy)
                    t = ecc_regexp_join(ecc_regexp_node(ECC_RXOP_SPLIT, 2, NULL), ecc_regexp_join(ecc_regexp_node(ECC_RXOP_JUMP, ecc_regexp_nlen(t) + 1, NULL), t));
                else
                    t = ecc_regexp_join(ecc_regexp_node(ECC_RXOP_SPLIT, ecc_regexp_nlen(t) + 1, NULL), t);
            }
        }
        n = ecc_regexp_join(n, t);
    }
    return n;

error:
    ecc_regexp_toss(t);
    return n;
}

static eccregexnode_t* ecc_regexp_disjunction(eccrxparser_t* p, eccobjerror_t** error)
{
    eccregexnode_t *n = ecc_regexp_alternative(p, error), *d;
    if(ecc_regexp_accept(p, '|'))
    {
        uint16_t len;
        d = ecc_regexp_disjunction(p, error);
        n = ecc_regexp_join(n, ecc_regexp_node(ECC_RXOP_JUMP, ecc_regexp_nlen(d) + 1, NULL));
        len = ecc_regexp_nlen(n);
        n = ecc_regexp_join(n, d);
        n = ecc_regexp_join(ecc_regexp_node(ECC_RXOP_SPLIT, len + 1, NULL), n);
    }
    return n;
}

static eccregexnode_t* ecc_regexp_pattern(eccrxparser_t* p, eccobjerror_t** error)
{
    assert(*p->c == '/');
    assert(*(p->end - 1) == '/');

    ++p->c;
    return ecc_regexp_join(ecc_regexp_disjunction(p, error), ecc_regexp_node(ECC_RXOP_MATCH, 0, NULL));
}

static void ecc_regexp_clear(eccrxstate_t* const s, const char* c, uint8_t* bytes)
{
    uint8_t index;

    if(!bytes)
        return;

    while(*bytes)
    {
        index = *bytes++;
        s->index[index] = index % 2 ? 0 : c;
    }
}

static int ecc_regexp_forkmatch(eccrxstate_t* const s, eccregexnode_t* n, ecctextstring_t text, int16_t offset)
{
    int result;

    if(n->depth == 0xff)
        return 0;//!\\ too many recursion
    else
        ++n->depth;

    result = ecc_regexp_match(s, n + offset, text);
    --n->depth;
    return result;
}

static int ecc_regexp_match(eccrxstate_t* const s, eccregexnode_t* n, ecctextstring_t text)
{
    goto start;
next:
    ++n;
start:;

    //	#if DUMP_REGEXP
    //		fprintf(stderr, "\n%.*s\n^ ", text.length, text.bytes);
    //		printNode(n);
    //	#endif

    switch((eccrxopcode_t)n->opcode)
    {
        case ECC_RXOP_NLOOKAHEAD:
        case ECC_RXOP_LOOKAHEAD:
            if(ecc_regexp_forkmatch(s, n, text, 1) == (n->opcode - 1))
                goto jump;

            return 0;

        case ECC_RXOP_START:
            if(text.bytes != s->start)
                return 0;

            goto next;

        case ECC_RXOP_END:
            if(text.bytes != s->end)
                return 0;

            goto next;

        case ECC_RXOP_LINESTART:
        {
            ecctextstring_t prev = ecc_textbuf_make(text.bytes, (int32_t)(text.bytes - s->start));
            if(text.bytes != s->start && !ecc_textbuf_islinefeed(ecc_textbuf_prevcharacter(&prev)))
                return 0;

            goto next;
        }

        case ECC_RXOP_LINEEND:
            if(text.bytes != s->end && !ecc_textbuf_islinefeed(ecc_textbuf_character(text)))
                return 0;

            goto next;

        case ECC_RXOP_BOUNDARY:
        {
            ecctextstring_t prev = ecc_textbuf_make(text.bytes, (int32_t)(text.bytes - s->start));
            if(text.bytes == s->start)
            {
                if(ecc_textbuf_isword(ecc_textbuf_character(text)) != n->offset)
                    return 0;
            }
            else if(text.bytes == s->end)
            {
                if(ecc_textbuf_isword(ecc_textbuf_prevcharacter(&prev)) != n->offset)
                    return 0;
            }
            else
            {
                if((ecc_textbuf_isword(ecc_textbuf_prevcharacter(&prev)) != ecc_textbuf_isword(ecc_textbuf_character(text))) != n->offset)
                    return 0;
            }
            goto next;
        }

        case ECC_RXOP_SPLIT:
            if(ecc_regexp_forkmatch(s, n, text, 1))
                return 1;

            goto jump;

        case ECC_RXOP_REFERENCE:
        {
            uint16_t len = s->capture[n->offset * 2 + 1] ? s->capture[n->offset * 2 + 1] - s->capture[n->offset * 2] : 0;

            if(len)
            {
                //#if DUMP_REGEXP
                //				fprintf(stderr, "ref: %.*s", len, s->capture[n->offset * 2]);
                //#endif
                if(text.length < len || memcmp(text.bytes, s->capture[n->offset * 2], len))
                    return 0;

                ecc_textbuf_advance(&text, len);
            }
            goto next;
        }

        case ECC_RXOP_REDO:
        {
            int hasmin = n->depth + 1 >= n->bytes[0];
            int lazy = n->bytes[2] && hasmin;

            if(n->bytes[1] && n->depth >= n->bytes[1])
                return 0;

            if(ecc_regexp_forkmatch(s, n, text, lazy ? 1 : n->offset))
            {
                ecc_regexp_clear(s, text.bytes, (uint8_t*)n->bytes + 3);
                return 1;
            }

            if(!hasmin)
                return 0;

            if(lazy)
                goto jump;
            else
                goto next;
        }

        case ECC_RXOP_SAVE:
        {
            const char* capture = s->capture[n->offset];
            s->capture[n->offset] = text.bytes;
            if(ecc_regexp_forkmatch(s, n, text, 1))
            {
                if(s->capture[n->offset] <= s->index[n->offset] && text.bytes == s->capture[n->offset])
                {
                    s->capture[n->offset] = NULL;
                }
                return 1;
            }
            s->capture[n->offset] = capture;
            return 0;
        }

        case ECC_RXOP_NSAVE:
        {
            const char* capture = s->capture[n->offset];
            s->capture[n->offset] = text.bytes;
            if(!ecc_regexp_forkmatch(s, n, text, 1))
            {
                s->capture[n->offset] = NULL;
                return 0;
            }
            s->capture[n->offset] = capture;
            return 1;
        }

        case ECC_RXOP_DIGIT:
            if(text.length < 1 || ecc_textbuf_isdigit(ecc_textbuf_nextcharacter(&text)) != n->offset)
                return 0;

            goto next;

        case ECC_RXOP_SPACE:
            if(text.length < 1 || ecc_textbuf_isspace(ecc_textbuf_nextcharacter(&text)) != n->offset)
                return 0;

            goto next;

        case ECC_RXOP_WORD:
            if(text.length < 1 || ecc_textbuf_isword(ecc_textbuf_nextcharacter(&text)) != n->offset)
                return 0;

            goto next;

        case ECC_RXOP_BYTES:
            if(text.length < n->offset || memcmp(n->bytes, text.bytes, n->offset))
                return 0;

            ecc_textbuf_advance(&text, n->offset);
            goto next;

        case ECC_RXOP_ONEOF:
        {
            char buffer[5];
            ecctextchar_t c;

            if(!text.length)
                return 0;

            c = ecc_textbuf_character(text);
            memcpy(buffer, text.bytes, c.units);
            buffer[c.units] = '\0';

            if(n->bytes && strstr(n->bytes, buffer))
            {
                ecc_textbuf_nextcharacter(&text);
                goto next;
            }
            return 0;
        }

        case ECC_RXOP_NEITHEROF:
        {
            char buffer[5];
            ecctextchar_t c;

            if(!text.length)
                return 0;

            c = ecc_textbuf_character(text);
            memcpy(buffer, text.bytes, c.units);
            buffer[c.units] = '\0';

            if(n->bytes && strstr(n->bytes, buffer))
                return 0;

            ecc_textbuf_nextcharacter(&text);
            goto next;
        }

        case ECC_RXOP_INRANGE:
        case ECC_RXOP_INRANGECASE:
        {
            ecctextstring_t range = ecc_textbuf_make(n->bytes, n->offset);
            ecctextchar_t from, to, c;

            if(!text.length)
                return 0;

            from = ecc_textbuf_nextcharacter(&range);
            ecc_textbuf_advance(&range, 1);
            to = ecc_textbuf_nextcharacter(&range);
            c = ecc_textbuf_character(text);

            if(n->opcode == ECC_RXOP_INRANGECASE)
            {
                char buffer[c.units];
                ecctextstring_t casetext = ecc_textbuf_make(buffer, 0);

                casetext.length = (int32_t)(ecc_textbuf_tolower(ecc_textbuf_make(text.bytes, (int32_t)sizeof(buffer)), buffer) - buffer);
                c = ecc_textbuf_character(casetext);
                if(c.units == casetext.length && (c.codepoint >= from.codepoint && c.codepoint <= to.codepoint))
                {
                    ecc_textbuf_nextcharacter(&text);
                    goto next;
                }

                casetext.length = (int32_t)(ecc_textbuf_toupper(ecc_textbuf_make(text.bytes, (int32_t)sizeof(buffer)), buffer) - buffer);
                c = ecc_textbuf_character(casetext);
                if(c.units == casetext.length && (c.codepoint >= from.codepoint && c.codepoint <= to.codepoint))
                {
                    ecc_textbuf_nextcharacter(&text);
                    goto next;
                }
            }
            else
            {
                if((c.codepoint >= from.codepoint && c.codepoint <= to.codepoint))
                {
                    ecc_textbuf_nextcharacter(&text);
                    goto next;
                }
            }

            return 0;
        }

        case ECC_RXOP_ANY:
            if(text.length >= 1 && !ecc_textbuf_islinefeed(ecc_textbuf_nextcharacter(&text)))
                goto next;

            return 0;

        case ECC_RXOP_JUMP:
            goto jump;

        case ECC_RXOP_MATCH:
            s->capture[1] = text.bytes;
            return 1;

        case ECC_RXOP_OVER:
            break;
    }
    abort();

jump:
    n += n->offset;
    goto start;
}

static eccvalue_t objregexpfn_constructor(ecccontext_t* context)
{
    eccvalue_t pattern, flags;

    pattern = ecc_context_argument(context, 0);
    flags = ecc_context_argument(context, 1);

    return ecc_value_regexp(ecc_regexp_createwith(context, pattern, flags));
}

static eccvalue_t objregexpfn_toString(ecccontext_t* context)
{
    eccobjregexp_t* self = context->thisvalue.data.regexp;

    ecc_context_assertthistype(context, ECC_VALTYPE_REGEXP);

    return ecc_value_fromchars(self->pattern);
}

static eccvalue_t objregexpfn_exec(ecccontext_t* context)
{
    eccobjregexp_t* self = context->thisvalue.data.regexp;
    eccvalue_t value, lastIndex;

    ecc_context_assertthistype(context, ECC_VALTYPE_REGEXP);

    value = ecc_value_tostring(context, ecc_context_argument(context, 0));
    lastIndex = self->global ? ecc_value_tointeger(context, ecc_object_getmember(context, &self->object, ECC_ConstKey_lastIndex)) : ecc_value_fromint(0);

    ecc_object_putmember(context, &self->object, ECC_ConstKey_lastIndex, ecc_value_fromint(0));

    if(lastIndex.data.integer >= 0)
    {
        uint16_t length = ecc_value_stringlength(&value);
        const char* bytes = ecc_value_stringbytes(&value);
        const char* capture[self->count * 2];
        const char* strindex[self->count * 2];
        eccstrbuffer_t* element;

        eccrxstate_t state = { ecc_string_textatindex(bytes, length, lastIndex.data.integer, 0).bytes, bytes + length, capture, strindex, 0};

        if(state.start >= bytes && state.start <= state.end && ecc_regexp_matchwithstate(self, &state))
        {
            eccobject_t* array = ecc_array_createsized(self->count);
            int32_t numindex, count;

            for(numindex = 0, count = self->count; numindex < count; ++numindex)
            {
                if(capture[numindex * 2])
                {
                    element = ecc_strbuf_createwithbytes((int32_t)(capture[numindex * 2 + 1] - capture[numindex * 2]), capture[numindex * 2]);
                    array->element[numindex].hmapitemvalue = ecc_value_fromchars(element);
                }
                else
                    array->element[numindex].hmapitemvalue = ECCValConstUndefined;
            }

            if(self->global)
                ecc_object_putmember(context, &self->object, ECC_ConstKey_lastIndex,
                                      ecc_value_fromint(ecc_string_unitindex(bytes, length, (int32_t)(capture[1] - bytes))));

            ecc_object_addmember(array, ECC_ConstKey_index, ecc_value_fromint(ecc_string_unitindex(bytes, length, (int32_t)(capture[0] - bytes))), 0);
            ecc_object_addmember(array, ECC_ConstKey_input, value, 0);

            return ecc_value_object(array);
        }
    }
    return ECCValConstNull;
}

static eccvalue_t objregexpfn_test(ecccontext_t* context)
{
    eccobjregexp_t* self = context->thisvalue.data.regexp;
    eccvalue_t value, lastIndex;

    ecc_context_assertthistype(context, ECC_VALTYPE_REGEXP);

    value = ecc_value_tostring(context, ecc_context_argument(context, 0));
    lastIndex = ecc_value_tointeger(context, ecc_object_getmember(context, &self->object, ECC_ConstKey_lastIndex));

    ecc_object_putmember(context, &self->object, ECC_ConstKey_lastIndex, ecc_value_fromint(0));

    if(lastIndex.data.integer >= 0)
    {
        uint16_t length = ecc_value_stringlength(&value);
        const char* bytes = ecc_value_stringbytes(&value);
        const char* capture[self->count * 2];
        const char* index[self->count * 2];

        eccrxstate_t state = { ecc_string_textatindex(bytes, length, lastIndex.data.integer, 0).bytes, bytes + length, capture, index, 0};

        if(state.start >= bytes && state.start <= state.end && ecc_regexp_matchwithstate(self, &state))
        {
            if(self->global)
                ecc_object_putmember(context, &self->object, ECC_ConstKey_lastIndex,
                                      ecc_value_fromint(ecc_string_unitindex(bytes, length, (int32_t)(capture[1] - bytes))));

            return ECCValConstTrue;
        }
    }
    return ECCValConstFalse;
}

// MARK: - Methods

void ecc_regexp_setup()
{
    eccobjerror_t* error = NULL;
    const eccvalflag_t h = ECC_VALFLAG_HIDDEN;

    ecc_function_setupbuiltinobject(&ECC_CtorFunc_Regexp, objregexpfn_constructor, 2, &ECC_Prototype_Regexp,
                                          ecc_value_regexp(ecc_regexp_create(ecc_strbuf_create("/(?:)/"), &error, 0)), &ECC_Type_Regexp);

    assert(error == NULL);

    ecc_function_addto(ECC_Prototype_Regexp, "toString", objregexpfn_toString, 0, h);
    ecc_function_addto(ECC_Prototype_Regexp, "exec", objregexpfn_exec, 1, h);
    ecc_function_addto(ECC_Prototype_Regexp, "test", objregexpfn_test, 1, h);
}

void ecc_regexp_teardown(void)
{
    ECC_Prototype_Regexp = NULL;
    ECC_CtorFunc_Regexp = NULL;
}

eccobjregexp_t* ecc_regexp_create(eccstrbuffer_t* s, eccobjerror_t** error, int options)
{
    eccrxparser_t p = { 0 };

    eccobjregexp_t* self;
    self = (eccobjregexp_t*)malloc(sizeof(*self));
    memset(self, 0, sizeof(eccobjregexp_t));
    ecc_mempool_addobject(&self->object);

    ecc_object_initialize(&self->object, ECC_Prototype_Regexp);

    p.c = s->bytes;
    p.end = s->bytes + s->length;
    while(p.end > p.c && *(p.end - 1) != '/')
    {
        switch(*(--p.end))
        {
            case 'i':
                p.ignoreCase = 1;
                continue;

            case 'm':
                p.multiline = 1;
                continue;
        }
    }

#if DUMP_REGEXP
    fprintf(stderr, "\n%.*s\n", s->length, s->bytes);
#endif

    self->pattern = s;
    self->program = ecc_regexp_pattern(&p, error);
    self->count = p.count + 1;
    self->source = ecc_strbuf_createwithbytes((int32_t)(p.c - self->pattern->bytes - 1), self->pattern->bytes + 1);

    //	++self->pattern->refcount;
    //	++self->source->refcount;

    if(error && *p.c == '/')
    {
        for(;;)
        {
            switch(*(++p.c))
            {
                case 'g':
                    {
                    g:
                        if(self->global == 1)
                        {
                            *error = ecc_error_syntaxerror(ecc_textbuf_make(p.c, 1), ecc_strbuf_create("invalid flag"));
                        }
                        self->global = 1;
                    }
                    continue;

                case 'i':
                    {
                    i:
                        if(self->ignoreCase == 1)
                        {
                            *error = ecc_error_syntaxerror(ecc_textbuf_make(p.c, 1), ecc_strbuf_create("invalid flag"));
                        }
                        self->ignoreCase = 1;
                    }
                    continue;

                case 'm':
                    {
                    m:
                        if(self->multiline == 1)
                        {
                            *error = ecc_error_syntaxerror(ecc_textbuf_make(p.c, 1), ecc_strbuf_create("invalid flag"));
                        }
                        self->multiline = 1;
                    }
                    continue;
                case '\0':
                    {
                    }
                    break;
                case '\\':
                    {
                        if(options & ECC_REGEXOPT_ALLOWUNICODEFLAGS)
                        {
                            if(!memcmp(p.c + 1, "u0067", 5))
                            {
                                p.c += 5;
                                goto g;
                            }
                            else if(!memcmp(p.c + 1, "u0069", 5))
                            {
                                p.c += 5;
                                goto i;
                            }
                            else if(!memcmp(p.c + 1, "u006d", 5) || !memcmp(p.c + 1, "u006D", 5))
                            {
                                p.c += 5;
                                goto m;
                            }
                        }
                    }
                    /* fallthrough */

                default:
                    {
                        *error = ecc_error_syntaxerror(ecc_textbuf_make(p.c, 1), ecc_strbuf_create("invalid flag"));
                    }
            }
            break;
        }
    }
    else if(error && !*error)
    {
        *error = ecc_error_syntaxerror(ecc_textbuf_make(p.c, 1), ecc_strbuf_create("invalid character '%c'", isgraph(*p.c) ? *p.c : '?'));
    }
    ecc_object_addmember(&self->object, ECC_ConstKey_source, ecc_value_fromchars(self->source), ECC_VALFLAG_READONLY | ECC_VALFLAG_HIDDEN | ECC_VALFLAG_SEALED);
    ecc_object_addmember(&self->object, ECC_ConstKey_global, ecc_value_truth(self->global), ECC_VALFLAG_READONLY | ECC_VALFLAG_HIDDEN | ECC_VALFLAG_SEALED);
    ecc_object_addmember(&self->object, ECC_ConstKey_ignoreCase, ecc_value_truth(self->ignoreCase), ECC_VALFLAG_READONLY | ECC_VALFLAG_HIDDEN | ECC_VALFLAG_SEALED);
    ecc_object_addmember(&self->object, ECC_ConstKey_multiline, ecc_value_truth(self->multiline), ECC_VALFLAG_READONLY | ECC_VALFLAG_HIDDEN | ECC_VALFLAG_SEALED);
    ecc_object_addmember(&self->object, ECC_ConstKey_lastIndex, ecc_value_fromint(0), ECC_VALFLAG_HIDDEN | ECC_VALFLAG_SEALED);

    return self;
}

eccobjregexp_t* ecc_regexp_createwith(ecccontext_t* context, eccvalue_t pattern, eccvalue_t flags)
{
    eccobjerror_t* error = NULL;
    eccappendbuffer_t chars;
    eccobjregexp_t* regexp;
    eccvalue_t value;

    if(pattern.type == ECC_VALTYPE_REGEXP && flags.type == ECC_VALTYPE_UNDEFINED)
    {
        if(context->construct)
            value = ecc_value_fromchars(pattern.data.regexp->pattern);
        else
            return pattern.data.regexp;
    }
    else
    {
        ecc_strbuf_beginappend(&chars);

        ecc_strbuf_append(&chars, "/");

        if(pattern.type == ECC_VALTYPE_REGEXP)
            ecc_strbuf_appendvalue(&chars, context, ecc_value_fromchars(pattern.data.regexp->source));
        else
        {
            if(pattern.type == ECC_VALTYPE_UNDEFINED || (ecc_value_isstring(pattern) && !ecc_value_stringlength(&pattern)))
            {
                if(!context->ecc->sloppyMode)
                    ecc_strbuf_append(&chars, "(?:)");
            }
            else
                ecc_strbuf_appendvalue(&chars, context, pattern);
        }

        ecc_strbuf_append(&chars, "/");

        if(flags.type != ECC_VALTYPE_UNDEFINED)
            ecc_strbuf_appendvalue(&chars, context, flags);

        value = ecc_strbuf_endappend(&chars);
        if(value.type != ECC_VALTYPE_CHARS)
            value = ecc_value_fromchars(ecc_strbuf_createwithbytes(ecc_value_stringlength(&value), ecc_value_stringbytes(&value)));
    }

    assert(value.type == ECC_VALTYPE_CHARS);
    regexp = ecc_regexp_create(value.data.chars, &error, context->ecc->sloppyMode ? ECC_REGEXOPT_ALLOWUNICODEFLAGS : 0);
    if(error)
    {
        ecc_context_settextindex(context, ECC_CTXINDEXTYPE_NO);
        context->ecc->ofLine = 1;
        context->ecc->ofText = ecc_value_textof(&value);
        context->ecc->ofInput = "(RegExp)";
        ecc_context_throw(context, ecc_value_error(error));
    }
    return regexp;
}

int ecc_regexp_matchwithstate(eccobjregexp_t* self, eccrxstate_t* state)
{
    int result = 0;
    uint16_t index, count;
    ecctextstring_t text = ecc_textbuf_make(state->start, (int32_t)(state->end - state->start));

#if DUMP_REGEXP
    eccregexnode_t* n = self->program;
    while(n->opcode != ECC_RXOP_OVER)
        printNode(n++);
#endif

    do
    {
        memset(state->capture, 0, sizeof(*state->capture) * (self->count * 2));
        memset(state->index, 0, sizeof(*state->index) * (self->count * 2));
        state->capture[0] = state->index[0] = text.bytes;
        result = ecc_regexp_match(state, self->program, text);
        ecc_textbuf_nextcharacter(&text);
    } while(!result && text.length);

    /* XXX: cleanup */

    for(index = 0, count = ecc_regexp_nlen(self->program); index < count; ++index)
    {
        if(self->program[index].opcode == ECC_RXOP_SPLIT || self->program[index].opcode == ECC_RXOP_REFERENCE)
            self->program[index].bytes = NULL;

        self->program[index].depth = 0;
    }

    return result;
}
