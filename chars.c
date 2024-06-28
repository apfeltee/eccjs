//
//  chars.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"


uint32_t ecc_strbuf_nextpoweroftwo(uint32_t v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

uint32_t ecc_strbuf_sizeforlength(uint32_t length)
{
    uint32_t size = sizeof(eccstrbuffer_t) + length;

    if(size < 16)
        return 16;
    else
        return ecc_strbuf_nextpoweroftwo(size);
}

eccstrbuffer_t* ecc_strbuf_reuseorcreate(eccappendbuffer_t* chars, uint32_t length)
{
    eccstrbuffer_t *self = NULL, *reuse = chars ? chars->sbufvalue : NULL;

    if(reuse && ecc_strbuf_sizeforlength(reuse->length) >= ecc_strbuf_sizeforlength(length))
        return reuse;
    //	else
    //		chars = ecc_mempool_reusablechars(length);

    if(!self)
    {
        if(length < 8)
            return NULL;

        self = (eccstrbuffer_t*)malloc(ecc_strbuf_sizeforlength(length));
        ecc_mempool_addchars(self);
    }

    if(reuse)
        memcpy(self, reuse, sizeof(*self) + reuse->length);
    else
    {
        memset(self, 0, sizeof(eccstrbuffer_t));
        self->length = chars->units;
        memcpy(self->bytes, chars->buffer, chars->units);
    }

    chars->sbufvalue = self;
    return self;
}

// MARK: - Static Members

// MARK: - Methods

eccstrbuffer_t* ecc_strbuf_createva(int32_t length, const char* format, va_list ap)
{
    eccstrbuffer_t* self;

    self = ecc_strbuf_createsized(length);
    vsprintf(self->bytes, format, ap);

    return self;
}

eccstrbuffer_t* ecc_strbuf_create(const char* format, ...)
{
    uint16_t length;
    va_list ap;
    eccstrbuffer_t* self;

    va_start(ap, format);
    length = vsnprintf(NULL, 0, format, ap);
    va_end(ap);

    va_start(ap, format);
    self = ecc_strbuf_createva(length, format, ap);
    va_end(ap);

    return self;
}

eccstrbuffer_t* ecc_strbuf_createsized(int32_t length)
{
    eccstrbuffer_t* self = (eccstrbuffer_t*)malloc(ecc_strbuf_sizeforlength(length));
    ecc_mempool_addchars(self);
    memset(self, 0, sizeof(eccstrbuffer_t));

    self->length = length;
    self->bytes[length] = '\0';

    return self;
}

eccstrbuffer_t* ecc_strbuf_createwithbytes(int32_t length, const char* bytes)
{
    eccstrbuffer_t* self = (eccstrbuffer_t*)malloc(ecc_strbuf_sizeforlength(length));
    ecc_mempool_addchars(self);
    memset(self, 0, sizeof(eccstrbuffer_t));

    self->length = length;
    memcpy(self->bytes, bytes, length);
    self->bytes[length] = '\0';

    return self;
}

void ecc_strbuf_beginappend(eccappendbuffer_t* chars)
{
    chars->sbufvalue = NULL;
    chars->units = 0;
}

void ecc_strbuf_append(eccappendbuffer_t* chars, const char* format, ...)
{
    uint32_t length;
    va_list ap;
    eccstrbuffer_t* self = chars->sbufvalue;

    va_start(ap, format);
    length = vsnprintf(NULL, 0, format, ap);
    va_end(ap);

    self = ecc_strbuf_reuseorcreate(chars, (self ? self->length : chars->units) + length);

    va_start(ap, format);
    vsprintf(self ? (self->bytes + self->length) : (chars->buffer + chars->units), format, ap);
    va_end(ap);

    if(self)
        self->length += length;
    else
        chars->units += length;
}

void ecc_strbuf_appendtext(eccappendbuffer_t* chars, ecctextstring_t text)
{
    eccstrbuffer_t* self = chars->sbufvalue;
    ecctextchar_t lo = ecc_textbuf_character(text), hi = { 0 };
    ecctextstring_t prev;
    int surrogates = 0;

    if(!text.length)
        return;

    if(self)
        prev = ecc_textbuf_make(self->bytes + self->length, self->length);
    else
        prev = ecc_textbuf_make(chars->buffer + chars->units, chars->units);

    if(lo.units == 3 && lo.codepoint >= 0xDC00 && lo.codepoint <= 0xDFFF)
    {
        hi = ecc_textbuf_prevcharacter(&prev);
        if(hi.units == 3 && hi.codepoint >= 0xD800 && hi.codepoint <= 0xDBFF)
        {
            surrogates = 1;
            ecc_textbuf_nextcharacter(&text);
            if(self)
                self->length = prev.length;
            else
                chars->units = prev.length;
        }
    }

    self = ecc_strbuf_reuseorcreate(chars, (self ? self->length : chars->units) + text.length);

    if(surrogates)
    {
        uint32_t cp = 0x10000 + (((hi.codepoint - 0xD800) << 10) | ((lo.codepoint - 0xDC00) & 0x03FF));

        if(self)
            self->length += ecc_strbuf_writecodepoint(self->bytes + self->length, cp);
        else
            chars->units += ecc_strbuf_writecodepoint(chars->buffer + chars->units, cp);
    }

    memcpy(self ? (self->bytes + self->length) : (chars->buffer + chars->units), text.bytes, text.length);
    if(self)
        self->length += text.length;
    else
        chars->units += text.length;
}

void ecc_strbuf_appendcodepoint(eccappendbuffer_t* chars, uint32_t cp)
{
    char buffer[5] = { 0 };
    ecctextstring_t text = ecc_textbuf_make(buffer, ecc_strbuf_writecodepoint(buffer, cp));
    ecc_strbuf_appendtext(chars, text);
}

void ecc_strbuf_appendvalue(eccappendbuffer_t* chars, ecccontext_t* context, eccvalue_t value)
{
    switch((eccvaltype_t)value.type)
    {
        case ECC_VALTYPE_KEY:
        case ECC_VALTYPE_TEXT:
        case ECC_VALTYPE_STRING:
        case ECC_VALTYPE_CHARS:
        case ECC_VALTYPE_BUFFER:
            ecc_strbuf_appendtext(chars, ecc_value_textof(&value));
            return;

        case ECC_VALTYPE_NULL:
            ecc_strbuf_appendtext(chars, ECC_ConstString_Null);
            return;

        case ECC_VALTYPE_UNDEFINED:
            ecc_strbuf_appendtext(chars, ECC_ConstString_Undefined);
            return;

        case ECC_VALTYPE_FALSE:
            ecc_strbuf_appendtext(chars, ECC_ConstString_False);
            return;

        case ECC_VALTYPE_TRUE:
            ecc_strbuf_appendtext(chars, ECC_ConstString_True);
            return;

        case ECC_VALTYPE_BOOLEAN:
            ecc_strbuf_appendtext(chars, value.data.boolean->truth ? ECC_ConstString_True : ECC_ConstString_False);
            return;

        case ECC_VALTYPE_INTEGER:
            ecc_strbuf_appendbinary(chars, value.data.integer, 10);
            return;

        case ECC_VALTYPE_NUMBER:
            ecc_strbuf_appendbinary(chars, value.data.number->value, 10);
            return;

        case ECC_VALTYPE_BINARY:
            ecc_strbuf_appendbinary(chars, value.data.binary, 10);
            return;

        case ECC_VALTYPE_REGEXP:
        case ECC_VALTYPE_FUNCTION:
        case ECC_VALTYPE_OBJECT:
        case ECC_VALTYPE_ERROR:
        case ECC_VALTYPE_DATE:
        case ECC_VALTYPE_HOST:
            ecc_strbuf_appendvalue(chars, context, ecc_value_tostring(context, value));
            return;

        case ECC_VALTYPE_REFERENCE:
            break;
    }
    ecc_script_fatal("Invalid value type : %u", value.type);
}

uint32_t ecc_strbuf_stripbinaryofbytes(char* bytes, uint32_t length)
{
    while(bytes[length - 1] == '0')
        bytes[--length] = '\0';

    if(bytes[length - 1] == '.')
        bytes[--length] = '\0';

    return length;
}

uint32_t ecc_strbuf_normalizebinaryofbytes(char* bytes, uint32_t length)
{
    if(length > 5 && bytes[length - 5] == 'e' && bytes[length - 3] == '0')
    {
        bytes[length - 3] = bytes[length - 2];
        bytes[length - 2] = bytes[length - 1];
        bytes[length - 1] = 0;
        --length;
    }
    else if(length > 4 && bytes[length - 4] == 'e' && bytes[length - 2] == '0')
    {
        bytes[length - 2] = bytes[length - 1];
        bytes[length - 1] = 0;
        --length;
    }
    return length;
}

void ecc_strbuf_appendbinary(eccappendbuffer_t* chars, double binary, int base)
{
    if(isnan(binary))
    {
        ecc_strbuf_appendtext(chars, ECC_ConstString_Nan);
        return;
    }
    else if(!isfinite(binary))
    {
        if(binary < 0)
            ecc_strbuf_appendtext(chars, ECC_ConstString_NegativeInfinity);
        else
            ecc_strbuf_appendtext(chars, ECC_ConstString_Infinity);

        return;
    }

    if(!base || base == 10)
    {
        if(binary <= -1e+21 || binary >= 1e+21)
            ecc_strbuf_append(chars, "%g", binary);
        else if((binary < 1 && binary >= 0.000001) || (binary > -1 && binary <= -0.000001))
        {
            ecc_strbuf_append(chars, "%.10f", binary);
            if(chars->sbufvalue)
                chars->sbufvalue->length = ecc_strbuf_stripbinaryofbytes(chars->sbufvalue->bytes, chars->sbufvalue->length);
            else
                chars->units = ecc_strbuf_stripbinaryofbytes(chars->buffer, chars->units);

            return;
        }
        else
        {
            double dblDig10 = pow(10, DBL_DIG);
            int precision = binary >= -dblDig10 && binary <= dblDig10 ? DBL_DIG : 21;

            ecc_strbuf_append(chars, "%.*g", precision, binary);
        }

        ecc_strbuf_normalizebinary(chars);
        return;
    }
    else
    {
        int sign = binary < 0;
        unsigned long integer = sign ? -binary : binary;

        if(base == 8 || base == 16)
        {
            const char* format = sign ? (base == 8 ? "-%lo" : "-%lx") : (base == 8 ? "%lo" : "%lx");

            ecc_strbuf_append(chars, format, integer);
        }
        else
        {
            static char const digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
            char buffer[1 + sizeof(integer) * CHAR_BIT];
            char* p = buffer + sizeof(buffer) - 1;
            uint16_t count;

            while(integer)
            {
                *(--p) = digits[integer % base];
                integer /= base;
            }
            if(sign)
                *(--p) = '-';

            count = buffer + sizeof(buffer) - 1 - p;
            ecc_strbuf_append(chars, "%.*s", count, p);
        }
    }
}

void ecc_strbuf_normalizebinary(eccappendbuffer_t* chars)
{
    if(chars->sbufvalue)
        chars->sbufvalue->length = ecc_strbuf_normalizebinaryofbytes(chars->sbufvalue->bytes, chars->sbufvalue->length);
    else
        chars->units = ecc_strbuf_normalizebinaryofbytes(chars->buffer, chars->units);
}

eccvalue_t ecc_strbuf_endappend(eccappendbuffer_t* chars)
{
    eccstrbuffer_t* self = chars->sbufvalue;

    if(chars->sbufvalue)
    {
        self->bytes[self->length] = '\0';
        return ecc_value_fromchars(self);
    }
    else
        return ecc_value_buffer(chars->buffer, chars->units);
}

void ecc_strbuf_destroy(eccstrbuffer_t* self)
{
    assert(self);

    free(self), self = NULL;
}

uint8_t ecc_strbuf_codepointlength(uint32_t cp)
{
    if(cp < 0x80)
        return 1;
    else if(cp < 0x800)
        return 2;
    else if(cp < 0x10000)
        return 3;
    else
        return 4;

    return 0;
}

uint8_t ecc_strbuf_writecodepoint(char* bytes, uint32_t cp)
{
    if(cp < 0x80)
    {
        bytes[0] = cp;
        return 1;
    }
    else if(cp < 0x800)
    {
        bytes[0] = 0xC0 | (cp >> 6);
        bytes[1] = 0x80 | (cp & 0x3F);
        return 2;
    }
    else if(cp < 0x10000)
    {
        bytes[0] = 0xE0 | (cp >> 12);
        bytes[1] = 0x80 | (cp >> 6 & 0x3F);
        bytes[2] = 0x80 | (cp & 0x3F);
        return 3;
    }
    else
    {
        bytes[0] = 0xF0 | (cp >> 18);
        bytes[1] = 0x80 | (cp >> 12 & 0x3F);
        bytes[2] = 0x80 | (cp >> 6 & 0x3F);
        bytes[3] = 0x80 | (cp & 0x3F);
        return 4;
    }

    return 0;
}
