
/*
//  lexer.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
*/

#include "ecc.h"

#define eccmac_stringandlen(str) str, sizeof(str) - 1

static const struct
{
    const char* name;
    size_t length;
    eccasttoktype_t token;
} g_lexkeyidents[] = {
    { eccmac_stringandlen("break"), ECC_TOK_BREAK },
    { eccmac_stringandlen("case"), ECC_TOK_CASE },
    { eccmac_stringandlen("catch"), ECC_TOK_CATCH },
    { eccmac_stringandlen("continue"), ECC_TOK_CONTINUE },
    { eccmac_stringandlen("debugger"), ECC_TOK_DEBUGGER },
    { eccmac_stringandlen("default"), ECC_TOK_DEFAULT },
    { eccmac_stringandlen("delete"), ECC_TOK_DELETE },
    { eccmac_stringandlen("do"), ECC_TOK_DO },
    { eccmac_stringandlen("else"), ECC_TOK_ELSE },
    { eccmac_stringandlen("finally"), ECC_TOK_FINALLY },
    { eccmac_stringandlen("for"), ECC_TOK_FOR },
    { eccmac_stringandlen("function"), ECC_TOK_FUNCTION },
    { eccmac_stringandlen("if"), ECC_TOK_IF },
    { eccmac_stringandlen("in"), ECC_TOK_IN },
    { eccmac_stringandlen("instanceof"), ECC_TOK_INSTANCEOF },
    { eccmac_stringandlen("new"), ECC_TOK_NEW },
    { eccmac_stringandlen("return"), ECC_TOK_RETURN },
    { eccmac_stringandlen("switch"), ECC_TOK_SWITCH },
    { eccmac_stringandlen("typeof"), ECC_TOK_TYPEOF },
    { eccmac_stringandlen("throw"), ECC_TOK_THROW },
    { eccmac_stringandlen("try"), ECC_TOK_TRY },
    { eccmac_stringandlen("var"), ECC_TOK_VAR },
    { eccmac_stringandlen("void"), ECC_TOK_VOID },
    { eccmac_stringandlen("while"), ECC_TOK_WHILE },
    { eccmac_stringandlen("with"), ECC_TOK_WITH },
    { eccmac_stringandlen("void"), ECC_TOK_VOID },
    { eccmac_stringandlen("typeof"), ECC_TOK_TYPEOF },
    { eccmac_stringandlen("null"), ECC_TOK_NULL },
    { eccmac_stringandlen("true"), ECC_TOK_TRUE },
    { eccmac_stringandlen("false"), ECC_TOK_FALSE },
    { eccmac_stringandlen("this"), ECC_TOK_THIS },
    {NULL, 0, (eccasttoktype_t)0},
};

static const struct
{
    const char* name;
    size_t length;
} g_lexreservedidents[] = {
    { eccmac_stringandlen("class") },
    { eccmac_stringandlen("enum") },
    { eccmac_stringandlen("extends") },
    { eccmac_stringandlen("super") },
    { eccmac_stringandlen("const") },
    { eccmac_stringandlen("export") },
    { eccmac_stringandlen("import") },
    { eccmac_stringandlen("implements") },
    { eccmac_stringandlen("let") },
    { eccmac_stringandlen("private") },
    { eccmac_stringandlen("public") },
    { eccmac_stringandlen("interface") },
    { eccmac_stringandlen("package") },
    { eccmac_stringandlen("protected") },
    { eccmac_stringandlen("static") },
    { eccmac_stringandlen("yield") },
    { NULL, 0},
};

int8_t ecc_astlex_hexhigit(int b)
{
    if(b >= 'a' && b <= 'f')
    {
        return b - 'a' + 10;
    }
    else if(b >= 'A' && b <= 'F')
    {
        return b - 'A' + 10;
    }
    return b - '0';
}

uint8_t ecc_astlex_uint8hex(char a, char b)
{
    return ecc_astlex_hexhigit(a) << 4 | ecc_astlex_hexhigit(b);
}

uint32_t ecc_astlex_uint16hex(char a, char b, char c, char d)
{
    return ecc_astlex_hexhigit(a) << 12 | ecc_astlex_hexhigit(b) << 8 | ecc_astlex_hexhigit(c) << 4 | ecc_astlex_hexhigit(d);
}

