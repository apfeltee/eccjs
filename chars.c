//
//  chars.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"

// MARK: - Private

static ecccharbuffer_t* nsccharsfn_createVA(int32_t length, const char* format, va_list ap);
static ecccharbuffer_t* nsccharsfn_create(const char* format, ...);
static ecccharbuffer_t* nsccharsfn_createSized(int32_t length);
static ecccharbuffer_t* nsccharsfn_createWithBytes(int32_t length, const char* bytes);
static void nsccharsfn_beginAppend(eccappendbuffer_t*);
static void nsccharsfn_append(eccappendbuffer_t*, const char* format, ...);
static void nsccharsfn_appendCodepoint(eccappendbuffer_t*, uint32_t cp);
static void nsccharsfn_appendValue(eccappendbuffer_t*, eccstate_t* context, eccvalue_t value);
static void nsccharsfn_appendBinary(eccappendbuffer_t*, double binary, int base);
static void nsccharsfn_normalizeBinary(eccappendbuffer_t*);
static eccvalue_t nsccharsfn_endAppend(eccappendbuffer_t*);
static void nsccharsfn_destroy(ecccharbuffer_t*);
static uint8_t nsccharsfn_codepointLength(uint32_t cp);
static uint8_t nsccharsfn_writeCodepoint(char*, uint32_t cp);
const struct eccpseudonschars_t ECCNSChars = {
    nsccharsfn_createVA,    nsccharsfn_create,       nsccharsfn_createSized,     nsccharsfn_createWithBytes, nsccharsfn_beginAppend, nsccharsfn_append,          nsccharsfn_appendCodepoint,
    nsccharsfn_appendValue, nsccharsfn_appendBinary, nsccharsfn_normalizeBinary, nsccharsfn_endAppend,       nsccharsfn_destroy,     nsccharsfn_codepointLength, nsccharsfn_writeCodepoint,
    {}
};

static inline uint32_t eccchars_nextPowerOfTwo(uint32_t v)
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

static inline uint32_t eccchars_sizeForLength(uint32_t length)
{
    uint32_t size = sizeof(ecccharbuffer_t) + length;

    if(size < 16)
        return 16;
    else
        return eccchars_nextPowerOfTwo(size);
}

static ecccharbuffer_t* eccchars_reuseOrCreate(eccappendbuffer_t* chars, uint32_t length)
{
    ecccharbuffer_t *self = NULL, *reuse = chars ? chars->value : NULL;

    if(reuse && eccchars_sizeForLength(reuse->length) >= eccchars_sizeForLength(length))
        return reuse;
    //	else
    //		chars = ECCNSMemoryPool.reusableChars(length);

    if(!self)
    {
        if(length < 8)
            return NULL;

        self = malloc(eccchars_sizeForLength(length));
        ECCNSMemoryPool.addChars(self);
    }

    if(reuse)
        memcpy(self, reuse, sizeof(*self) + reuse->length);
    else
    {
        *self = ECCNSChars.identity;
        self->length = chars->units;
        memcpy(self->bytes, chars->buffer, chars->units);
    }

    chars->value = self;
    return self;
}

// MARK: - Static Members

// MARK: - Methods

ecccharbuffer_t* nsccharsfn_createVA(int32_t length, const char* format, va_list ap)
{
    ecccharbuffer_t* self;

    self = nsccharsfn_createSized(length);
    vsprintf(self->bytes, format, ap);

    return self;
}

ecccharbuffer_t* nsccharsfn_create(const char* format, ...)
{
    uint16_t length;
    va_list ap;
    ecccharbuffer_t* self;

    va_start(ap, format);
    length = vsnprintf(NULL, 0, format, ap);
    va_end(ap);

    va_start(ap, format);
    self = nsccharsfn_createVA(length, format, ap);
    va_end(ap);

    return self;
}

ecccharbuffer_t* nsccharsfn_createSized(int32_t length)
{
    ecccharbuffer_t* self = malloc(eccchars_sizeForLength(length));
    ECCNSMemoryPool.addChars(self);
    *self = ECCNSChars.identity;

    self->length = length;
    self->bytes[length] = '\0';

    return self;
}

