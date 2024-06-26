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

// MARK: - Static Members

static eccastlexer_t* createWithInput(eccioinput_t*);
static void destroy(eccastlexer_t*);
static eccasttoktype_t nextToken(eccastlexer_t*);
static const char* tokenChars(eccasttoktype_t token, char buffer[4]);
static eccvalue_t scanBinary(ecctextstring_t text, eccastlexflags_t);
static eccvalue_t scanInteger(ecctextstring_t text, int base, eccastlexflags_t);
static uint32_t scanElement(ecctextstring_t text);
static uint8_t uint8Hex(char a, char b);
static uint16_t uint16Hex(char a, char b, char c, char d);
const struct eccpseudonslexer_t io_libecc_Lexer = {
    createWithInput, destroy, nextToken, tokenChars, scanBinary, scanInteger, scanElement, uint8Hex, uint16Hex,
    {},
};

static void addLine(eccastlexer_t* self, uint32_t offset)
{
    size_t need;
    char** tmp;
    if(self->input->lineCount + 1 >= self->input->lineCapacity)
    {
        self->input->lineCapacity *= 2;
        need = (sizeof(*self->input->lines) * self->input->lineCapacity);
        if(need == 0)
        {
            need = 1;
        }
        tmp = realloc(self->input->lines, need);
        if(tmp == NULL)
        {
            fprintf(stderr, "cannot reallocate to %d bytes\n", need);
        }
        self->input->lines = tmp;
    }
    self->input->lines[++self->input->lineCount] = offset;
}

static unsigned char previewChar(eccastlexer_t* self)
{
    if(self->offset < self->input->length)
        return self->input->bytes[self->offset];
    else
        return 0;
}