double ecc_astlex_strtolhexfallback(eccstrbox_t text)
{
    double dblval = 0;
    int sign = 1;
    int offset = 0;
    int c;
    if(text.bytes[offset] == '-')
    {
        sign = -1;
        ++offset;
    }
    if(text.length - offset > 1 && text.bytes[offset] == '0' && tolower(text.bytes[offset + 1]) == 'x')
    {
        offset += 2;
        while(text.length - offset >= 1)
        {
            c = text.bytes[offset++];
            dblval *= 16;
            if(isdigit(c))
            {
                dblval += c - '0';
            }
            else if(isxdigit(c))
            {
                dblval += tolower(c) - ('a' - 10);
            }
            else
            {
                return ECC_CONST_NAN;
            }
        }
    }
    return dblval * sign;
}

void ecc_astlex_addline(eccastlexer_t* self, uint32_t offset)
{
    size_t needed;
    uint32_t* tmp;
    if(self->input->lineCount + 1 >= self->input->lineCapacity)
    {
        self->input->lineCapacity *= 2;
        needed = sizeof(*self->input->lines) * self->input->lineCapacity;
        if(needed == 0)
        {
            needed += 8;
        }
        tmp = (uint32_t*)realloc(self->input->lines, needed);
        if(tmp == NULL)
        {
            fprintf(stderr, "in addline: failed to reallocate for %ld bytes\n", needed);
        }
        self->input->lines = tmp;
    }
    self->input->lines[++self->input->lineCount] = offset;
}

unsigned char ecc_astlex_previewchar(eccastlexer_t* self)
{
    if(self->offset < self->input->length)
        return self->input->bytes[self->offset];
    else
        return 0;
}

uint32_t ecc_astlex_nextchar(eccastlexer_t* self)
{
    if(self->offset < self->input->length)
    {
        eccrune_t c = ecc_strbox_character(ecc_strbox_make(self->input->bytes + self->offset, self->input->length - self->offset));

        self->offset += c.units;

        if((self->permitutfoutsidelit && ecc_strbox_islinefeed(c)) || (c.codepoint == '\r' && ecc_astlex_previewchar(self) != '\n') || c.codepoint == '\n')
        {
            self->stdidlinebreak = 1;
            ecc_astlex_addline(self, self->offset);
            c.codepoint = '\n';
        }
        else if(self->permitutfoutsidelit && ecc_strbox_isspace(c))
            c.codepoint = ' ';

        self->text.length += c.units;
        return c.codepoint;
    }
    else
        return 0;
}

int ecc_astlex_acceptchar(eccastlexer_t* self, char c)
{
    if(ecc_astlex_previewchar(self) == c)
    {
        ecc_astlex_nextchar(self);
        return 1;
    }
    else
        return 0;
}

char ecc_astlex_eof(eccastlexer_t* self)
{
    return self->offset >= self->input->length;
}

int ecc_astlex_syntaxerror(eccastlexer_t* self, eccstrbuffer_t* message)
{
    eccobjerror_t* error = ecc_error_syntaxerror(self->text, message);
    self->tokenvalue = ecc_value_error(error);
    return ECC_TOK_ERROR;
}

eccastlexer_t* ecc_astlex_createwithinput(eccioinput_t* input)
{
    eccastlexer_t* self = (eccastlexer_t*)malloc(sizeof(*self));
    memset(self, 0, sizeof(eccastlexer_t));

    assert(input);
    self->input = input;

    return self;
}

void ecc_astlex_destroy(eccastlexer_t* self)
{
    assert(self);

    self->input = NULL;
    free(self), self = NULL;
}

