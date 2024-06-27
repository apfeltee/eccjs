//
//  regexp.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//

#include "ecc.h"

#define DUMP_REGEXP 0

typedef enum eccrxopcode_t eccrxopcode_t;
typedef struct eccrxparser_t eccrxparser_t;

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
static eccregexnode_t *eccrx_node(eccrxopcode_t opcode, long offset, const char *bytes);
static void eccrx_toss(eccregexnode_t *node);
static uint16_t eccrx_nlen(eccregexnode_t *n);
static eccregexnode_t *eccrx_join(eccregexnode_t *a, eccregexnode_t *b);
static int eccrx_accept(eccrxparser_t *p, char c);
static eccrxopcode_t eccrx_escape(eccrxparser_t *p, int16_t *offset, char buffer[5]);
static eccrxopcode_t eccrx_character(ecctextstring_t text, int16_t *offset, char buffer[12], int ignoreCase);
static eccregexnode_t *eccrx_characternode(ecctextstring_t text, int ignoreCase);
static eccregexnode_t *eccrx_term(eccrxparser_t *p, eccobjerror_t **error);
static eccregexnode_t *eccrx_alternative(eccrxparser_t *p, eccobjerror_t **error);
static eccregexnode_t *eccrx_disjunction(eccrxparser_t *p, eccobjerror_t **error);
static eccregexnode_t *eccrx_pattern(eccrxparser_t *p, eccobjerror_t **error);
static void eccrx_clear(eccrxstate_t *const s, const char *c, uint8_t *bytes);
static int eccrx_forkmatch(eccrxstate_t *const s, eccregexnode_t *n, ecctextstring_t text, int16_t offset);
static int eccrx_match(eccrxstate_t *const s, eccregexnode_t *n, ecctextstring_t text);
static eccvalue_t objregexpfn_constructor(eccstate_t *context);
static eccvalue_t objregexpfn_toString(eccstate_t *context);
static eccvalue_t objregexpfn_exec(eccstate_t *context);
static eccvalue_t objregexpfn_test(eccstate_t *context);
static void nsregexpfn_setup(void);
static void nsregexpfn_teardown(void);
static eccobjregexp_t *nsregexpfn_create(ecccharbuffer_t *s, eccobjerror_t **error, eccrxoptions_t options);
static eccobjregexp_t *nsregexpfn_createWith(eccstate_t *context, eccvalue_t pattern, eccvalue_t flags);
static int nsregexpfn_matchWithState(eccobjregexp_t *self, eccrxstate_t *state);


const struct eccpseudonsregexp_t ECCNSRegExp = {
    nsregexpfn_setup, nsregexpfn_teardown, nsregexpfn_create, nsregexpfn_createWith, nsregexpfn_matchWithState,
    {}
};

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

    ECCNSMemoryPool.markValue(ECCNSValue.chars(self->pattern));
    ECCNSMemoryPool.markValue(ECCNSValue.chars(self->source));
}

static void regextypefn_capture(eccobject_t* object)
{
    eccobjregexp_t* self = (eccobjregexp_t*)object;
    ++self->pattern->referenceCount;
    ++self->source->referenceCount;
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
    --self->pattern->referenceCount;
    --self->source->referenceCount;
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

static eccregexnode_t* eccrx_node(eccrxopcode_t opcode, long offset, const char* bytes)
{
    eccregexnode_t* n = calloc(2, sizeof(*n));

    if(offset && bytes)
    {
        n[0].bytes = calloc(offset + 1, 1);
        memcpy(n[0].bytes, bytes, offset);
    }
    n[0].offset = offset;
    n[0].opcode = opcode;

    return n;
}

static void eccrx_toss(eccregexnode_t* node)
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

static uint16_t eccrx_nlen(eccregexnode_t* n)
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

static eccregexnode_t* eccrx_join(eccregexnode_t* a, eccregexnode_t* b)
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

        c->bytes = realloc(c->bytes, c->offset + b->offset + 1);
        memcpy(c->bytes + c->offset, b->bytes, b->offset + 1);
        c->offset += b->offset;
        free(b->bytes), b->bytes = NULL;
    }
    else
    {
        a = realloc(a, sizeof(*a) * (lena + lenb + 1));
        memcpy(a + lena, b, sizeof(*a) * (lenb + 1));
    }
    free(b), b = NULL;
    return a;
}

