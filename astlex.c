//
//  lexer.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"

// MARK: - Private
#define eccmac_stringandlen(str) str, sizeof(str) - 1

eccastlexer_t* nslexerfn_createWithInput(eccioinput_t*);
void nslexerfn_destroy(eccastlexer_t*);
int nslexerfn_nextToken(eccastlexer_t*);
const char* nslexerfn_tokenChars(int token, char buffer[4]);
eccvalue_t nslexerfn_scanBinary(ecctextstring_t text, int);
eccvalue_t nslexerfn_scanInteger(ecctextstring_t text, int base, int);
uint32_t nslexerfn_scanElement(ecctextstring_t text);
uint8_t nslexerfn_uint8Hex(char a, char b);
uint16_t nslexerfn_uint16Hex(char a, char b, char c, char d);


static const struct
{
    const char* name;
    size_t length;
    eccasttoktype_t token;
} keywords[] = {
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
};

static const struct
{
    const char* name;
    size_t length;
} reservedKeywords[] = {
    { eccmac_stringandlen("class") },   { eccmac_stringandlen("enum") },      { eccmac_stringandlen("extends") }, { eccmac_stringandlen("super") },
    { eccmac_stringandlen("const") },   { eccmac_stringandlen("export") },    { eccmac_stringandlen("import") },  { eccmac_stringandlen("implements") },
    { eccmac_stringandlen("let") },     { eccmac_stringandlen("private") },   { eccmac_stringandlen("public") },  { eccmac_stringandlen("interface") },
    { eccmac_stringandlen("package") }, { eccmac_stringandlen("protected") }, { eccmac_stringandlen("static") },  { eccmac_stringandlen("yield") },
};


const struct eccpseudonslexer_t ECCNSLexer = {
    nslexerfn_createWithInput,
    nslexerfn_destroy,
    nslexerfn_nextToken,
    nslexerfn_tokenChars,
    nslexerfn_scanBinary,
    nslexerfn_scanInteger,
    nslexerfn_scanElement,
    nslexerfn_uint8Hex,
    nslexerfn_uint16Hex,
    {}
};


int8_t ecclex_hexhigit(int c)
{
    if(c >= 'a' && c <= 'f')
    {
        return c - 'a' + 10;
    }
    else if(c >= 'A' && c <= 'F')
    {
        return c - 'A' + 10;
    }
    return c - '0';
}

uint8_t nslexerfn_uint8Hex(char a, char b)
{
    return ecclex_hexhigit(a) << 4 | ecclex_hexhigit(b);
}

uint16_t nslexerfn_uint16Hex(char a, char b, char c, char d)
{
    return ecclex_hexhigit(a) << 12 | ecclex_hexhigit(b) << 8 | ecclex_hexhigit(c) << 4 | ecclex_hexhigit(d);
}

double ecclex_strtolHexFallback(ecctextstring_t text)
{
    double binary = 0;
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
            binary *= 16;
            if(isdigit(c))
            {
                binary += c - '0';
            }
            else if(isxdigit(c))
            {
                binary += tolower(c) - ('a' - 10);
            }
            else
            {
                return NAN;
            }
        }
    }
    return binary * sign;
}

void ecclex_addLine(eccastlexer_t* self, uint32_t offset)
{
    if(self->input->lineCount + 1 >= self->input->lineCapacity)
    {
        self->input->lineCapacity *= 2;
        self->input->lines = (uint32_t*)realloc(self->input->lines, sizeof(*self->input->lines) * self->input->lineCapacity);
    }
    self->input->lines[++self->input->lineCount] = offset;
}

unsigned char ecclex_previewChar(eccastlexer_t* self)
{
    if(self->offset < self->input->length)
        return self->input->bytes[self->offset];
    else
        return 0;
}

uint32_t ecclex_nextChar(eccastlexer_t* self)
{
    if(self->offset < self->input->length)
    {
        ecctextchar_t c = ECCNSText.character(ECCNSText.make(self->input->bytes + self->offset, self->input->length - self->offset));

        self->offset += c.units;

        if((self->allowUnicodeOutsideLiteral && ECCNSText.isLineFeed(c)) || (c.codepoint == '\r' && ecclex_previewChar(self) != '\n') || c.codepoint == '\n')
        {
            self->didLineBreak = 1;
            ecclex_addLine(self, self->offset);
            c.codepoint = '\n';
        }
        else if(self->allowUnicodeOutsideLiteral && ECCNSText.isSpace(c))
            c.codepoint = ' ';

        self->text.length += c.units;
        return c.codepoint;
    }
    else
        return 0;
}