ecccharbuffer_t* nsccharsfn_createWithBytes(int32_t length, const char* bytes)
{
    ecccharbuffer_t* self = malloc(eccchars_sizeForLength(length));
    ECCNSMemoryPool.addChars(self);
    *self = ECCNSChars.identity;

    self->length = length;
    memcpy(self->bytes, bytes, length);
    self->bytes[length] = '\0';

    return self;
}

void nsccharsfn_beginAppend(eccappendbuffer_t* chars)
{
    chars->value = NULL;
    chars->units = 0;
}

void nsccharsfn_append(eccappendbuffer_t* chars, const char* format, ...)
{
    uint32_t length;
    va_list ap;
    ecccharbuffer_t* self = chars->value;

    va_start(ap, format);
    length = vsnprintf(NULL, 0, format, ap);
    va_end(ap);

    self = eccchars_reuseOrCreate(chars, (self ? self->length : chars->units) + length);

    va_start(ap, format);
    vsprintf(self ? (self->bytes + self->length) : (chars->buffer + chars->units), format, ap);
    va_end(ap);

    if(self)
        self->length += length;
    else
        chars->units += length;
}

static void eccchars_appendText(eccappendbuffer_t* chars, ecctextstring_t text)
{
    ecccharbuffer_t* self = chars->value;
    ecctextchar_t lo = ECCNSText.character(text), hi = { 0 };
    ecctextstring_t prev;
    int surrogates = 0;

    if(!text.length)
        return;

    if(self)
        prev = ECCNSText.make(self->bytes + self->length, self->length);
    else
        prev = ECCNSText.make(chars->buffer + chars->units, chars->units);

    if(lo.units == 3 && lo.codepoint >= 0xDC00 && lo.codepoint <= 0xDFFF)
    {
        hi = ECCNSText.prevCharacter(&prev);
        if(hi.units == 3 && hi.codepoint >= 0xD800 && hi.codepoint <= 0xDBFF)
        {
            surrogates = 1;
            ECCNSText.nextCharacter(&text);
            if(self)
                self->length = prev.length;
            else
                chars->units = prev.length;
        }
    }

    self = eccchars_reuseOrCreate(chars, (self ? self->length : chars->units) + text.length);

    if(surrogates)
    {
        uint32_t cp = 0x10000 + (((hi.codepoint - 0xD800) << 10) | ((lo.codepoint - 0xDC00) & 0x03FF));

        if(self)
            self->length += nsccharsfn_writeCodepoint(self->bytes + self->length, cp);
        else
            chars->units += nsccharsfn_writeCodepoint(chars->buffer + chars->units, cp);
    }

    memcpy(self ? (self->bytes + self->length) : (chars->buffer + chars->units), text.bytes, text.length);
    if(self)
        self->length += text.length;
    else
        chars->units += text.length;
}

void nsccharsfn_appendCodepoint(eccappendbuffer_t* chars, uint32_t cp)
{
    char buffer[5] = { 0 };
    ecctextstring_t text = ECCNSText.make(buffer, nsccharsfn_writeCodepoint(buffer, cp));
    eccchars_appendText(chars, text);
}

void nsccharsfn_appendValue(eccappendbuffer_t* chars, eccstate_t* context, eccvalue_t value)
{
    switch((eccvaltype_t)value.type)
    {
        case ECC_VALTYPE_KEY:
        case ECC_VALTYPE_TEXT:
        case ECC_VALTYPE_STRING:
        case ECC_VALTYPE_CHARS:
        case ECC_VALTYPE_BUFFER:
            eccchars_appendText(chars, ECCNSValue.textOf(&value));
            return;

        case ECC_VALTYPE_NULL:
            eccchars_appendText(chars, ECC_ConstString_Null);
            return;

        case ECC_VALTYPE_UNDEFINED:
            eccchars_appendText(chars, ECC_ConstString_Undefined);
            return;

        case ECC_VALTYPE_FALSE:
            eccchars_appendText(chars, ECC_ConstString_False);
            return;

        case ECC_VALTYPE_TRUE:
            eccchars_appendText(chars, ECC_ConstString_True);
            return;

        case ECC_VALTYPE_BOOLEAN:
            eccchars_appendText(chars, value.data.boolean->truth ? ECC_ConstString_True : ECC_ConstString_False);
            return;

        case ECC_VALTYPE_INTEGER:
            nsccharsfn_appendBinary(chars, value.data.integer, 10);
            return;

        case ECC_VALTYPE_NUMBER:
            nsccharsfn_appendBinary(chars, value.data.number->value, 10);
            return;

        case ECC_VALTYPE_BINARY:
            nsccharsfn_appendBinary(chars, value.data.binary, 10);
            return;

        case ECC_VALTYPE_REGEXP:
        case ECC_VALTYPE_FUNCTION:
        case ECC_VALTYPE_OBJECT:
        case ECC_VALTYPE_ERROR:
        case ECC_VALTYPE_DATE:
        case ECC_VALTYPE_HOST:
            nsccharsfn_appendValue(chars, context, ECCNSValue.toString(context, value));
            return;

        case ECC_VALTYPE_REFERENCE:
            break;
    }
    ECCNSScript.fatal("Invalid value type : %u", value.type);
}