int ecc_astlex_nexttoken(eccastlexer_t* self)
{
    int k;
    int rxclit;
    int haveesc;
    int didLineBreak;
    int ubi1;
    int ubi2;
    int ubi3;
    int ubi4;
    int isb80;
    uint32_t index;
    uint32_t currch;
    uint32_t esclength;
    size_t kidlen;
    char end;
    char ubc0;
    char ubc1;
    char ubc2;
    char ubc3;
    char ubc4;
    const char* kidstr;
    const char* escbytes;
    eccappbuf_t chars;
    eccappbuf_t esccharbuf;
    eccstrbox_t escsubtext;
    eccstrbox_t identxt;
    eccvalue_t escvalue;
    assert(self);
    self->tokenvalue = ECCValConstUndefined;
    self->stdidlinebreak = 0;
retry:
    self->text.bytes = self->input->bytes + self->offset;
    self->text.length = 0;

    while((currch = ecc_astlex_nextchar(self)))
    {
        switch(currch)
        {
            case '\n':
            case '\r':
            case '\t':
            case '\v':
            case '\f':
            case ' ':
                {
                    goto retry;
                }
                break;
            case '/':
                {
                    if(ecc_astlex_acceptchar(self, '*'))
                    {
                        while(!ecc_astlex_eof(self))
                        {
                            if(ecc_astlex_nextchar(self) == '*' && ecc_astlex_acceptchar(self, '/'))
                            {
                                goto retry;
                            }
                        }
                        return ecc_astlex_syntaxerror(self, ecc_strbuf_create("unterminated comment"));
                    }
                    else if(ecc_astlex_previewchar(self) == '/')
                    {
                        while((currch = ecc_astlex_nextchar(self)))
                        {
                            if(currch == '\r' || currch == '\n')
                            {
                                goto retry;
                            }
                        }
                        return 0;
                    }
                    else if(self->allowRegex)
                    {
                        while(!ecc_astlex_eof(self))
                        {
                            rxclit = ecc_astlex_nextchar(self);
                            if(rxclit == '\\')
                            {
                                rxclit = ecc_astlex_nextchar(self);
                            }
                            else if(rxclit == '/')
                            {
                                while(isalnum(ecc_astlex_previewchar(self)) || ecc_astlex_previewchar(self) == '\\')
                                {
                                    ecc_astlex_nextchar(self);
                                }
                                return ECC_TOK_REGEXP;
                            }
                            if(rxclit == '\n')
                            {
                                break;
                            }
                        }
                        return ecc_astlex_syntaxerror(self, ecc_strbuf_create("unterminated regexp literal"));
                    }
                    else if(ecc_astlex_acceptchar(self, '='))
                    {
                        return ECC_TOK_DIVIDEASSIGN;
                    }
                    else
                    {
                        return '/';
                    }
                }
                break;
            case '\'':
            case '"':
                {
                    end = currch;
                    haveesc = 0;
                    didLineBreak = self->stdidlinebreak;
                    while((currch = ecc_astlex_nextchar(self)))
                    {
                        if(currch == '\\')
                        {
                            haveesc = 1;
                            ecc_astlex_nextchar(self);
                            self->stdidlinebreak = didLineBreak;
                        }
                        else if(currch == (uint32_t)end)
                        {
                            escbytes = self->text.bytes++;
                            esclength = self->text.length -= 2;
                            if(haveesc)
                            {
                                ++escbytes;
                                --esclength;
                                ecc_strbuf_beginappend(&chars);
                                for(index = 0; index <= esclength; ++index)
                                {
                                    if(escbytes[index] == '\\' && escbytes[++index] != '\\')
                                    {
                                        switch(escbytes[index])
                                        {
                                            case '0':
                                                ecc_strbuf_appendcodepoint(&chars, '\0');
                                                break;
                                            case 'b':
                                                ecc_strbuf_appendcodepoint(&chars, '\b');
                                                break;
                                            case 'f':
                                                ecc_strbuf_appendcodepoint(&chars, '\f');
                                                break;
                                            case 'n':
                                                ecc_strbuf_appendcodepoint(&chars, '\n');
                                                break;
                                            case 'r':
                                                ecc_strbuf_appendcodepoint(&chars, '\r');
                                                break;
                                            case 't':
                                                ecc_strbuf_appendcodepoint(&chars, '\t');
                                                break;
                                            case 'v':
                                                ecc_strbuf_appendcodepoint(&chars, '\v');
                                                break;
                                            case 'x':
                                                {
                                                    if(isxdigit(escbytes[index + 1]) && isxdigit(escbytes[index + 2]))
                                                    {
                                                        ecc_strbuf_appendcodepoint(&chars, ecc_astlex_uint8hex(escbytes[index + 1], escbytes[index + 2]));
                                                        index += 2;
                                                        break;
                                                    }
                                                    self->text = ecc_strbox_make(self->text.bytes + index - 1, 4);
                                                    return ecc_astlex_syntaxerror(self, ecc_strbuf_create("malformed hexadecimal character escape sequence"));
                                                }
                                            case 'u':
                                                {
                                                    ubi1 = escbytes[index + 1];
                                                    ubi2 = escbytes[index + 2];
                                                    ubi3 = escbytes[index + 3];
                                                    ubi4 = escbytes[index + 4];
                                                    if(isxdigit(ubi1) && isxdigit(ubi2) && isxdigit(ubi3) && isxdigit(ubi4))
                                                    {
                                                        ecc_strbuf_appendcodepoint(&chars, ecc_astlex_uint16hex(ubi1, ubi2, ubi3, ubi4));
                                                        index += 4;
                                                        break;
                                                    }
                                                    self->text = ecc_strbox_make(self->text.bytes + index - 1, 6);
                                                    return ecc_astlex_syntaxerror(self, ecc_strbuf_create("malformed Unicode character escape sequence"));
                                                }
                                                break;
                                            case '\r':
                                                {
                                                    if(escbytes[index + 1] == '\n')
                                                    {
                                                        ++index;
                                                    }
                                                }
                                                /* fallthrough */
                                            case '\n':
                                                {
                                                }
                                                continue;
                                            default:
                                                {
                                                    ecc_strbuf_append(&chars, "%c", escbytes[index]);
                                                }
                                                break;
                                        }
                                    }
                                    else
                                    {
                                        ecc_strbuf_append(&chars, "%c", escbytes[index]);
                                    }
                                }
                                self->tokenvalue = ecc_ioinput_attachvalue(self->input, ecc_strbuf_endappend(&chars));
                                return ECC_TOK_ESCAPEDSTRING;
                            }
                            return ECC_TOK_STRING;
                        }
                        else if(currch == '\r' || currch == '\n')
                        {
                            break;
                        }
                    }
                    return ecc_astlex_syntaxerror(self, ecc_strbuf_create("unterminated string literal"));
                }
            case '.':
                {
                    if(!isdigit(ecc_astlex_previewchar(self)))
                    {
                        return currch;
                    }
                }
                /* fallthrough */
            case '0':
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
                    int isbin = 0;
                    if(currch == '0' && (ecc_astlex_acceptchar(self, 'x') || ecc_astlex_acceptchar(self, 'X')))
                    {
                        while((currch = ecc_astlex_previewchar(self)))
                        {
                            if(isxdigit(currch))
                            {
                                ecc_astlex_nextchar(self);
                            }
                            else
                            {
                                break;
                            }
                        }
                        if(self->text.length <= 2)
                        {
                            return ecc_astlex_syntaxerror(self, ecc_strbuf_create("missing hexadecimal digits after '0x'"));
                        }
                    }
                    else
                    {
                        while(isdigit(ecc_astlex_previewchar(self)))
                        {
                            ecc_astlex_nextchar(self);
                        }
                        if(currch == '.' || ecc_astlex_acceptchar(self, '.'))
                        {
                            isbin = 1;
                        }
                        while(isdigit(ecc_astlex_previewchar(self)))
                        {
                            ecc_astlex_nextchar(self);
                        }
                        if(ecc_astlex_acceptchar(self, 'e') || ecc_astlex_acceptchar(self, 'E'))
                        {
                            isbin = 1;
                            if(!ecc_astlex_acceptchar(self, '+'))
                            {
                                ecc_astlex_acceptchar(self, '-');
                            }
                            if(!isdigit(ecc_astlex_previewchar(self)))
                            {
                                return ecc_astlex_syntaxerror(self, ecc_strbuf_create("missing exponent"));
                            }
                            while(isdigit(ecc_astlex_previewchar(self)))
                            {
                                ecc_astlex_nextchar(self);
                            }
                        }
                    }
                    if(isalpha(ecc_astlex_previewchar(self)))
                    {
                        self->text.bytes += self->text.length;
                        self->text.length = 1;
                        return ecc_astlex_syntaxerror(self, ecc_strbuf_create("identifier starts immediately after numeric literal"));
                    }
                    if(isbin)
                    {
                        self->tokenvalue = ecc_astlex_scanbinary(self->text, 0);
                        return ECC_TOK_BINARY;
                    }
                    else
                    {
                        self->tokenvalue = ecc_astlex_scaninteger(self->text, 0, 0);
                        if(self->tokenvalue.type == ECC_VALTYPE_INTEGER)
                        {
                            return ECC_TOK_INTEGER;
                        }
                        else
                        {
                            return ECC_TOK_BINARY;
                        }
                    }
                }
            case '}':
            case ')':
            case ']':
            case '{':
            case '(':
            case '[':
            case ';':
            case ',':
            case '~':
            case '?':
            case ':':
                {
                    return currch;
                }
            case '^':
                {
                    if(ecc_astlex_acceptchar(self, '='))
                        return ECC_TOK_XORASSIGN;
                    else
                        return currch;
                }
            case '%':
                {
                    if(ecc_astlex_acceptchar(self, '='))
                        return ECC_TOK_MODULOASSIGN;
                    else
                        return currch;
                }
            case '*':
                {
                    if(ecc_astlex_acceptchar(self, '='))
                        return ECC_TOK_MULTIPLYASSIGN;
                    else
                        return currch;
                }
            case '=':
                {
                    if(ecc_astlex_acceptchar(self, '='))
                    {
                        if(ecc_astlex_acceptchar(self, '='))
                            return ECC_TOK_IDENTICAL;
                        else
                            return ECC_TOK_EQUAL;
                    }
                    else
                        return currch;
                }
            case '!':
                {
                    if(ecc_astlex_acceptchar(self, '='))
                    {
                        if(ecc_astlex_acceptchar(self, '='))
                            return ECC_TOK_NOTIDENTICAL;
                        else
                            return ECC_TOK_NOTEQUAL;
                    }
                    else
                        return currch;
                }
            case '+':
                {
                    if(ecc_astlex_acceptchar(self, '+'))
                        return ECC_TOK_INCREMENT;
                    else if(ecc_astlex_acceptchar(self, '='))
                        return ECC_TOK_ADDASSIGN;
                    else
                        return currch;
                }
            case '-':
                {
                    if(ecc_astlex_acceptchar(self, '-'))
                        return ECC_TOK_DECREMENT;
                    else if(ecc_astlex_acceptchar(self, '='))
                        return ECC_TOK_MINUSASSIGN;
                    else
                        return currch;
                }
            case '&':
                {
                    if(ecc_astlex_acceptchar(self, '&'))
                        return ECC_TOK_LOGICALAND;
                    else if(ecc_astlex_acceptchar(self, '='))
                        return ECC_TOK_ANDASSIGN;
                    else
                        return currch;
                }
            case '|':
                {
                    if(ecc_astlex_acceptchar(self, '|'))
                    {
                        return ECC_TOK_LOGICALOR;
                    }
                    else if(ecc_astlex_acceptchar(self, '='))
                    {
                        return ECC_TOK_ORASSIGN;
                    }
                    else
                    {
                        return currch;
                    }
                }
                break;
            case '<':
                {            
                    if(ecc_astlex_acceptchar(self, '<'))
                    {
                        if(ecc_astlex_acceptchar(self, '='))
                            return ECC_TOK_LEFTSHIFTASSIGN;
                        else
                            return ECC_TOK_LEFTSHIFT;
                    }
                    else if(ecc_astlex_acceptchar(self, '='))
                    {
                        return ECC_TOK_LESSOREQUAL;
                    }
                    else
                    {
                        return currch;
                    }
                }
                break;
            case '>':
                {
                    if(ecc_astlex_acceptchar(self, '>'))
                    {
                        if(ecc_astlex_acceptchar(self, '>'))
                        {
                            if(ecc_astlex_acceptchar(self, '='))
                            {
                                return ECC_TOK_UNSIGNEDRIGHTSHIFTASSIGN;
                            }
                            else
                            {
                                return ECC_TOK_UNSIGNEDRIGHTSHIFT;
                            }
                        }
                        else if(ecc_astlex_acceptchar(self, '='))
                        {
                            return ECC_TOK_RIGHTSHIFTASSIGN;
                        }
                        else
                        {
                            return ECC_TOK_RIGHTSHIFT;
                        }
                    }
                    else if(ecc_astlex_acceptchar(self, '='))
                    {
                        return ECC_TOK_MOREOREQUAL;
                    }
                    else
                    {
                        return currch;
                    }
                }
            default:
                {
                    isb80 = (currch < 0x80 && isalpha(currch));
                    if(isb80 || currch == '$' || currch == '_' || (self->permitutfoutsidelit && (currch == '\\' || currch >= 0x80)))
                    {
                        identxt = self->text;
                        haveesc = 0;
                        do
                        {
                            if(currch == '\\')
                            {
                                ubc0 = ecc_astlex_nextchar(self);
                                ubc1 = ecc_astlex_nextchar(self);
                                ubc2 = ecc_astlex_nextchar(self);
                                ubc3 = ecc_astlex_nextchar(self);
                                ubc4 = ecc_astlex_nextchar(self);
                                if(ubc0 == 'u' && isxdigit(ubc1) && isxdigit(ubc2) && isxdigit(ubc3) && isxdigit(ubc4))
                                {
                                    currch = ecc_astlex_uint16hex(ubc1, ubc2, ubc3, ubc4);
                                    haveesc = 1;
                                }
                                else
                                {
                                    return ecc_astlex_syntaxerror(self, ecc_strbuf_create("incomplete unicode escape"));
                                }
                            }
                            if(ecc_strbox_isspace((eccrune_t){ currch, 0 }))
                            {
                                break;
                            }
                            identxt = self->text;
                            currch = ecc_astlex_nextchar(self);
                        } while(isalnum(currch) || currch == '$' || currch == '_' || (self->permitutfoutsidelit && (currch == '\\' || currch >= 0x80)));
                        self->text = identxt;
                        self->offset = (uint32_t)(identxt.bytes + identxt.length - self->input->bytes);
                        if(haveesc)
                        {
                            escsubtext = self->text;
                            ecc_strbuf_beginappend(&esccharbuf);
                            while(escsubtext.length)
                            {
                                currch = ecc_strbox_nextcharacter(&escsubtext).codepoint;
                                if(currch == '\\' && ecc_strbox_nextcharacter(&escsubtext).codepoint == 'u')
                                {
                                    ubc1 = ecc_strbox_nextcharacter(&escsubtext).codepoint;
                                    ubc2 = ecc_strbox_nextcharacter(&escsubtext).codepoint;
                                    ubc3 = ecc_strbox_nextcharacter(&escsubtext).codepoint;
                                    ubc4 = ecc_strbox_nextcharacter(&escsubtext).codepoint;
                                    if(isxdigit(ubc1) && isxdigit(ubc2) && isxdigit(ubc3) && isxdigit(ubc4))
                                    {
                                        currch = ecc_astlex_uint16hex(ubc1, ubc2, ubc3, ubc4);
                                    }
                                    else
                                    {
                                        ecc_strbox_advance(&escsubtext, -5);
                                    }
                                }
                                ecc_strbuf_appendcodepoint(&esccharbuf, currch);
                            }
                            escvalue = ecc_ioinput_attachvalue(self->input, ecc_strbuf_endappend(&esccharbuf));
                            self->tokenvalue = ecc_value_fromkey(ecc_keyidx_makewithtext(ecc_value_textof(&escvalue), escvalue.type != ECC_VALTYPE_CHARS));
                            return ECC_TOK_IDENTIFIER;
                        }
                        if(!self->disallowKeyword)
                        {
                            for(k = 0; g_lexkeyidents[k].name != NULL; ++k)
                            {
                                kidstr = g_lexkeyidents[k].name;
                                kidlen = g_lexkeyidents[k].length;
                                if(self->text.length == (int)kidlen && memcmp(self->text.bytes, kidstr, kidlen) == 0)
                                {
                                    return g_lexkeyidents[k].token;
                                }
                            }
                            for(k = 0; g_lexreservedidents[k].name != NULL; k++)
                            {
                                kidstr = g_lexreservedidents[k].name;
                                kidlen = g_lexreservedidents[k].length;
                                if(self->text.length == (int)kidlen && memcmp(self->text.bytes, kidstr, kidlen) == 0)
                                {
                                    return ecc_astlex_syntaxerror(self, ecc_strbuf_create("'%s' is a reserved identifier", kidstr));
                                }
                            }
                        }
                        self->tokenvalue = ecc_value_fromkey(ecc_keyidx_makewithtext(self->text, 0));
                        return ECC_TOK_IDENTIFIER;
                    }
                    else
                    {
                        if(currch >= 0x80)
                        {
                            return ecc_astlex_syntaxerror(self, ecc_strbuf_create("invalid character '%.*s'", self->text.length, self->text.bytes));
                        }
                        else if(isprint(currch))
                        {
                            return ecc_astlex_syntaxerror(self, ecc_strbuf_create("invalid character '%c'", currch));
                        }
                        else
                        {
                            return ecc_astlex_syntaxerror(self, ecc_strbuf_create("invalid character '\\%d'", currch & 0xff));
                        }
                    }
                }
        }
    }

    ecc_astlex_addline(self, self->offset);
    return ECC_TOK_NO;
}