int ecclex_acceptChar(eccastlexer_t* self, char c)
{
    if(ecclex_previewChar(self) == c)
    {
        ecclex_nextChar(self);
        return 1;
    }
    else
        return 0;
}

char ecclex_eof(eccastlexer_t* self)
{
    return self->offset >= self->input->length;
}

int ecclex_syntaxError(eccastlexer_t* self, ecccharbuffer_t* message)
{
    eccobjerror_t* error = ECCNSError.syntaxError(self->text, message);
    self->value = ecc_value_error(error);
    return ECC_TOK_ERROR;
}

// MARK: - Methods

eccastlexer_t* nslexerfn_createWithInput(eccioinput_t* input)
{
    eccastlexer_t* self = (eccastlexer_t*)malloc(sizeof(*self));
    *self = ECCNSLexer.identity;

    assert(input);
    self->input = input;

    return self;
}

void nslexerfn_destroy(eccastlexer_t* self)
{
    assert(self);

    self->input = NULL;
    free(self), self = NULL;
}

int nslexerfn_nextToken(eccastlexer_t* self)
{
    uint32_t c;
    assert(self);

    self->value = ECCValConstUndefined;
    self->didLineBreak = 0;

retry:
    self->text.bytes = self->input->bytes + self->offset;
    self->text.length = 0;

    while((c = ecclex_nextChar(self)))
    {
        switch(c)
        {
            case '\n':
            case '\r':
            case '\t':
            case '\v':
            case '\f':
            case ' ':
                goto retry;
            case '/':
            {
                if(ecclex_acceptChar(self, '*'))
                {
                    while(!ecclex_eof(self))
                        if(ecclex_nextChar(self) == '*' && ecclex_acceptChar(self, '/'))
                            goto retry;

                    return ecclex_syntaxError(self, ECCNSChars.create("unterminated comment"));
                }
                else if(ecclex_previewChar(self) == '/')
                {
                    while((c = ecclex_nextChar(self)))
                        if(c == '\r' || c == '\n')
                            goto retry;

                    return 0;
                }
                else if(self->allowRegex)
                {
                    while(!ecclex_eof(self))
                    {
                        int rxclit = ecclex_nextChar(self);

                        if(rxclit == '\\')
                            rxclit = ecclex_nextChar(self);
                        else if(rxclit == '/')
                        {
                            while(isalnum(ecclex_previewChar(self)) || ecclex_previewChar(self) == '\\')
                                ecclex_nextChar(self);

                            return ECC_TOK_REGEXP;
                        }

                        if(rxclit == '\n')
                            break;
                    }
                    return ecclex_syntaxError(self, ECCNSChars.create("unterminated regexp literal"));
                }
                else if(ecclex_acceptChar(self, '='))
                    return ECC_TOK_DIVIDEASSIGN;
                else
                    return '/';
            }
            case '\'':
            case '"':
                {
                    char end = c;
                    int haveesc = 0;
                    int didLineBreak = self->didLineBreak;

                    while((c = ecclex_nextChar(self)))
                    {
                        if(c == '\\')
                        {
                            haveesc = 1;
                            ecclex_nextChar(self);
                            self->didLineBreak = didLineBreak;
                        }
                        else if(c == (uint32_t)end)
                        {
                            const char* bytes = self->text.bytes++;
                            uint32_t length = self->text.length -= 2;

                            if(haveesc)
                            {
                                eccappendbuffer_t chars;
                                uint32_t index;

                                ++bytes;
                                --length;
                                ECCNSChars.beginAppend(&chars);

                                for(index = 0; index <= length; ++index)
                                    if(bytes[index] == '\\' && bytes[++index] != '\\')
                                    {
                                        switch(bytes[index])
                                        {
                                            case '0':
                                                ECCNSChars.appendCodepoint(&chars, '\0');
                                                break;
                                            case 'b':
                                                ECCNSChars.appendCodepoint(&chars, '\b');
                                                break;
                                            case 'f':
                                                ECCNSChars.appendCodepoint(&chars, '\f');
                                                break;
                                            case 'n':
                                                ECCNSChars.appendCodepoint(&chars, '\n');
                                                break;
                                            case 'r':
                                                ECCNSChars.appendCodepoint(&chars, '\r');
                                                break;
                                            case 't':
                                                ECCNSChars.appendCodepoint(&chars, '\t');
                                                break;
                                            case 'v':
                                                ECCNSChars.appendCodepoint(&chars, '\v');
                                                break;
                                            case 'x':
                                                {
                                                    if(isxdigit(bytes[index + 1]) && isxdigit(bytes[index + 2]))
                                                    {
                                                        ECCNSChars.appendCodepoint(&chars, nslexerfn_uint8Hex(bytes[index + 1], bytes[index + 2]));
                                                        index += 2;
                                                        break;
                                                    }
                                                    self->text = ECCNSText.make(self->text.bytes + index - 1, 4);
                                                    return ecclex_syntaxError(self, ECCNSChars.create("malformed hexadecimal character escape sequence"));
                                                }
                                            case 'u':
                                                {
                                                    if(isxdigit(bytes[index + 1]) && isxdigit(bytes[index + 2]) && isxdigit(bytes[index + 3]) && isxdigit(bytes[index + 4]))
                                                    {
                                                        ECCNSChars.appendCodepoint(&chars, nslexerfn_uint16Hex(bytes[index + 1], bytes[index + 2], bytes[index + 3], bytes[index + 4]));
                                                        index += 4;
                                                        break;
                                                    }
                                                    self->text = ECCNSText.make(self->text.bytes + index - 1, 6);
                                                    return ecclex_syntaxError(self, ECCNSChars.create("malformed Unicode character escape sequence"));
                                                }
                                            case '\r':
                                                {
                                                    if(bytes[index + 1] == '\n')
                                                    {
                                                        ++index;
                                                    }
                                                }
                                            case '\n':
                                                {
                                                }
                                                continue;
                                            default:
                                                {
                                                    ECCNSChars.append(&chars, "%c", bytes[index]);
                                                }
                                        }
                                    }
                                    else
                                        ECCNSChars.append(&chars, "%c", bytes[index]);

                                self->value = ECCNSInput.attachValue(self->input, ECCNSChars.endAppend(&chars));
                                return ECC_TOK_ESCAPEDSTRING;
                            }

                            return ECC_TOK_STRING;
                        }
                        else if(c == '\r' || c == '\n')
                            break;
                    }
                    return ecclex_syntaxError(self, ECCNSChars.create("unterminated string literal"));
                }

            case '.':
                {
                    if(!isdigit(ecclex_previewChar(self)))
                    {
                        return c;
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
                    int binary = 0;
                    if(c == '0' && (ecclex_acceptChar(self, 'x') || ecclex_acceptChar(self, 'X')))
                    {
                        while((c = ecclex_previewChar(self)))
                        {
                            if(isxdigit(c))
                            {
                                ecclex_nextChar(self);
                            }
                            else
                            {
                                break;
                            }
                        }
                        if(self->text.length <= 2)
                        {
                            return ecclex_syntaxError(self, ECCNSChars.create("missing hexadecimal digits after '0x'"));
                        }
                    }
                    else
                    {
                        while(isdigit(ecclex_previewChar(self)))
                        {
                            ecclex_nextChar(self);
                        }
                        if(c == '.' || ecclex_acceptChar(self, '.'))
                        {
                            binary = 1;
                        }
                        while(isdigit(ecclex_previewChar(self)))
                        {
                            ecclex_nextChar(self);
                        }
                        if(ecclex_acceptChar(self, 'e') || ecclex_acceptChar(self, 'E'))
                        {
                            binary = 1;
                            if(!ecclex_acceptChar(self, '+'))
                            {
                                ecclex_acceptChar(self, '-');
                            }
                            if(!isdigit(ecclex_previewChar(self)))
                            {
                                return ecclex_syntaxError(self, ECCNSChars.create("missing exponent"));
                            }
                            while(isdigit(ecclex_previewChar(self)))
                            {
                                ecclex_nextChar(self);
                            }
                        }
                    }
                    if(isalpha(ecclex_previewChar(self)))
                    {
                        self->text.bytes += self->text.length;
                        self->text.length = 1;
                        return ecclex_syntaxError(self, ECCNSChars.create("identifier starts immediately after numeric literal"));
                    }
                    if(binary)
                    {
                        self->value = nslexerfn_scanBinary(self->text, 0);
                        return ECC_TOK_BINARY;
                    }
                    else
                    {
                        self->value = nslexerfn_scanInteger(self->text, 0, 0);
                        if(self->value.type == ECC_VALTYPE_INTEGER)
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
                    return c;
                }
            case '^':
                {
                    if(ecclex_acceptChar(self, '='))
                        return ECC_TOK_XORASSIGN;
                    else
                        return c;
                }
            case '%':
                {
                    if(ecclex_acceptChar(self, '='))
                        return ECC_TOK_MODULOASSIGN;
                    else
                        return c;
                }
            case '*':
                {
                    if(ecclex_acceptChar(self, '='))
                        return ECC_TOK_MULTIPLYASSIGN;
                    else
                        return c;
                }
            case '=':
                {
                    if(ecclex_acceptChar(self, '='))
                    {
                        if(ecclex_acceptChar(self, '='))
                            return ECC_TOK_IDENTICAL;
                        else
                            return ECC_TOK_EQUAL;
                    }
                    else
                        return c;
                }
            case '!':
                {
                    if(ecclex_acceptChar(self, '='))
                    {
                        if(ecclex_acceptChar(self, '='))
                            return ECC_TOK_NOTIDENTICAL;
                        else
                            return ECC_TOK_NOTEQUAL;
                    }
                    else
                        return c;
                }
            case '+':
                {
                    if(ecclex_acceptChar(self, '+'))
                        return ECC_TOK_INCREMENT;
                    else if(ecclex_acceptChar(self, '='))
                        return ECC_TOK_ADDASSIGN;
                    else
                        return c;
                }
            case '-':
                {
                    if(ecclex_acceptChar(self, '-'))
                        return ECC_TOK_DECREMENT;
                    else if(ecclex_acceptChar(self, '='))
                        return ECC_TOK_MINUSASSIGN;
                    else
                        return c;
                }
            case '&':
                {
                    if(ecclex_acceptChar(self, '&'))
                        return ECC_TOK_LOGICALAND;
                    else if(ecclex_acceptChar(self, '='))
                        return ECC_TOK_ANDASSIGN;
                    else
                        return c;
                }
            case '|':
                {
                    if(ecclex_acceptChar(self, '|'))
                        return ECC_TOK_LOGICALOR;
                    else if(ecclex_acceptChar(self, '='))
                        return ECC_TOK_ORASSIGN;
                    else
                        return c;
                }
            case '<':
                {            
                    if(ecclex_acceptChar(self, '<'))
                    {
                        if(ecclex_acceptChar(self, '='))
                            return ECC_TOK_LEFTSHIFTASSIGN;
                        else
                            return ECC_TOK_LEFTSHIFT;
                    }
                    else if(ecclex_acceptChar(self, '='))
                        return ECC_TOK_LESSOREQUAL;
                    else
                        return c;
                }

            case '>':
                {
                    if(ecclex_acceptChar(self, '>'))
                    {
                        if(ecclex_acceptChar(self, '>'))
                        {
                            if(ecclex_acceptChar(self, '='))
                            {
                                return ECC_TOK_UNSIGNEDRIGHTSHIFTASSIGN;
                            }
                            else
                            {
                                return ECC_TOK_UNSIGNEDRIGHTSHIFT;
                            }
                        }
                        else if(ecclex_acceptChar(self, '='))
                        {
                            return ECC_TOK_RIGHTSHIFTASSIGN;
                        }
                        else
                        {
                            return ECC_TOK_RIGHTSHIFT;
                        }
                    }
                    else if(ecclex_acceptChar(self, '='))
                    {
                        return ECC_TOK_MOREOREQUAL;
                    }
                    else
                    {
                        return c;
                    }
                }
            default:
                {
                    int k;
                    int haveesc;
                    if((c < 0x80 && isalpha(c)) || c == '$' || c == '_' || (self->allowUnicodeOutsideLiteral && (c == '\\' || c >= 0x80)))
                    {
                        ecctextstring_t text = self->text;
                        haveesc = 0;
                        do
                        {
                            if(c == '\\')
                            {
                                char uu = ecclex_nextChar(self), u1 = ecclex_nextChar(self), u2 = ecclex_nextChar(self), u3 = ecclex_nextChar(self), u4 = ecclex_nextChar(self);
                                if(uu == 'u' && isxdigit(u1) && isxdigit(u2) && isxdigit(u3) && isxdigit(u4))
                                {
                                    c = nslexerfn_uint16Hex(u1, u2, u3, u4);
                                    haveesc = 1;
                                }
                                else
                                {
                                    return ecclex_syntaxError(self, ECCNSChars.create("incomplete unicode escape"));
                                }
                            }

                            if(ECCNSText.isSpace((ecctextchar_t){ c, 0 }))
                            {
                                break;
                            }
                            text = self->text;
                            c = ecclex_nextChar(self);
                        } while(isalnum(c) || c == '$' || c == '_' || (self->allowUnicodeOutsideLiteral && (c == '\\' || c >= 0x80)));
                        self->text = text;
                        self->offset = (uint32_t)(text.bytes + text.length - self->input->bytes);
                        if(haveesc)
                        {
                            ecctextstring_t subtext = self->text;
                            eccappendbuffer_t chars;
                            eccvalue_t value;
                            ECCNSChars.beginAppend(&chars);
                            while(subtext.length)
                            {
                                c = ECCNSText.nextCharacter(&subtext).codepoint;
                                if(c == '\\' && ECCNSText.nextCharacter(&subtext).codepoint == 'u')
                                {
                                    char u1;
                                    char u2;
                                    char u3;
                                    char u4;
                                    u1 = ECCNSText.nextCharacter(&subtext).codepoint;
                                    u2 = ECCNSText.nextCharacter(&subtext).codepoint;
                                    u3 = ECCNSText.nextCharacter(&subtext).codepoint;
                                    u4 = ECCNSText.nextCharacter(&subtext).codepoint;
                                    if(isxdigit(u1) && isxdigit(u2) && isxdigit(u3) && isxdigit(u4))
                                    {
                                        c = nslexerfn_uint16Hex(u1, u2, u3, u4);
                                    }
                                    else
                                    {
                                        ECCNSText.advance(&subtext, -5);
                                    }
                                }
                                ECCNSChars.appendCodepoint(&chars, c);
                            }
                            value = ECCNSInput.attachValue(self->input, ECCNSChars.endAppend(&chars));
                            self->value = ecc_value_key(ECCNSKey.makeWithText(ecc_value_textof(&value), value.type != ECC_VALTYPE_CHARS));
                            return ECC_TOK_IDENTIFIER;
                        }
                        if(!self->disallowKeyword)
                        {
                            for(k = 0; k < (int)(sizeof(keywords) / sizeof(*keywords)); ++k)
                            {
                                if(self->text.length == (int)keywords[k].length && memcmp(self->text.bytes, keywords[k].name, keywords[k].length) == 0)
                                {
                                    return keywords[k].token;
                                }
                            }
                            for(k = 0; k < (int)(sizeof(reservedKeywords) / sizeof(*reservedKeywords)); ++k)
                            {
                                if(self->text.length == (int)reservedKeywords[k].length && memcmp(self->text.bytes, reservedKeywords[k].name, reservedKeywords[k].length) == 0)
                                {
                                    return ecclex_syntaxError(self, ECCNSChars.create("'%s' is a reserved identifier", reservedKeywords[k]));
                                }
                            }
                        }
                        self->value = ecc_value_key(ECCNSKey.makeWithText(self->text, 0));
                        return ECC_TOK_IDENTIFIER;
                    }
                    else
                    {
                        if(c >= 0x80)
                        {
                            return ecclex_syntaxError(self, ECCNSChars.create("invalid character '%.*s'", self->text.length, self->text.bytes));
                        }
                        else if(isprint(c))
                        {
                            return ecclex_syntaxError(self, ECCNSChars.create("invalid character '%c'", c));
                        }
                        else
                        {
                            return ecclex_syntaxError(self, ECCNSChars.create("invalid character '\\%d'", c & 0xff));
                        }
                    }
                }
        }
    }

    ecclex_addLine(self, self->offset);
    return ECC_TOK_NO;
}

const char* nslexerfn_tokenChars(int token, char buffer[4])
{
    int index;
    static const struct
    {
        const char* name;
        const eccasttoktype_t token;
    } tokenList[] = {
        { (sizeof("end of script") > sizeof("") ? "end of script" : "no"), ECC_TOK_NO },
        { (sizeof("") > sizeof("") ? "" : "error"), ECC_TOK_ERROR },
        { (sizeof("") > sizeof("") ? "" : "null"), ECC_TOK_NULL },
        { (sizeof("") > sizeof("") ? "" : "true"), ECC_TOK_TRUE },
        { (sizeof("") > sizeof("") ? "" : "false"), ECC_TOK_FALSE },
        { (sizeof("number") > sizeof("") ? "number" : "integer"), ECC_TOK_INTEGER },
        { (sizeof("number") > sizeof("") ? "number" : "binary"), ECC_TOK_BINARY },
        { (sizeof("") > sizeof("") ? "" : "string"), ECC_TOK_STRING },
        { (sizeof("string") > sizeof("") ? "string" : "escapedString"), ECC_TOK_ESCAPEDSTRING },
        { (sizeof("") > sizeof("") ? "" : "identifier"), ECC_TOK_IDENTIFIER },
        { (sizeof("") > sizeof("") ? "" : "regexp"), ECC_TOK_REGEXP },
        { (sizeof("") > sizeof("") ? "" : "break"), ECC_TOK_BREAK },
        { (sizeof("") > sizeof("") ? "" : "case"), ECC_TOK_CASE },
        { (sizeof("") > sizeof("") ? "" : "catch"), ECC_TOK_CATCH },
        { (sizeof("") > sizeof("") ? "" : "continue"), ECC_TOK_CONTINUE },
        { (sizeof("") > sizeof("") ? "" : "debugger"), ECC_TOK_DEBUGGER },
        { (sizeof("") > sizeof("") ? "" : "default"), ECC_TOK_DEFAULT },
        { (sizeof("") > sizeof("") ? "" : "delete"), ECC_TOK_DELETE },
        { (sizeof("") > sizeof("") ? "" : "do"), ECC_TOK_DO },
        { (sizeof("") > sizeof("") ? "" : "else"), ECC_TOK_ELSE },
        { (sizeof("") > sizeof("") ? "" : "finally"), ECC_TOK_FINALLY },
        { (sizeof("") > sizeof("") ? "" : "for"), ECC_TOK_FOR },
        { (sizeof("") > sizeof("") ? "" : "function"), ECC_TOK_FUNCTION },
        { (sizeof("") > sizeof("") ? "" : "if"), ECC_TOK_IF },
        { (sizeof("") > sizeof("") ? "" : "in"), ECC_TOK_IN },
        { (sizeof("") > sizeof("") ? "" : "instanceof"), ECC_TOK_INSTANCEOF },
        { (sizeof("") > sizeof("") ? "" : "new"), ECC_TOK_NEW },
        { (sizeof("") > sizeof("") ? "" : "return"), ECC_TOK_RETURN },
        { (sizeof("") > sizeof("") ? "" : "switch"), ECC_TOK_SWITCH },
        { (sizeof("") > sizeof("") ? "" : "this"), ECC_TOK_THIS },
        { (sizeof("") > sizeof("") ? "" : "throw"), ECC_TOK_THROW },
        { (sizeof("") > sizeof("") ? "" : "try"), ECC_TOK_TRY },
        { (sizeof("") > sizeof("") ? "" : "typeof"), ECC_TOK_TYPEOF },
        { (sizeof("") > sizeof("") ? "" : "var"), ECC_TOK_VAR },
        { (sizeof("") > sizeof("") ? "" : "void"), ECC_TOK_VOID },
        { (sizeof("") > sizeof("") ? "" : "with"), ECC_TOK_WITH },
        { (sizeof("") > sizeof("") ? "" : "while"), ECC_TOK_WHILE },
        { (sizeof("'=='") > sizeof("") ? "'=='" : "equal"), ECC_TOK_EQUAL },
        { (sizeof("'!='") > sizeof("") ? "'!='" : "notEqual"), ECC_TOK_NOTEQUAL },
        { (sizeof("'==='") > sizeof("") ? "'==='" : "identical"), ECC_TOK_IDENTICAL },
        { (sizeof("'!=='") > sizeof("") ? "'!=='" : "notIdentical"), ECC_TOK_NOTIDENTICAL },
        { (sizeof("'<<='") > sizeof("") ? "'<<='" : "leftShiftAssign"), ECC_TOK_LEFTSHIFTASSIGN },
        { (sizeof("'>>='") > sizeof("") ? "'>>='" : "rightShiftAssign"), ECC_TOK_RIGHTSHIFTASSIGN },
        { (sizeof("'>>>='") > sizeof("") ? "'>>>='" : "unsignedRightShiftAssign"), ECC_TOK_UNSIGNEDRIGHTSHIFTASSIGN },
        { (sizeof("'<<'") > sizeof("") ? "'<<'" : "leftShift"), ECC_TOK_LEFTSHIFT },
        { (sizeof("'>>'") > sizeof("") ? "'>>'" : "rightShift"), ECC_TOK_RIGHTSHIFT },
        { (sizeof("'>>>'") > sizeof("") ? "'>>>'" : "unsignedRightShift"), ECC_TOK_UNSIGNEDRIGHTSHIFT },
        { (sizeof("'<='") > sizeof("") ? "'<='" : "lessOrEqual"), ECC_TOK_LESSOREQUAL },
        { (sizeof("'>='") > sizeof("") ? "'>='" : "moreOrEqual"), ECC_TOK_MOREOREQUAL },
        { (sizeof("'++'") > sizeof("") ? "'++'" : "increment"), ECC_TOK_INCREMENT },
        { (sizeof("'--'") > sizeof("") ? "'--'" : "decrement"), ECC_TOK_DECREMENT },
        { (sizeof("'&&'") > sizeof("") ? "'&&'" : "logicalAnd"), ECC_TOK_LOGICALAND },
        { (sizeof("'||'") > sizeof("") ? "'||'" : "logicalOr"), ECC_TOK_LOGICALOR },
        { (sizeof("'+='") > sizeof("") ? "'+='" : "addAssign"), ECC_TOK_ADDASSIGN },
        { (sizeof("'-='") > sizeof("") ? "'-='" : "minusAssign"), ECC_TOK_MINUSASSIGN },
        { (sizeof("'*='") > sizeof("") ? "'*='" : "multiplyAssign"), ECC_TOK_MULTIPLYASSIGN },
        { (sizeof("'/='") > sizeof("") ? "'/='" : "divideAssign"), ECC_TOK_DIVIDEASSIGN },
        { (sizeof("'%='") > sizeof("") ? "'%='" : "moduloAssign"), ECC_TOK_MODULOASSIGN },
        { (sizeof("'&='") > sizeof("") ? "'&='" : "andAssign"), ECC_TOK_ANDASSIGN },
        { (sizeof("'|='") > sizeof("") ? "'|='" : "orAssign"), ECC_TOK_ORASSIGN },
        { (sizeof("'^='") > sizeof("") ? "'^='" : "xorAssign"), ECC_TOK_XORASSIGN },
    };

    if(token > ECC_TOK_NO && token < ECC_TOK_ERROR)
    {
        buffer[0] = '\'';
        buffer[1] = token;
        buffer[2] = '\'';
        buffer[3] = '\0';
        return buffer;
    }

    for(index = 0; index < (int)sizeof(tokenList); ++index)
    {
        if(tokenList[index].token == token)
        {
            return tokenList[index].name;
        }
    }
    assert(0);
    return "unknown";
}

eccvalue_t nslexerfn_scanBinary(ecctextstring_t text, int flags)
{
    int lazy;
    double binary;
    char* end;
    char* buffer;
    buffer = (char*)calloc(text.length+1, sizeof(char));
    lazy = flags & ECC_LEXFLAG_SCANLAZY;
    end = buffer;
    binary = NAN;
    if(flags & ECC_LEXFLAG_SCANSLOPPY)
    {
        ecctextstring_t tail = ECCNSText.make(text.bytes + text.length, text.length);
        while(tail.length && ECCNSText.isSpace(ECCNSText.prevCharacter(&tail)))
        {
            text.length = tail.length;
        }
        while(text.length && ECCNSText.isSpace(ECCNSText.character(text)))
        {
            ECCNSText.nextCharacter(&text);
        }
    }
    else
    {
        while(text.length && isspace(*text.bytes))
        {
            ECCNSText.advance(&text, 1);
        }
    }
    memcpy(buffer, text.bytes, text.length);
    buffer[text.length] = '\0';
    if(text.length)
    {
        if(lazy && text.length >= 2 && buffer[0] == '0' && buffer[1] == 'x')
        {
            free(buffer);
            return ecc_value_binary(0);
        }
        if(text.length >= ECC_ConstString_Infinity.length && !memcmp(buffer, ECC_ConstString_Infinity.bytes, ECC_ConstString_Infinity.length))
        {
            binary = INFINITY;
            end += ECC_ConstString_Infinity.length;
        }
        else if(text.length >= ECC_ConstString_NegativeInfinity.length && !memcmp(buffer, ECC_ConstString_NegativeInfinity.bytes, ECC_ConstString_NegativeInfinity.length))
        {
            binary = -INFINITY;
            end += ECC_ConstString_NegativeInfinity.length;
        }
        else if(!isalpha(buffer[0]))
        {
            binary = strtod(buffer, &end);
        }
        if((!lazy && *end && !isspace(*end)) || (lazy && end == buffer))
        {
            binary = NAN;
        }
    }
    else if(!lazy)
    {
        binary = 0;
    }
    free(buffer);
    return ecc_value_binary(binary);
}


eccvalue_t nslexerfn_scanInteger(ecctextstring_t text, int base, int flags)
{
    int lazy;
    long integer;
    char* end;
    char* buffer;
    buffer = (char*)calloc(text.length + 1, sizeof(char));
    lazy = flags & ECC_LEXFLAG_SCANLAZY;
    if(flags & ECC_LEXFLAG_SCANSLOPPY)
    {
        ecctextstring_t tail = ECCNSText.make(text.bytes + text.length, text.length);
        while(tail.length && ECCNSText.isSpace(ECCNSText.prevCharacter(&tail)))
        {
            text.length = tail.length;
        }        
        while(text.length && ECCNSText.isSpace(ECCNSText.character(text)))
        {
            ECCNSText.nextCharacter(&text);
        }
    }
    else
    {
        while(text.length && isspace(*text.bytes))
        {
            ECCNSText.advance(&text, 1);
        }
    }
    memcpy(buffer, text.bytes, text.length);
    buffer[text.length] = '\0';
    errno = 0;
    integer = strtol(buffer, &end, base);
    if((lazy && end == buffer) || (!lazy && *end && !isspace(*end)))
    {
        free(buffer);
        return ecc_value_binary(NAN);
    }
    else if(errno == ERANGE)
    {
        if(!base || base == 10 || base == 16)
        {
            double binary = strtod(buffer, NULL);
            if(!binary && (!base || base == 16))
            {
                binary = ecclex_strtolHexFallback(text);
            }
            free(buffer);
            return ecc_value_binary(binary);
        }
        free(buffer);
        ECCNSEnv.printWarning("`parseInt('%.*s', %d)` out of bounds; only long int are supported by radices other than 10 or 16", text.length, text.bytes, base);
        return ecc_value_binary(NAN);
    }
    free(buffer);
    if(integer < INT32_MIN || integer > INT32_MAX)
    {
        return ecc_value_binary(integer);
    }
    return ecc_value_integer((int32_t)integer);
}

uint32_t nslexerfn_scanElement(ecctextstring_t text)
{
    eccvalue_t value;
    uint16_t index;

    if(!text.length)
        return UINT32_MAX;

    for(index = 0; index < text.length; ++index)
        if(!isdigit(text.bytes[index]))
            return UINT32_MAX;

    value = nslexerfn_scanInteger(text, 0, 0);

    if(value.type == ECC_VALTYPE_INTEGER)
        return value.data.integer;
    if(value.type == ECC_VALTYPE_BINARY && value.data.binary >= 0 && value.data.binary < UINT32_MAX && value.data.binary == (uint32_t)value.data.binary)
        return value.data.binary;
    else
        return UINT32_MAX;
}