static uint32_t eccchars_stripBinaryOfBytes(char* bytes, uint32_t length)
{
    while(bytes[length - 1] == '0')
        bytes[--length] = '\0';

    if(bytes[length - 1] == '.')
        bytes[--length] = '\0';

    return length;
}

static uint32_t eccchars_normalizeBinaryOfBytes(char* bytes, uint32_t length)
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

void nsccharsfn_appendBinary(eccappendbuffer_t* chars, double binary, int base)
{
    if(isnan(binary))
    {
        eccchars_appendText(chars, ECC_ConstString_Nan);
        return;
    }
    else if(!isfinite(binary))
    {
        if(binary < 0)
            eccchars_appendText(chars, ECC_ConstString_NegativeInfinity);
        else
            eccchars_appendText(chars, ECC_ConstString_Infinity);

        return;
    }

    if(!base || base == 10)
    {
        if(binary <= -1e+21 || binary >= 1e+21)
            nsccharsfn_append(chars, "%g", binary);
        else if((binary < 1 && binary >= 0.000001) || (binary > -1 && binary <= -0.000001))
        {
            nsccharsfn_append(chars, "%.10f", binary);
            if(chars->value)
                chars->value->length = eccchars_stripBinaryOfBytes(chars->value->bytes, chars->value->length);
            else
                chars->units = eccchars_stripBinaryOfBytes(chars->buffer, chars->units);

            return;
        }
        else
        {
            double dblDig10 = pow(10, DBL_DIG);
            int precision = binary >= -dblDig10 && binary <= dblDig10 ? DBL_DIG : 21;

            nsccharsfn_append(chars, "%.*g", precision, binary);
        }

        nsccharsfn_normalizeBinary(chars);
        return;
    }
    else
    {
        int sign = binary < 0;
        unsigned long integer = sign ? -binary : binary;

        if(base == 8 || base == 16)
        {
            const char* format = sign ? (base == 8 ? "-%lo" : "-%lx") : (base == 8 ? "%lo" : "%lx");

            nsccharsfn_append(chars, format, integer);
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
            nsccharsfn_append(chars, "%.*s", count, p);
        }
    }
}

void nsccharsfn_normalizeBinary(eccappendbuffer_t* chars)
{
    if(chars->value)
        chars->value->length = eccchars_normalizeBinaryOfBytes(chars->value->bytes, chars->value->length);
    else
        chars->units = eccchars_normalizeBinaryOfBytes(chars->buffer, chars->units);
}

eccvalue_t nsccharsfn_endAppend(eccappendbuffer_t* chars)
{
    ecccharbuffer_t* self = chars->value;

    if(chars->value)
    {
        self->bytes[self->length] = '\0';
        return ECCNSValue.chars(self);
    }
    else
        return ECCNSValue.buffer(chars->buffer, chars->units);
}

void nsccharsfn_destroy(ecccharbuffer_t* self)
{
    assert(self);

    free(self), self = NULL;
}

uint8_t nsccharsfn_codepointLength(uint32_t cp)
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

uint8_t nsccharsfn_writeCodepoint(char* bytes, uint32_t cp)
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