static int eccrx_accept(eccrxparser_t* p, char c)
{
    if(*p->c == c)
    {
        ++p->c;
        return 1;
    }
    return 0;
}

static eccrxopcode_t eccrx_escape(eccrxparser_t* p, int16_t* offset, char buffer[5])
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
                        *offset = ECCNSChars.writeCodepoint(buffer, ((uint8_t*)buffer)[0]);
                }
            }
            break;

        case 'x':
            {
                if(isxdigit(p->c[0]) && isxdigit(p->c[1]))
                {
                    *offset = ECCNSChars.writeCodepoint(buffer, ECCNSLexer.uint8Hex(p->c[0], p->c[1]));
                    p->c += 2;
                }
            }
            break;

        case 'u':
            {
                if(isxdigit(p->c[0]) && isxdigit(p->c[1]) && isxdigit(p->c[2]) && isxdigit(p->c[3]))
                {
                    *offset = ECCNSChars.writeCodepoint(buffer, ECCNSLexer.uint16Hex(p->c[0], p->c[1], p->c[2], p->c[3]));
                    p->c += 4;
                }
            }
            break;

        default:
            break;
    }

    return ECC_RXOP_BYTES;
}

static eccrxopcode_t eccrx_character(ecctextstring_t text, int16_t* offset, char buffer[12], int ignoreCase)
{
    if(ignoreCase)
    {
        char* split = ECCNSText.toLower(text, buffer);
        char* check = ECCNSText.toUpper(text, split);
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

static eccregexnode_t* eccrx_characternode(ecctextstring_t text, int ignoreCase)
{
    char buffer[12];
    int16_t offset;
    eccrxopcode_t opcode = eccrx_character(text, &offset, buffer, ignoreCase);

    return eccrx_node(opcode, offset, buffer);
}

static eccregexnode_t* eccrx_term(eccrxparser_t* p, eccobjerror_t** error)
{
    eccregexnode_t* n;
    ecctextstring_t text;

    p->disallowQuantifier = 0;

    if(p->c >= p->end - 1)
        return NULL;
    else if(eccrx_accept(p, '^'))
    {
        p->disallowQuantifier = 1;
        return eccrx_node(p->multiline ? ECC_RXOP_LINESTART : ECC_RXOP_START, 0, NULL);
    }
    else if(eccrx_accept(p, '$'))
    {
        p->disallowQuantifier = 1;
        return eccrx_node(p->multiline ? ECC_RXOP_LINEEND : ECC_RXOP_END, 0, NULL);
    }
    else if(eccrx_accept(p, '\\'))
    {
        switch(*p->c)
        {
            case 'b':
                ++p->c;
                p->disallowQuantifier = 1;
                return eccrx_node(ECC_RXOP_BOUNDARY, 1, NULL);

            case 'B':
                ++p->c;
                p->disallowQuantifier = 1;
                return eccrx_node(ECC_RXOP_BOUNDARY, 0, NULL);

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

                return eccrx_node(ECC_RXOP_REFERENCE, c, NULL);
            }

            default:
            {
                eccrxopcode_t opcode;
                int16_t offset;
                char buffer[5];

                opcode = eccrx_escape(p, &offset, buffer);
                if(opcode == ECC_RXOP_BYTES)
                {
                    text = ECCNSText.make(buffer, offset);
                    return eccrx_characternode(text, p->ignoreCase);
                }
                else
                    return eccrx_node(opcode, offset, NULL);
            }
        }
    }
    else if(eccrx_accept(p, '('))
    {
        unsigned char count = 0;
        char modifier = '\0';

        if(eccrx_accept(p, '?'))
        {
            if(*p->c == '=' || *p->c == '!' || *p->c == ':')
                modifier = *(p->c++);
        }
        else
        {
            count = ++p->count;
            if((int)count * 2 + 1 > 0xff)
            {
                *error = ECCNSError.syntaxError(ECCNSText.make(p->c, 1), ECCNSChars.create("too many captures"));
                return NULL;
            }
        }

        n = eccrx_disjunction(p, error);
        if(!eccrx_accept(p, ')'))
        {
            if(!*error)
                *error = ECCNSError.syntaxError(ECCNSText.make(p->c, 1), ECCNSChars.create("expect ')'"));

            return NULL;
        }

        switch(modifier)
        {
            case '\0':
                return eccrx_join(eccrx_node(ECC_RXOP_SAVE, count * 2, NULL), eccrx_join(n, eccrx_node(ECC_RXOP_SAVE, count * 2 + 1, NULL)));

            case '=':
            {
                int c = eccrx_nlen(n);
                p->disallowQuantifier = 1;
                return eccrx_join(eccrx_node(ECC_RXOP_LOOKAHEAD, c + 2, NULL), eccrx_join(n, eccrx_node(ECC_RXOP_MATCH, 0, NULL)));
            }

            case '!':
            {
                int i = 0, c = eccrx_nlen(n);
                p->disallowQuantifier = 1;
                for(; i < c; ++i)
                    if(n[i].opcode == ECC_RXOP_SAVE)
                        n[i].opcode = ECC_RXOP_NSAVE;

                return eccrx_join(eccrx_node(ECC_RXOP_NLOOKAHEAD, c + 2, NULL), eccrx_join(n, eccrx_node(ECC_RXOP_MATCH, 0, NULL)));
            }
            case ':':
                return n;
        }
    }
    else if(eccrx_accept(p, '.'))
        return eccrx_node(ECC_RXOP_ANY, 0, NULL);
    else if(eccrx_accept(p, '['))
    {
        eccrxopcode_t opcode;
        int16_t offset;

        int not = eccrx_accept(p, '^'), length = 0, lastLength, range = -1;
        char buffer[255];
        n = NULL;

        while(*p->c != ']' || range >= 0)
        {
            lastLength = length;

            if(eccrx_accept(p, '\\'))
            {
                opcode = eccrx_escape(p, &offset, buffer + length);
                if(opcode == ECC_RXOP_BYTES)
                    length += offset;
                else
                {
                    if(not )
                        n = eccrx_join(eccrx_node(ECC_RXOP_NLOOKAHEAD, 3, NULL), eccrx_join(eccrx_node(opcode, offset, NULL), eccrx_join(eccrx_node(ECC_RXOP_MATCH, 0, NULL), n)));
                    else
                        n = eccrx_join(eccrx_node(ECC_RXOP_SPLIT, 3, NULL), eccrx_join(eccrx_node(opcode, offset, NULL), eccrx_join(eccrx_node(ECC_RXOP_JUMP, eccrx_nlen(n) + 2, NULL), n)));
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
                    ecctextstring_t innertxt = ECCNSText.make(buffer + range, length - range);
                    ecctextchar_t from, to;

                    from = ECCNSText.nextCharacter(&innertxt);
                    ECCNSText.advance(&innertxt, 1);
                    to = ECCNSText.nextCharacter(&innertxt);

                    if(from.codepoint > to.codepoint)
                    {
                        eccrx_toss(n);
                        *error = ECCNSError.syntaxError(ECCNSText.make(p->c - length - range, length - range),
                                                             ECCNSChars.create("range out of order in character class"));
                        return NULL;
                    }

                    if(not )
                        n = eccrx_join(eccrx_node(ECC_RXOP_NLOOKAHEAD, 3, NULL),
                                 eccrx_join(eccrx_node(p->ignoreCase ? ECC_RXOP_INRANGECASE : ECC_RXOP_INRANGE, length - range, buffer + range), eccrx_join(eccrx_node(ECC_RXOP_MATCH, 0, NULL), n)));
                    else
                        n = eccrx_join(eccrx_node(ECC_RXOP_SPLIT, 3, NULL),
                                 eccrx_join(eccrx_node(p->ignoreCase ? ECC_RXOP_INRANGECASE : ECC_RXOP_INRANGE, length - range, buffer + range), eccrx_join(eccrx_node(ECC_RXOP_JUMP, eccrx_nlen(n) + 2, NULL), n)));

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
                *error = ECCNSError.syntaxError(ECCNSText.make(p->c - 1, 1), ECCNSChars.create("expect ']'"));
                return NULL;
            }
        }
        if(p->ignoreCase)
        {
            char casebuffer[6];
            ecctextstring_t single;
            ecctextchar_t c;
            text = ECCNSText.make(buffer + length, length);

            while(text.length)
            {
                c = ECCNSText.prevCharacter(&text);
                single = text;
                single.length = c.units;

                offset = ECCNSText.toLower(single, casebuffer) - casebuffer;
                if(memcmp(text.bytes, casebuffer, offset))
                {
                    memcpy(buffer + length, casebuffer, offset);
                    length += offset;
                }

                offset = ECCNSText.toUpper(single, casebuffer) - casebuffer;
                if(memcmp(text.bytes, casebuffer, offset))
                {
                    memcpy(buffer + length, casebuffer, offset);
                    length += offset;
                }
            }
        }

        buffer[length] = '\0';
        eccrx_accept(p, ']');
        return eccrx_join(n, eccrx_node(not ? ECC_RXOP_NEITHEROF : ECC_RXOP_ONEOF, length, buffer));
    }
    else if(*p->c && strchr("*+?)}|", *p->c))
        return NULL;

    text = ECCNSText.make(p->c, (int32_t)(p->end - p->c));
    text.length = ECCNSText.character(text).units;
    p->c += text.length;
    return eccrx_characternode(text, p->ignoreCase);
}

static eccregexnode_t* eccrx_alternative(eccrxparser_t* p, eccobjerror_t** error)
{
    eccregexnode_t *n = NULL, *t = NULL;

    for(;;)
    {
        if(!(t = eccrx_term(p, error)))
            break;

        if(!p->disallowQuantifier)
        {
            int noop = 0, lazy = 0;

            uint8_t quantifier = eccrx_accept(p, '?') ? '?' : eccrx_accept(p, '*') ? '*' : eccrx_accept(p, '+') ? '+' : eccrx_accept(p, '{') ? '{' : '\0', min = 1, max = 1;

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
                        *error = ECCNSError.syntaxError(ECCNSText.make(p->c, 1), ECCNSChars.create("expect number"));
                        goto error;
                    }

                    if(eccrx_accept(p, ','))
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

                    if(!eccrx_accept(p, '}'))
                    {
                        *error = ECCNSError.syntaxError(ECCNSText.make(p->c, 1), ECCNSChars.create("expect '}'"));
                        goto error;
                    }
                    break;
                }
            }

            lazy = eccrx_accept(p, '?');
            if(noop)
            {
                eccrx_toss(t);
                continue;
            }

            if(max != 1)
            {
                eccregexnode_t* redo;
                uint16_t index, count = eccrx_nlen(t) - (lazy ? 1 : 0), length = 3;
                char buffer[count + length];

                for(index = (lazy ? 1 : 0); index < count; ++index)
                    if(t[index].opcode == ECC_RXOP_SAVE)
                        buffer[length++] = (uint8_t)t[index].offset;

                buffer[0] = min;
                buffer[1] = max;
                buffer[2] = lazy;
                buffer[length] = '\0';

                redo = eccrx_node(ECC_RXOP_REDO, -eccrx_nlen(t), NULL);
                redo->bytes = malloc(length + 1);
                memcpy(redo->bytes, buffer, length + 1);

                t = eccrx_join(t, redo);
            }

            if(min == 0)
            {
                if(lazy)
                    t = eccrx_join(eccrx_node(ECC_RXOP_SPLIT, 2, NULL), eccrx_join(eccrx_node(ECC_RXOP_JUMP, eccrx_nlen(t) + 1, NULL), t));
                else
                    t = eccrx_join(eccrx_node(ECC_RXOP_SPLIT, eccrx_nlen(t) + 1, NULL), t);
            }
        }
        n = eccrx_join(n, t);
    }
    return n;