const char* ecc_astlex_tokenchars(int token, char buffer[4])
{
    int index;
    static const struct
    {
        const char* name;
        const eccasttoktype_t token;
    } tokenList[] = {
        {"end of script", ECC_TOK_NO},
        {"error", ECC_TOK_ERROR},
        {"null", ECC_TOK_NULL},
        {"true", ECC_TOK_TRUE},
        {"false", ECC_TOK_FALSE},
        {"number", ECC_TOK_INTEGER},
        {"number", ECC_TOK_BINARY},
        {"string", ECC_TOK_STRING},
        {"string", ECC_TOK_ESCAPEDSTRING},
        {"identifier", ECC_TOK_IDENTIFIER},
        {"regexp", ECC_TOK_REGEXP},
        {"break", ECC_TOK_BREAK},
        {"case", ECC_TOK_CASE},
        {"catch", ECC_TOK_CATCH},
        {"continue", ECC_TOK_CONTINUE},
        {"debugger", ECC_TOK_DEBUGGER},
        {"default", ECC_TOK_DEFAULT},
        {"delete", ECC_TOK_DELETE},
        {"do", ECC_TOK_DO},
        {"else", ECC_TOK_ELSE},
        {"finally", ECC_TOK_FINALLY},
        {"for", ECC_TOK_FOR},
        {"function", ECC_TOK_FUNCTION},
        {"if", ECC_TOK_IF},
        {"in", ECC_TOK_IN},
        {"instanceof", ECC_TOK_INSTANCEOF},
        {"new", ECC_TOK_NEW},
        {"return", ECC_TOK_RETURN},
        {"switch", ECC_TOK_SWITCH},
        {"this", ECC_TOK_THIS},
        {"throw", ECC_TOK_THROW},
        {"try", ECC_TOK_TRY},
        {"typeof", ECC_TOK_TYPEOF},
        {"var", ECC_TOK_VAR},
        {"void", ECC_TOK_VOID},
        {"with", ECC_TOK_WITH},
        {"while", ECC_TOK_WHILE},
        {"'=='", ECC_TOK_EQUAL},
        {"'!='", ECC_TOK_NOTEQUAL},
        {"'==='", ECC_TOK_IDENTICAL},
        {"'!=='", ECC_TOK_NOTIDENTICAL},
        {"'<<='", ECC_TOK_LEFTSHIFTASSIGN},
        {"'>>='", ECC_TOK_RIGHTSHIFTASSIGN},
        {"'>>>='", ECC_TOK_UNSIGNEDRIGHTSHIFTASSIGN},
        {"'<<'", ECC_TOK_LEFTSHIFT},
        {"'>>'", ECC_TOK_RIGHTSHIFT},
        {"'>>>'", ECC_TOK_UNSIGNEDRIGHTSHIFT},
        {"'<='", ECC_TOK_LESSOREQUAL},
        {"'>='", ECC_TOK_MOREOREQUAL},
        {"'++'", ECC_TOK_INCREMENT},
        {"'--'", ECC_TOK_DECREMENT},
        {"'&&'", ECC_TOK_LOGICALAND},
        {"'||'", ECC_TOK_LOGICALOR},
        {"'+='", ECC_TOK_ADDASSIGN},
        {"'-='", ECC_TOK_MINUSASSIGN},
        {"'*='", ECC_TOK_MULTIPLYASSIGN},
        {"'/='", ECC_TOK_DIVIDEASSIGN},
        {"'%='", ECC_TOK_MODULOASSIGN},
        {"'&='", ECC_TOK_ANDASSIGN},
        {"'|='", ECC_TOK_ORASSIGN},
        {"'^='", ECC_TOK_XORASSIGN},
        {NULL, (eccasttoktype_t)0},
    };
    if(token > ECC_TOK_NO && token < ECC_TOK_ERROR)
    {
        buffer[0] = '\'';
        buffer[1] = token;
        buffer[2] = '\'';
        buffer[3] = '\0';
        return buffer;
    }
    for(index = 0; tokenList[index].name != NULL; index++)
    {
        if(tokenList[index].token == token)
        {
            return tokenList[index].name;
        }
    }
    assert(0);
    return "unknown";
}