static uint32_t nextChar(eccastlexer_t* self)
{
    if(self->offset < self->input->length)
    {
        ecctextchar_t c = ECCNSText.character(ECCNSText.make(self->input->bytes + self->offset, self->input->length - self->offset));

        self->offset += c.units;

        if((self->allowUnicodeOutsideLiteral && ECCNSText.isLineFeed(c)) || (c.codepoint == '\r' && previewChar(self) != '\n') || c.codepoint == '\n')
        {
            self->didLineBreak = 1;
            addLine(self, self->offset);
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

static int acceptChar(eccastlexer_t* self, char c)
{
    if(previewChar(self) == c)
    {
        nextChar(self);
        return 1;
    }
    else
        return 0;
}

static char eof(eccastlexer_t* self)
{
    return self->offset >= self->input->length;
}

static eccasttoktype_t syntaxError(eccastlexer_t* self, ecccharbuffer_t* message)
{
    eccobjerror_t* error = io_libecc_Error.syntaxError(self->text, message);
    self->parsedvalue = ECCNSValue.error(error);
    return ECC_TOK_ERROR;
}

// MARK: - Methods

eccastlexer_t* createWithInput(eccioinput_t* input)
{
    eccastlexer_t* self = malloc(sizeof(*self));
    *self = io_libecc_Lexer.identity;

    assert(input);
    self->input = input;

    return self;
}

void destroy(eccastlexer_t* self)
{
    assert(self);

    self->input = NULL;
    free(self), self = NULL;
}

eccasttoktype_t nextToken(eccastlexer_t* self)
{
    uint32_t c;
    assert(self);

    self->parsedvalue = ECCValConstUndefined;
    self->didLineBreak = 0;

retry:
    self->text.bytes = self->input->bytes + self->offset;
    self->text.length = 0;

    while((c = nextChar(self)))
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
                if(acceptChar(self, '*'))
                {
                    while(!eof(self))
                        if(nextChar(self) == '*' && acceptChar(self, '/'))
                            goto retry;

                    return syntaxError(self, io_libecc_Chars.create("unterminated comment"));
                }
                else if(previewChar(self) == '/')
                {
                    while((c = nextChar(self)))
                        if(c == '\r' || c == '\n')
                            goto retry;

                    return 0;
                }
                else if(self->allowRegex)
                {
                    while(!eof(self))
                    {
                        int c = nextChar(self);

                        if(c == '\\')
                            c = nextChar(self);
                        else if(c == '/')
                        {
                            while(isalnum(previewChar(self)) || previewChar(self) == '\\')
                                nextChar(self);

                            return ECC_TOK_REGEXP;
                        }

                        if(c == '\n')
                            break;
                    }
                    return syntaxError(self, io_libecc_Chars.create("unterminated regexp literal"));
                }
                else if(acceptChar(self, '='))
                    return ECC_TOK_DIVIDEASSIGN;
                else
                    return '/';
            }

            case '\'':
            case '"':
            {
                char end = c;
                int haveEscape = 0;
                int didLineBreak = self->didLineBreak;

                while((c = nextChar(self)))
                {
                    if(c == '\\')
                    {
                        haveEscape = 1;
                        nextChar(self);
                        self->didLineBreak = didLineBreak;
                    }
                    else if(c == end)
                    {
                        const char* bytes = self->text.bytes++;
                        uint32_t length = self->text.length -= 2;

                        if(haveEscape)
                        {
                            eccappendbuffer_t chars;
                            uint32_t index;

                            ++bytes;
                            --length;

                            io_libecc_Chars.beginAppend(&chars);

                            for(index = 0; index <= length; ++index)
                                if(bytes[index] == '\\' && bytes[++index] != '\\')
                                {
                                    switch(bytes[index])
                                    {
                                        case '0':
                                            io_libecc_Chars.appendCodepoint(&chars, '\0');
                                            break;
                                        case 'b':
                                            io_libecc_Chars.appendCodepoint(&chars, '\b');
                                            break;
                                        case 'f':
                                            io_libecc_Chars.appendCodepoint(&chars, '\f');
                                            break;
                                        case 'n':
                                            io_libecc_Chars.appendCodepoint(&chars, '\n');
                                            break;
                                        case 'r':
                                            io_libecc_Chars.appendCodepoint(&chars, '\r');
                                            break;
                                        case 't':
                                            io_libecc_Chars.appendCodepoint(&chars, '\t');
                                            break;
                                        case 'v':
                                            io_libecc_Chars.appendCodepoint(&chars, '\v');
                                            break;
                                        case 'x':
                                            if(isxdigit(bytes[index + 1]) && isxdigit(bytes[index + 2]))
                                            {
                                                io_libecc_Chars.appendCodepoint(&chars, uint8Hex(bytes[index + 1], bytes[index + 2]));
                                                index += 2;
                                                break;
                                            }
                                            self->text = ECCNSText.make(self->text.bytes + index - 1, 4);
                                            return syntaxError(self, io_libecc_Chars.create("malformed hexadecimal character escape sequence"));

                                        case 'u':
                                            if(isxdigit(bytes[index + 1]) && isxdigit(bytes[index + 2]) && isxdigit(bytes[index + 3]) && isxdigit(bytes[index + 4]))
                                            {
                                                io_libecc_Chars.appendCodepoint(&chars, uint16Hex(bytes[index + 1], bytes[index + 2], bytes[index + 3], bytes[index + 4]));
                                                index += 4;
                                                break;
                                            }
                                            self->text = ECCNSText.make(self->text.bytes + index - 1, 6);
                                            return syntaxError(self, io_libecc_Chars.create("malformed Unicode character escape sequence"));

                                        case '\r':
                                            if(bytes[index + 1] == '\n')
                                                ++index;
                                        case '\n':
                                            continue;

                                        default:
                                            io_libecc_Chars.append(&chars, "%c", bytes[index]);
                                    }
                                }
                                else
                                    io_libecc_Chars.append(&chars, "%c", bytes[index]);

                            self->parsedvalue = io_libecc_Input.attachValue(self->input, io_libecc_Chars.endAppend(&chars));
                            return ECC_TOK_ESCAPEDSTRING;
                        }

                        return ECC_TOK_STRING;
                    }
                    else if(c == '\r' || c == '\n')
                        break;
                }

                return syntaxError(self, io_libecc_Chars.create("unterminated string literal"));
            }

            case '.':
                if(!isdigit(previewChar(self)))
                    return c;

                /*vvv*/
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

                if(c == '0' && (acceptChar(self, 'x') || acceptChar(self, 'X')))
                {
                    while((c = previewChar(self)))
                        if(isxdigit(c))
                            nextChar(self);
                        else
                            break;

                    if(self->text.length <= 2)
                        return syntaxError(self, io_libecc_Chars.create("missing hexadecimal digits after '0x'"));
                }
                else
                {
                    while(isdigit(previewChar(self)))
                        nextChar(self);

                    if(c == '.' || acceptChar(self, '.'))
                        binary = 1;

                    while(isdigit(previewChar(self)))
                        nextChar(self);

                    if(acceptChar(self, 'e') || acceptChar(self, 'E'))
                    {
                        binary = 1;

                        if(!acceptChar(self, '+'))
                            acceptChar(self, '-');

                        if(!isdigit(previewChar(self)))
                            return syntaxError(self, io_libecc_Chars.create("missing exponent"));

                        while(isdigit(previewChar(self)))
                            nextChar(self);
                    }
                }

                if(isalpha(previewChar(self)))
                {
                    self->text.bytes += self->text.length;
                    self->text.length = 1;
                    return syntaxError(self, io_libecc_Chars.create("identifier starts immediately after numeric literal"));
                }

                if(binary)
                {
                    self->parsedvalue = scanBinary(self->text, 0);
                    return ECC_TOK_BINARY;
                }
                else
                {
                    self->parsedvalue = scanInteger(self->text, 0, 0);

                    if(self->parsedvalue.type == ECC_VALTYPE_INTEGER)
                        return ECC_TOK_INTEGER;
                    else
                        return ECC_TOK_BINARY;
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
                return c;

            case '^':
                if(acceptChar(self, '='))
                    return ECC_TOK_XORASSIGN;
                else
                    return c;

            case '%':
                if(acceptChar(self, '='))
                    return ECC_TOK_MODULOASSIGN;
                else
                    return c;

            case '*':
                if(acceptChar(self, '='))
                    return ECC_TOK_MULTIPLYASSIGN;
                else
                    return c;

            case '=':
                if(acceptChar(self, '='))
                {
                    if(acceptChar(self, '='))
                        return ECC_TOK_IDENTICAL;
                    else
                        return ECC_TOK_EQUAL;
                }
                else
                    return c;

            case '!':
                if(acceptChar(self, '='))
                {
                    if(acceptChar(self, '='))
                        return ECC_TOK_NOTIDENTICAL;
                    else
                        return ECC_TOK_NOTEQUAL;
                }
                else
                    return c;

            case '+':
                if(acceptChar(self, '+'))
                    return ECC_TOK_INCREMENT;
                else if(acceptChar(self, '='))
                    return ECC_TOK_ADDASSIGN;
                else
                    return c;

            case '-':
                if(acceptChar(self, '-'))
                    return ECC_TOK_DECREMENT;
                else if(acceptChar(self, '='))
                    return ECC_TOK_MINUSASSIGN;
                else
                    return c;

            case '&':
                if(acceptChar(self, '&'))
                    return ECC_TOK_LOGICALAND;
                else if(acceptChar(self, '='))
                    return ECC_TOK_ANDASSIGN;
                else
                    return c;

            case '|':
                if(acceptChar(self, '|'))
                    return ECC_TOK_LOGICALOR;
                else if(acceptChar(self, '='))
                    return ECC_TOK_ORASSIGN;
                else
                    return c;

            case '<':
                if(acceptChar(self, '<'))
                {
                    if(acceptChar(self, '='))
                        return ECC_TOK_LEFTSHIFTASSIGN;
                    else
                        return ECC_TOK_LEFTSHIFT;
                }
                else if(acceptChar(self, '='))
                    return ECC_TOK_LESSOREQUAL;
                else
                    return c;

            case '>':
                if(acceptChar(self, '>'))
                {
                    if(acceptChar(self, '>'))
                    {
                        if(acceptChar(self, '='))
                            return ECC_TOK_UNSIGNEDRIGHTSHIFTASSIGN;
                        else
                            return ECC_TOK_UNSIGNEDRIGHTSHIFT;
                    }
                    else if(acceptChar(self, '='))
                        return ECC_TOK_RIGHTSHIFTASSIGN;
                    else
                        return ECC_TOK_RIGHTSHIFT;
                }
                else if(acceptChar(self, '='))
                    return ECC_TOK_MOREOREQUAL;
                else
                    return c;

            default:
            {
                if((c < 0x80 && isalpha(c)) || c == '$' || c == '_' || (self->allowUnicodeOutsideLiteral && (c == '\\' || c >= 0x80)))
                {
                    ecctextstring_t text = self->text;
                    int k, haveEscape = 0;

                    do
                    {
                        if(c == '\\')
                        {
                            char uu = nextChar(self);
                            char u1 = nextChar(self);
                            char u2 = nextChar(self);
                            char u3 = nextChar(self);
                            char u4 = nextChar(self);

                            if(uu == 'u' && isxdigit(u1) && isxdigit(u2) && isxdigit(u3) && isxdigit(u4))
                            {
                                c = uint16Hex(u1, u2, u3, u4);
                                haveEscape = 1;
                            }
                            else
                                return syntaxError(self, io_libecc_Chars.create("incomplete unicode escape"));
                        }

                        if(ECCNSText.isSpace((ecctextchar_t){ c }))
                            break;

                        text = self->text;
                        c = nextChar(self);
                    } while(isalnum(c) || c == '$' || c == '_' || (self->allowUnicodeOutsideLiteral && (c == '\\' || c >= 0x80)));

                    self->text = text;
                    self->offset = (uint32_t)(text.bytes + text.length - self->input->bytes);

                    if(haveEscape)
                    {
                        ecctextstring_t text = self->text;
                        eccappendbuffer_t chars;
                        eccvalue_t value;

                        io_libecc_Chars.beginAppend(&chars);

                        while(text.length)
                        {
                            c = ECCNSText.nextCharacter(&text).codepoint;

                            if(c == '\\' && ECCNSText.nextCharacter(&text).codepoint == 'u')
                            {
                                char u1 = ECCNSText.nextCharacter(&text).codepoint, u2 = ECCNSText.nextCharacter(&text).codepoint,
                                     u3 = ECCNSText.nextCharacter(&text).codepoint, u4 = ECCNSText.nextCharacter(&text).codepoint;

                                if(isxdigit(u1) && isxdigit(u2) && isxdigit(u3) && isxdigit(u4))
                                    c = uint16Hex(u1, u2, u3, u4);
                                else
                                    ECCNSText.advance(&text, -5);
                            }

                            io_libecc_Chars.appendCodepoint(&chars, c);
                        }

                        value = io_libecc_Input.attachValue(self->input, io_libecc_Chars.endAppend(&chars));
                        self->parsedvalue = ECCNSValue.key(io_libecc_Key.makeWithText(ECCNSValue.textOf(&value), value.type != ECC_VALTYPE_CHARS));
                        return ECC_TOK_IDENTIFIER;
                    }

                    if(!self->disallowKeyword)
                    {
                        for(k = 0; k < sizeof(keywords) / sizeof(*keywords); ++k)
                            if(self->text.length == keywords[k].length && memcmp(self->text.bytes, keywords[k].name, keywords[k].length) == 0)
                                return keywords[k].token;

                        for(k = 0; k < sizeof(reservedKeywords) / sizeof(*reservedKeywords); ++k)
                            if(self->text.length == reservedKeywords[k].length && memcmp(self->text.bytes, reservedKeywords[k].name, reservedKeywords[k].length) == 0)
                                return syntaxError(self, io_libecc_Chars.create("'%s' is a reserved identifier", reservedKeywords[k]));
                    }

                    self->parsedvalue = ECCNSValue.key(io_libecc_Key.makeWithText(self->text, 0));
                    return ECC_TOK_IDENTIFIER;
                }
                else
                {
                    if(c >= 0x80)
                        return syntaxError(self, io_libecc_Chars.create("invalid character '%.*s'", self->text.length, self->text.bytes));
                    else if(isprint(c))
                        return syntaxError(self, io_libecc_Chars.create("invalid character '%c'", c));
                    else
                        return syntaxError(self, io_libecc_Chars.create("invalid character '\\%d'", c & 0xff));
                }
            }
        }
    }

    addLine(self, self->offset);
    return ECC_TOK_NO;
}

const char* tokenChars(eccasttoktype_t token, char buffer[4])
{
    int index;

    struct
    {
        const char* name;
        const eccasttoktype_t token;
    } static const tokenList[] = {
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

    for(index = 0; index < sizeof(tokenList); ++index)
        if(tokenList[index].token == token)
            return tokenList[index].name;

    assert(0);
    return "unknow";
}

eccvalue_t scanBinary(ecctextstring_t text, eccastlexflags_t flags)
{
    int lazy = flags & ECC_LEXFLAG_SCANLAZY;
    char buffer[text.length + 1];
    char* end = buffer;
    double binary = NAN;

    if(flags & ECC_LEXFLAG_SCANSLOPPY)
    {
        ecctextstring_t tail = ECCNSText.make(text.bytes + text.length, text.length);

        while(tail.length && ECCNSText.isSpace(ECCNSText.prevCharacter(&tail)))
            text.length = tail.length;

        while(text.length && ECCNSText.isSpace(ECCNSText.character(text)))
            ECCNSText.nextCharacter(&text);
    }
    else
        while(text.length && isspace(*text.bytes))
            ECCNSText.advance(&text, 1);

    memcpy(buffer, text.bytes, text.length);
    buffer[text.length] = '\0';

    if(text.length)
    {
        if(lazy && text.length >= 2 && buffer[0] == '0' && buffer[1] == 'x')
            return ECCNSValue.binary(0);

        if(text.length >= ECC_ConstString_Infinity.length && !memcmp(buffer, ECC_ConstString_Infinity.bytes, ECC_ConstString_Infinity.length))
            binary = INFINITY, end += ECC_ConstString_Infinity.length;
        else if(text.length >= ECC_ConstString_NegativeInfinity.length
                && !memcmp(buffer, ECC_ConstString_NegativeInfinity.bytes, ECC_ConstString_NegativeInfinity.length))
            binary = -INFINITY, end += ECC_ConstString_NegativeInfinity.length;
        else if(!isalpha(buffer[0]))
            binary = strtod(buffer, &end);

        if((!lazy && *end && !isspace(*end)) || (lazy && end == buffer))
            binary = NAN;
    }
    else if(!lazy)
        binary = 0;

    return ECCNSValue.binary(binary);
}

static double strtolHexFallback(ecctextstring_t text)
{
    double binary = 0;
    int sign = 1;
    int offset = 0;
    int c;

    if(text.bytes[offset] == '-')
        sign = -1, ++offset;

    if(text.length - offset > 1 && text.bytes[offset] == '0' && tolower(text.bytes[offset + 1]) == 'x')
    {
        offset += 2;

        while(text.length - offset >= 1)
        {
            c = text.bytes[offset++];

            binary *= 16;

            if(isdigit(c))
                binary += c - '0';
            else if(isxdigit(c))
                binary += tolower(c) - ('a' - 10);
            else
                return NAN;
        }
    }

    return binary * sign;
}

eccvalue_t scanInteger(ecctextstring_t text, int base, eccastlexflags_t flags)
{
    int lazy = flags & ECC_LEXFLAG_SCANLAZY;
    long integer;
    char buffer[text.length + 1];
    char* end;

    if(flags & ECC_LEXFLAG_SCANSLOPPY)
    {
        ecctextstring_t tail = ECCNSText.make(text.bytes + text.length, text.length);

        while(tail.length && ECCNSText.isSpace(ECCNSText.prevCharacter(&tail)))
            text.length = tail.length;

        while(text.length && ECCNSText.isSpace(ECCNSText.character(text)))
            ECCNSText.nextCharacter(&text);
    }
    else
        while(text.length && isspace(*text.bytes))
            ECCNSText.advance(&text, 1);

    memcpy(buffer, text.bytes, text.length);
    buffer[text.length] = '\0';

    errno = 0;
    integer = strtol(buffer, &end, base);

    if((lazy && end == buffer) || (!lazy && *end && !isspace(*end)))
        return ECCNSValue.binary(NAN);
    else if(errno == ERANGE)
    {
        if(!base || base == 10 || base == 16)
        {
            double binary = strtod(buffer, NULL);
            if(!binary && (!base || base == 16))
                binary = strtolHexFallback(text);

            return ECCNSValue.binary(binary);
        }

        ECCNSEnv.printWarning("`parseInt('%.*s', %d)` out of bounds; only long int are supported by radices other than 10 or 16", text.length, text.bytes, base);
        return ECCNSValue.binary(NAN);
    }
    else if(integer < INT32_MIN || integer > INT32_MAX)
        return ECCNSValue.binary(integer);
    else
        return ECCNSValue.integer((int32_t)integer);
}

uint32_t scanElement(ecctextstring_t text)
{
    eccvalue_t value;
    uint16_t index;

    if(!text.length)
        return UINT32_MAX;

    for(index = 0; index < text.length; ++index)
        if(!isdigit(text.bytes[index]))
            return UINT32_MAX;

    value = scanInteger(text, 0, 0);

    if(value.type == ECC_VALTYPE_INTEGER)
        return value.data.integer;
    if(value.type == ECC_VALTYPE_BINARY && value.data.binary >= 0 && value.data.binary < UINT32_MAX && value.data.binary == (uint32_t)value.data.binary)
        return value.data.binary;
    else
        return UINT32_MAX;
}

static inline int8_t hexhigit(int c)
{
    if(c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    else if(c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    else
        return c - '0';
}

uint8_t uint8Hex(char a, char b)
{
    return hexhigit(a) << 4 | hexhigit(b);
}

uint16_t uint16Hex(char a, char b, char c, char d)
{
    return hexhigit(a) << 12 | hexhigit(b) << 8 | hexhigit(c) << 4 | hexhigit(d);
}