error:
    eccrx_toss(t);
    return n;
}

static eccregexnode_t* eccrx_disjunction(eccrxparser_t* p, eccobjerror_t** error)
{
    eccregexnode_t *n = eccrx_alternative(p, error), *d;
    if(eccrx_accept(p, '|'))
    {
        uint16_t len;
        d = eccrx_disjunction(p, error);
        n = eccrx_join(n, eccrx_node(ECC_RXOP_JUMP, eccrx_nlen(d) + 1, NULL));
        len = eccrx_nlen(n);
        n = eccrx_join(n, d);
        n = eccrx_join(eccrx_node(ECC_RXOP_SPLIT, len + 1, NULL), n);
    }
    return n;
}

static eccregexnode_t* eccrx_pattern(eccrxparser_t* p, eccobjerror_t** error)
{
    assert(*p->c == '/');
    assert(*(p->end - 1) == '/');

    ++p->c;
    return eccrx_join(eccrx_disjunction(p, error), eccrx_node(ECC_RXOP_MATCH, 0, NULL));
}

static void eccrx_clear(eccrxstate_t* const s, const char* c, uint8_t* bytes)
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

static int eccrx_forkmatch(eccrxstate_t* const s, eccregexnode_t* n, ecctextstring_t text, int16_t offset)
{
    int result;

    if(n->depth == 0xff)
        return 0;//!\\ too many recursion
    else
        ++n->depth;

    result = eccrx_match(s, n + offset, text);
    --n->depth;
    return result;
}