eccvalue_t ecc_astlex_scanbinary(eccstrbox_t text, int flags)
{
    int lazy;
    double dblval;
    char* end;
    char* buffer;
    eccstrbox_t tail;
    buffer = (char*)calloc(text.length+1, sizeof(char));
    lazy = flags & ECC_LEXFLAG_SCANLAZY;
    end = buffer;
    dblval = ECC_CONST_NAN;
    if(flags & ECC_LEXFLAG_SCANSLOPPY)
    {
        tail = ecc_strbox_make(text.bytes + text.length, text.length);
        while(tail.length && ecc_strbox_isspace(ecc_strbox_prevcharacter(&tail)))
        {
            text.length = tail.length;
        }
        while(text.length && ecc_strbox_isspace(ecc_strbox_character(text)))
        {
            ecc_strbox_nextcharacter(&text);
        }
    }
    else
    {
        while(text.length && isspace(*text.bytes))
        {
            ecc_strbox_advance(&text, 1);
        }
    }
    memcpy(buffer, text.bytes, text.length);
    buffer[text.length] = '\0';
    if(text.length)
    {
        if(lazy && text.length >= 2 && buffer[0] == '0' && buffer[1] == 'x')
        {
            free(buffer);
            return ecc_value_fromfloat(0);
        }
        if(text.length >= ECC_String_Infinity.length && !memcmp(buffer, ECC_String_Infinity.bytes, ECC_String_Infinity.length))
        {
            dblval = ECC_CONST_INFINITY;
            end += ECC_String_Infinity.length;
        }
        else if(text.length >= ECC_String_NegInfinity.length && !memcmp(buffer, ECC_String_NegInfinity.bytes, ECC_String_NegInfinity.length))
        {
            dblval = -ECC_CONST_INFINITY;
            end += ECC_String_NegInfinity.length;
        }
        else if(!isalpha(buffer[0]))
        {
            dblval = strtod(buffer, &end);
        }
        if((!lazy && *end && !isspace(*end)) || (lazy && end == buffer))
        {
            dblval = ECC_CONST_NAN;
        }
    }
    else if(!lazy)
    {
        dblval = 0;
    }
    free(buffer);
    return ecc_value_fromfloat(dblval);
}