static int eccrx_match(eccrxstate_t* const s, eccregexnode_t* n, ecctextstring_t text)
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
            if(eccrx_forkmatch(s, n, text, 1) == (n->opcode - 1))
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
            ecctextstring_t prev = ECCNSText.make(text.bytes, (int32_t)(text.bytes - s->start));
            if(text.bytes != s->start && !ECCNSText.isLineFeed(ECCNSText.prevCharacter(&prev)))
                return 0;

            goto next;
        }

        case ECC_RXOP_LINEEND:
            if(text.bytes != s->end && !ECCNSText.isLineFeed(ECCNSText.character(text)))
                return 0;

            goto next;

        case ECC_RXOP_BOUNDARY:
        {
            ecctextstring_t prev = ECCNSText.make(text.bytes, (int32_t)(text.bytes - s->start));
            if(text.bytes == s->start)
            {
                if(ECCNSText.isWord(ECCNSText.character(text)) != n->offset)
                    return 0;
            }
            else if(text.bytes == s->end)
            {
                if(ECCNSText.isWord(ECCNSText.prevCharacter(&prev)) != n->offset)
                    return 0;
            }
            else
            {
                if((ECCNSText.isWord(ECCNSText.prevCharacter(&prev)) != ECCNSText.isWord(ECCNSText.character(text))) != n->offset)
                    return 0;
            }
            goto next;
        }

        case ECC_RXOP_SPLIT:
            if(eccrx_forkmatch(s, n, text, 1))
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

                ECCNSText.advance(&text, len);
            }
            goto next;
        }

        case ECC_RXOP_REDO:
        {
            int hasmin = n->depth + 1 >= n->bytes[0];
            int lazy = n->bytes[2] && hasmin;

            if(n->bytes[1] && n->depth >= n->bytes[1])
                return 0;

            if(eccrx_forkmatch(s, n, text, lazy ? 1 : n->offset))
            {
                eccrx_clear(s, text.bytes, (uint8_t*)n->bytes + 3);
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
            if(eccrx_forkmatch(s, n, text, 1))
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
            if(!eccrx_forkmatch(s, n, text, 1))
            {
                s->capture[n->offset] = NULL;
                return 0;
            }
            s->capture[n->offset] = capture;
            return 1;
        }

        case ECC_RXOP_DIGIT:
            if(text.length < 1 || ECCNSText.isDigit(ECCNSText.nextCharacter(&text)) != n->offset)
                return 0;

            goto next;

        case ECC_RXOP_SPACE:
            if(text.length < 1 || ECCNSText.isSpace(ECCNSText.nextCharacter(&text)) != n->offset)
                return 0;

            goto next;

        case ECC_RXOP_WORD:
            if(text.length < 1 || ECCNSText.isWord(ECCNSText.nextCharacter(&text)) != n->offset)
                return 0;

            goto next;

        case ECC_RXOP_BYTES:
            if(text.length < n->offset || memcmp(n->bytes, text.bytes, n->offset))
                return 0;

            ECCNSText.advance(&text, n->offset);
            goto next;

        case ECC_RXOP_ONEOF:
        {
            char buffer[5];
            ecctextchar_t c;

            if(!text.length)
                return 0;

            c = ECCNSText.character(text);
            memcpy(buffer, text.bytes, c.units);
            buffer[c.units] = '\0';

            if(n->bytes && strstr(n->bytes, buffer))
            {
                ECCNSText.nextCharacter(&text);
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

            c = ECCNSText.character(text);
            memcpy(buffer, text.bytes, c.units);
            buffer[c.units] = '\0';

            if(n->bytes && strstr(n->bytes, buffer))
                return 0;

            ECCNSText.nextCharacter(&text);
            goto next;
        }

        case ECC_RXOP_INRANGE:
        case ECC_RXOP_INRANGECASE:
        {
            ecctextstring_t range = ECCNSText.make(n->bytes, n->offset);
            ecctextchar_t from, to, c;

            if(!text.length)
                return 0;

            from = ECCNSText.nextCharacter(&range);
            ECCNSText.advance(&range, 1);
            to = ECCNSText.nextCharacter(&range);
            c = ECCNSText.character(text);

            if(n->opcode == ECC_RXOP_INRANGECASE)
            {
                char buffer[c.units];
                ecctextstring_t casetext = ECCNSText.make(buffer, 0);

                casetext.length = (int32_t)(ECCNSText.toLower(ECCNSText.make(text.bytes, (int32_t)sizeof(buffer)), buffer) - buffer);
                c = ECCNSText.character(casetext);
                if(c.units == casetext.length && (c.codepoint >= from.codepoint && c.codepoint <= to.codepoint))
                {
                    ECCNSText.nextCharacter(&text);
                    goto next;
                }

                casetext.length = (int32_t)(ECCNSText.toUpper(ECCNSText.make(text.bytes, (int32_t)sizeof(buffer)), buffer) - buffer);
                c = ECCNSText.character(casetext);
                if(c.units == casetext.length && (c.codepoint >= from.codepoint && c.codepoint <= to.codepoint))
                {
                    ECCNSText.nextCharacter(&text);
                    goto next;
                }
            }
            else
            {
                if((c.codepoint >= from.codepoint && c.codepoint <= to.codepoint))
                {
                    ECCNSText.nextCharacter(&text);
                    goto next;
                }
            }

            return 0;
        }

        case ECC_RXOP_ANY:
            if(text.length >= 1 && !ECCNSText.isLineFeed(ECCNSText.nextCharacter(&text)))
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

static eccvalue_t objregexpfn_constructor(eccstate_t* context)
{
    eccvalue_t pattern, flags;

    pattern = ECCNSContext.argument(context, 0);
    flags = ECCNSContext.argument(context, 1);

    return ECCNSValue.regexp(nsregexpfn_createWith(context, pattern, flags));
}

static eccvalue_t objregexpfn_toString(eccstate_t* context)
{
    eccobjregexp_t* self = context->thisvalue.data.regexp;

    ECCNSContext.assertThisType(context, ECC_VALTYPE_REGEXP);

    return ECCNSValue.chars(self->pattern);
}

static eccvalue_t objregexpfn_exec(eccstate_t* context)
{
    eccobjregexp_t* self = context->thisvalue.data.regexp;
    eccvalue_t value, lastIndex;

    ECCNSContext.assertThisType(context, ECC_VALTYPE_REGEXP);

    value = ECCNSValue.toString(context, ECCNSContext.argument(context, 0));
    lastIndex = self->global ? ECCNSValue.toInteger(context, ECCNSObject.getMember(context, &self->object, ECC_ConstKey_lastIndex)) : ECCNSValue.integer(0);

    ECCNSObject.putMember(context, &self->object, ECC_ConstKey_lastIndex, ECCNSValue.integer(0));

    if(lastIndex.data.integer >= 0)
    {
        uint16_t length = ECCNSValue.stringLength(&value);
        const char* bytes = ECCNSValue.stringBytes(&value);
        const char* capture[self->count * 2];
        const char* strindex[self->count * 2];
        ecccharbuffer_t* element;

        eccrxstate_t state = { ECCNSString.textAtIndex(bytes, length, lastIndex.data.integer, 0).bytes, bytes + length, capture, strindex, 0};

        if(state.start >= bytes && state.start <= state.end && nsregexpfn_matchWithState(self, &state))
        {
            eccobject_t* array = ECCNSArray.createSized(self->count);
            int32_t numindex, count;

            for(numindex = 0, count = self->count; numindex < count; ++numindex)
            {
                if(capture[numindex * 2])
                {
                    element = ECCNSChars.createWithBytes((int32_t)(capture[numindex * 2 + 1] - capture[numindex * 2]), capture[numindex * 2]);
                    array->element[numindex].value = ECCNSValue.chars(element);
                }
                else
                    array->element[numindex].value = ECCValConstUndefined;
            }

            if(self->global)
                ECCNSObject.putMember(context, &self->object, ECC_ConstKey_lastIndex,
                                      ECCNSValue.integer(ECCNSString.unitIndex(bytes, length, (int32_t)(capture[1] - bytes))));

            ECCNSObject.addMember(array, ECC_ConstKey_index, ECCNSValue.integer(ECCNSString.unitIndex(bytes, length, (int32_t)(capture[0] - bytes))), 0);
            ECCNSObject.addMember(array, ECC_ConstKey_input, value, 0);

            return ECCNSValue.object(array);
        }
    }
    return ECCValConstNull;
}

static eccvalue_t objregexpfn_test(eccstate_t* context)
{
    eccobjregexp_t* self = context->thisvalue.data.regexp;
    eccvalue_t value, lastIndex;

    ECCNSContext.assertThisType(context, ECC_VALTYPE_REGEXP);

    value = ECCNSValue.toString(context, ECCNSContext.argument(context, 0));
    lastIndex = ECCNSValue.toInteger(context, ECCNSObject.getMember(context, &self->object, ECC_ConstKey_lastIndex));

    ECCNSObject.putMember(context, &self->object, ECC_ConstKey_lastIndex, ECCNSValue.integer(0));

    if(lastIndex.data.integer >= 0)
    {
        uint16_t length = ECCNSValue.stringLength(&value);
        const char* bytes = ECCNSValue.stringBytes(&value);
        const char* capture[self->count * 2];
        const char* index[self->count * 2];

        eccrxstate_t state = { ECCNSString.textAtIndex(bytes, length, lastIndex.data.integer, 0).bytes, bytes + length, capture, index, 0};

        if(state.start >= bytes && state.start <= state.end && nsregexpfn_matchWithState(self, &state))
        {
            if(self->global)
                ECCNSObject.putMember(context, &self->object, ECC_ConstKey_lastIndex,
                                      ECCNSValue.integer(ECCNSString.unitIndex(bytes, length, (int32_t)(capture[1] - bytes))));

            return ECCValConstTrue;
        }
    }
    return ECCValConstFalse;
}

// MARK: - Methods

static void nsregexpfn_setup()
{
    eccobjerror_t* error = NULL;
    const eccvalflag_t h = ECC_VALFLAG_HIDDEN;

    ECCNSFunction.setupBuiltinObject(&ECC_CtorFunc_Regexp, objregexpfn_constructor, 2, &ECC_Prototype_Regexp,
                                          ECCNSValue.regexp(nsregexpfn_create(ECCNSChars.create("/(?:)/"), &error, 0)), &ECC_Type_Regexp);

    assert(error == NULL);

    ECCNSFunction.addToObject(ECC_Prototype_Regexp, "toString", objregexpfn_toString, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_Regexp, "exec", objregexpfn_exec, 1, h);
    ECCNSFunction.addToObject(ECC_Prototype_Regexp, "test", objregexpfn_test, 1, h);
}

static void nsregexpfn_teardown(void)
{
    ECC_Prototype_Regexp = NULL;
    ECC_CtorFunc_Regexp = NULL;
}

static eccobjregexp_t* nsregexpfn_create(ecccharbuffer_t* s, eccobjerror_t** error, eccrxoptions_t options)
{
    eccrxparser_t p = { 0 };

    eccobjregexp_t* self = malloc(sizeof(*self));
    *self = ECCNSRegExp.identity;
    ECCNSMemoryPool.addObject(&self->object);

    ECCNSObject.initialize(&self->object, ECC_Prototype_Regexp);

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
    self->program = eccrx_pattern(&p, error);
    self->count = p.count + 1;
    self->source = ECCNSChars.createWithBytes((int32_t)(p.c - self->pattern->bytes - 1), self->pattern->bytes + 1);

    //	++self->pattern->referenceCount;
    //	++self->source->referenceCount;

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
                            *error = ECCNSError.syntaxError(ECCNSText.make(p.c, 1), ECCNSChars.create("invalid flag"));
                        }
                        self->global = 1;
                    }
                    continue;

                case 'i':
                    {
                    i:
                        if(self->ignoreCase == 1)
                        {
                            *error = ECCNSError.syntaxError(ECCNSText.make(p.c, 1), ECCNSChars.create("invalid flag"));
                        }
                        self->ignoreCase = 1;
                    }
                    continue;

                case 'm':
                    {
                    m:
                        if(self->multiline == 1)
                        {
                            *error = ECCNSError.syntaxError(ECCNSText.make(p.c, 1), ECCNSChars.create("invalid flag"));
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
                        *error = ECCNSError.syntaxError(ECCNSText.make(p.c, 1), ECCNSChars.create("invalid flag"));
                    }
            }
            break;
        }
    }
    else if(error && !*error)
    {
        *error = ECCNSError.syntaxError(ECCNSText.make(p.c, 1), ECCNSChars.create("invalid character '%c'", isgraph(*p.c) ? *p.c : '?'));
    }
    ECCNSObject.addMember(&self->object, ECC_ConstKey_source, ECCNSValue.chars(self->source), ECC_VALFLAG_READONLY | ECC_VALFLAG_HIDDEN | ECC_VALFLAG_SEALED);
    ECCNSObject.addMember(&self->object, ECC_ConstKey_global, ECCNSValue.truth(self->global), ECC_VALFLAG_READONLY | ECC_VALFLAG_HIDDEN | ECC_VALFLAG_SEALED);
    ECCNSObject.addMember(&self->object, ECC_ConstKey_ignoreCase, ECCNSValue.truth(self->ignoreCase), ECC_VALFLAG_READONLY | ECC_VALFLAG_HIDDEN | ECC_VALFLAG_SEALED);
    ECCNSObject.addMember(&self->object, ECC_ConstKey_multiline, ECCNSValue.truth(self->multiline), ECC_VALFLAG_READONLY | ECC_VALFLAG_HIDDEN | ECC_VALFLAG_SEALED);
    ECCNSObject.addMember(&self->object, ECC_ConstKey_lastIndex, ECCNSValue.integer(0), ECC_VALFLAG_HIDDEN | ECC_VALFLAG_SEALED);

    return self;
}

static eccobjregexp_t* nsregexpfn_createWith(eccstate_t* context, eccvalue_t pattern, eccvalue_t flags)
{
    eccobjerror_t* error = NULL;
    eccappendbuffer_t chars;
    eccobjregexp_t* regexp;
    eccvalue_t value;

    if(pattern.type == ECC_VALTYPE_REGEXP && flags.type == ECC_VALTYPE_UNDEFINED)
    {
        if(context->construct)
            value = ECCNSValue.chars(pattern.data.regexp->pattern);
        else
            return pattern.data.regexp;
    }
    else
    {
        ECCNSChars.beginAppend(&chars);

        ECCNSChars.append(&chars, "/");

        if(pattern.type == ECC_VALTYPE_REGEXP)
            ECCNSChars.appendValue(&chars, context, ECCNSValue.chars(pattern.data.regexp->source));
        else
        {
            if(pattern.type == ECC_VALTYPE_UNDEFINED || (ECCNSValue.isString(pattern) && !ECCNSValue.stringLength(&pattern)))
            {
                if(!context->ecc->sloppyMode)
                    ECCNSChars.append(&chars, "(?:)");
            }
            else
                ECCNSChars.appendValue(&chars, context, pattern);
        }

        ECCNSChars.append(&chars, "/");

        if(flags.type != ECC_VALTYPE_UNDEFINED)
            ECCNSChars.appendValue(&chars, context, flags);

        value = ECCNSChars.endAppend(&chars);
        if(value.type != ECC_VALTYPE_CHARS)
            value = ECCNSValue.chars(ECCNSChars.createWithBytes(ECCNSValue.stringLength(&value), ECCNSValue.stringBytes(&value)));
    }

    assert(value.type == ECC_VALTYPE_CHARS);
    regexp = nsregexpfn_create(value.data.chars, &error, context->ecc->sloppyMode ? ECC_REGEXOPT_ALLOWUNICODEFLAGS : 0);
    if(error)
    {
        ECCNSContext.setTextIndex(context, ECC_CTXINDEXTYPE_NO);
        context->ecc->ofLine = 1;
        context->ecc->ofText = ECCNSValue.textOf(&value);
        context->ecc->ofInput = "(ECCNSRegExp)";
        ECCNSContext.doThrow(context, ECCNSValue.error(error));
    }
    return regexp;
}

static int nsregexpfn_matchWithState(eccobjregexp_t* self, eccrxstate_t* state)
{
    int result = 0;
    uint16_t index, count;
    ecctextstring_t text = ECCNSText.make(state->start, (int32_t)(state->end - state->start));

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
        result = eccrx_match(state, self->program, text);
        ECCNSText.nextCharacter(&text);
    } while(!result && text.length);

    /* XXX: cleanup */

    for(index = 0, count = eccrx_nlen(self->program); index < count; ++index)
    {
        if(self->program[index].opcode == ECC_RXOP_SPLIT || self->program[index].opcode == ECC_RXOP_REFERENCE)
            self->program[index].bytes = NULL;

        self->program[index].depth = 0;
    }

    return result;
}