eccvalue_t ecc_astlex_scaninteger(eccstrbox_t text, int base, int flags)
{
    int lazy;
    long integer;
    double dblval;
    char* end;
    char* buffer;
    buffer = (char*)calloc(text.length + 1, sizeof(char));
    lazy = flags & ECC_LEXFLAG_SCANLAZY;
    if(flags & ECC_LEXFLAG_SCANSLOPPY)
    {
        eccstrbox_t tail = ecc_strbox_make(text.bytes + text.length, text.length);
        while(tail.length && ecc_strbox_isspace(ecc_strbox_prevcharacter(&tail)))
        {
            text.length = tail.length;
        }        
        while(text.length && ecc_strbox_isspace(ecc_strbox_character(text)))
        {
            ecc_strbox_nextcharacter(&text);
        }
    }
    else
    {
        while(text.length && isspace(*text.bytes))
        {
            ecc_strbox_advance(&text, 1);
        }
    }
    memcpy(buffer, text.bytes, text.length);
    buffer[text.length] = '\0';
    errno = 0;
    integer = strtol(buffer, &end, base);
    if((lazy && end == buffer) || (!lazy && *end && !isspace(*end)))
    {
        free(buffer);
        return ecc_value_fromfloat(ECC_CONST_NAN);
    }
    else if(errno == ERANGE)
    {
        if(!base || base == 10 || base == 16)
        {
            dblval = strtod(buffer, NULL);
            if(!dblval && (!base || base == 16))
            {
                dblval = ecc_astlex_strtolhexfallback(text);
            }
            free(buffer);
            return ecc_value_fromfloat(dblval);
        }
        free(buffer);
        ecc_env_printwarning("`parseInt('%.*s', %d)` out of bounds; only long int are supported by radices other than 10 or 16", text.length, text.bytes, base);
        return ecc_value_fromfloat(ECC_CONST_NAN);
    }
    free(buffer);
    if(integer < INT32_MIN || integer > INT32_MAX)
    {
        return ecc_value_fromfloat(integer);
    }
    return ecc_value_fromint((int32_t)integer);
}

uint32_t ecc_astlex_scanelement(eccstrbox_t text)
{
    eccvalue_t value;
    uint32_t index;
    if(!text.length)
    {
        return UINT32_MAX;
    }
    for(index = 0; index < text.length; ++index)
    {
        if(!isdigit(text.bytes[index]))
        {
            return UINT32_MAX;
        }
    }
    value = ecc_astlex_scaninteger(text, 0, 0);
    if(value.type == ECC_VALTYPE_INTEGER)
    {
        return value.data.integer;
    }
    if(value.type == ECC_VALTYPE_BINARY && value.data.valnumfloat >= 0 && value.data.valnumfloat < UINT32_MAX && value.data.valnumfloat == (uint32_t)value.data.valnumfloat)
    {
        return value.data.valnumfloat;
    }    
    return UINT32_MAX;
}

