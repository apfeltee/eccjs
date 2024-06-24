//
//  chars.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"

// MARK: - Private

static struct io_libecc_Chars* createVA(int32_t length, const char* format, va_list ap);
static struct io_libecc_Chars* create(const char* format, ...);
static struct io_libecc_Chars* createSized(int32_t length);
static struct io_libecc_Chars* createWithBytes(int32_t length, const char* bytes);
static void beginAppend(struct io_libecc_chars_Append*);
static void append(struct io_libecc_chars_Append*, const char* format, ...);
static void appendCodepoint(struct io_libecc_chars_Append*, uint32_t cp);
static void appendValue(struct io_libecc_chars_Append*, eccstate_t* const context, eccvalue_t value);
static void appendBinary(struct io_libecc_chars_Append*, double binary, int base);
static void normalizeBinary(struct io_libecc_chars_Append*);
static eccvalue_t endAppend(struct io_libecc_chars_Append*);
static void destroy(struct io_libecc_Chars*);
static uint8_t codepointLength(uint32_t cp);
static uint8_t writeCodepoint(char*, uint32_t cp);
const struct type_io_libecc_Chars io_libecc_Chars = {
    createVA,    create,       createSized,     createWithBytes, beginAppend, append,          appendCodepoint,
    appendValue, appendBinary, normalizeBinary, endAppend,       destroy,     codepointLength, writeCodepoint,
};

static inline
uint32_t nextPowerOfTwo(uint32_t v)
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

static inline
uint32_t sizeForLength(uint32_t length)
{
	uint32_t size = sizeof(struct io_libecc_Chars) + length;
	
	if (size < 16)
		return 16;
	else
		return nextPowerOfTwo(size);
}

static
struct io_libecc_Chars *reuseOrCreate (struct io_libecc_chars_Append *chars, uint32_t length)
{
	struct io_libecc_Chars *self = NULL, *reuse = chars? chars->value: NULL;
	
	if (reuse && sizeForLength(reuse->length) >= sizeForLength(length))
		return reuse;
//	else
//		chars = io_libecc_Pool.reusableChars(length);
	
	if (!self)
	{
		if (length < 8)
			return NULL;
		
		self = malloc(sizeForLength(length));
		io_libecc_Pool.addChars(self);
	}
	
	if (reuse)
		memcpy(self, reuse, sizeof(*self) + reuse->length);
	else
	{
		*self = io_libecc_Chars.identity;
		self->length = chars->units;
		memcpy(self->bytes, chars->buffer, chars->units);
	}
	
	chars->value = self;
	return self;
}

// MARK: - Static Members

// MARK: - Methods

struct io_libecc_Chars * createVA (int32_t length, const char *format, va_list ap)
{
	struct io_libecc_Chars *self;
	
	self = createSized(length);
	vsprintf(self->bytes, format, ap);
	
	return self;
}

struct io_libecc_Chars * create (const char *format, ...)
{
	uint16_t length;
	va_list ap;
	struct io_libecc_Chars *self;
	
	va_start(ap, format);
	length = vsnprintf(NULL, 0, format, ap);
	va_end(ap);
	
	va_start(ap, format);
	self = createVA(length, format, ap);
	va_end(ap);
	
	return self;
}

struct io_libecc_Chars * createSized (int32_t length)
{
	struct io_libecc_Chars *self = malloc(sizeForLength(length));
	io_libecc_Pool.addChars(self);
	*self = io_libecc_Chars.identity;
	
	self->length = length;
	self->bytes[length] = '\0';
	
	return self;
}

struct io_libecc_Chars * createWithBytes (int32_t length, const char *bytes)
{
	struct io_libecc_Chars *self = malloc(sizeForLength(length));
	io_libecc_Pool.addChars(self);
	*self = io_libecc_Chars.identity;
	
	self->length = length;
	memcpy(self->bytes, bytes, length);
	self->bytes[length] = '\0';
	
	return self;
}


void beginAppend (struct io_libecc_chars_Append *chars)
{
	chars->value = NULL;
	chars->units = 0;
}

void append (struct io_libecc_chars_Append *chars, const char *format, ...)
{
	uint32_t length;
	va_list ap;
	struct io_libecc_Chars *self = chars->value;
	
	va_start(ap, format);
	length = vsnprintf(NULL, 0, format, ap);
	va_end(ap);
	
	self = reuseOrCreate(chars, (self? self->length: chars->units) + length);
	
	va_start(ap, format);
	vsprintf(self? (self->bytes + self->length): (chars->buffer + chars->units), format, ap);
	va_end(ap);
	
	if (self)
		self->length += length;
	else
		chars->units += length;
}

static
void appendText (struct io_libecc_chars_Append * chars, ecctextstring_t text)
{
	struct io_libecc_Chars *self = chars->value;
	ecctextchar_t lo = ECCNSText.character(text), hi = { 0 };
	ecctextstring_t prev;
	int surrogates = 0;
	
	if (!text.length)
		return;
	
	if (self)
		prev = ECCNSText.make(self->bytes + self->length, self->length);
	else
		prev = ECCNSText.make(chars->buffer + chars->units, chars->units);
	
	if (lo.units == 3 && lo.codepoint >= 0xDC00 && lo.codepoint <= 0xDFFF)
	{
		hi = ECCNSText.prevCharacter(&prev);
		if (hi.units == 3 && hi.codepoint >= 0xD800 && hi.codepoint <= 0xDBFF)
		{
			surrogates = 1;
			ECCNSText.nextCharacter(&text);
			if (self)
				self->length = prev.length;
			else
				chars->units = prev.length;
		}
	}
	
	self = reuseOrCreate(chars, (self? self->length: chars->units) + text.length);
	
	if (surrogates)
	{
		uint32_t cp = 0x10000 + (((hi.codepoint - 0xD800) << 10) | ((lo.codepoint - 0xDC00) & 0x03FF));
		
		if (self)
			self->length += writeCodepoint(self->bytes + self->length, cp);
		else
			chars->units += writeCodepoint(chars->buffer + chars->units, cp);
	}
	
	memcpy(self? (self->bytes + self->length): (chars->buffer + chars->units), text.bytes, text.length);
	if (self)
		self->length += text.length;
	else
		chars->units += text.length;
}

void appendCodepoint (struct io_libecc_chars_Append *chars, uint32_t cp)
{
	char buffer[5] = { 0 };
	ecctextstring_t text = ECCNSText.make(buffer, writeCodepoint(buffer, cp));
	appendText(chars, text);
}

void appendValue (struct io_libecc_chars_Append *chars, eccstate_t * const context, eccvalue_t value)
{
	switch ((eccvaltype_t)value.type)
	{
		case ECC_VALTYPE_KEY:
		case ECC_VALTYPE_TEXT:
		case ECC_VALTYPE_STRING:
		case ECC_VALTYPE_CHARS:
		case ECC_VALTYPE_BUFFER:
			appendText(chars, ECCNSValue.textOf(&value));
			return;
			
		case ECC_VALTYPE_NULL:
			appendText(chars, ECC_ConstString_Null);
			return;
			
		case ECC_VALTYPE_UNDEFINED:
			appendText(chars, ECC_ConstString_Undefined);
			return;
			
		case ECC_VALTYPE_FALSE:
			appendText(chars, ECC_ConstString_False);
			return;
			
		case ECC_VALTYPE_TRUE:
			appendText(chars, ECC_ConstString_True);
			return;
			
		case ECC_VALTYPE_BOOLEAN:
			appendText(chars, value.data.boolean->truth? ECC_ConstString_True: ECC_ConstString_False);
			return;
			
		case ECC_VALTYPE_INTEGER:
			appendBinary(chars, value.data.integer, 10);
			return;
			
		case ECC_VALTYPE_NUMBER:
			appendBinary(chars, value.data.number->value, 10);
			return;
			
		case ECC_VALTYPE_BINARY:
			appendBinary(chars, value.data.binary, 10);
			return;
			
		case ECC_VALTYPE_REGEXP:
		case ECC_VALTYPE_FUNCTION:
		case ECC_VALTYPE_OBJECT:
		case ECC_VALTYPE_ERROR:
		case ECC_VALTYPE_DATE:
		case ECC_VALTYPE_HOST:
			appendValue(chars, context, ECCNSValue.toString(context, value));
			return;
			
		case ECC_VALTYPE_REFERENCE:
			break;
	}
	ECCNSScript.fatal("Invalid io_libecc_value_Type : %u", value.type);
}

static
uint32_t stripBinaryOfBytes (char *bytes, uint32_t length)
{
	while (bytes[length - 1] == '0')
		bytes[--length] = '\0';
	
	if (bytes[length - 1] == '.')
		bytes[--length] = '\0';
	
	return length;
}

static
uint32_t normalizeBinaryOfBytes (char *bytes, uint32_t length)
{
	if (length > 5 && bytes[length - 5] == 'e' && bytes[length - 3] == '0')
	{
		bytes[length - 3] = bytes[length - 2];
		bytes[length - 2] = bytes[length - 1];
		bytes[length - 1] = 0;
		--length;
	}
	else if (length > 4 && bytes[length - 4] == 'e' && bytes[length - 2] == '0')
	{
		bytes[length - 2] = bytes[length - 1];
		bytes[length - 1] = 0;
		--length;
	}
	return length;
}

void appendBinary (struct io_libecc_chars_Append *chars, double binary, int base)
{
	if (isnan(binary))
	{
		appendText(chars, ECC_ConstString_Nan);
		return;
	}
	else if (!isfinite(binary))
	{
		if (binary < 0)
			appendText(chars, ECC_ConstString_NegativeInfinity);
		else
			appendText(chars, ECC_ConstString_Infinity);
		
		return;
	}
	
	if (!base || base == 10)
	{
		if (binary <= -1e+21 || binary >= 1e+21)
			append(chars, "%g", binary);
		else if ((binary < 1 && binary >= 0.000001) || (binary > -1 && binary <= -0.000001))
		{
			append(chars, "%.10f", binary);
			if (chars->value)
				chars->value->length = stripBinaryOfBytes(chars->value->bytes, chars->value->length);
			else
				chars->units = stripBinaryOfBytes(chars->buffer, chars->units);
			
			return;
		}
		else
		{
			double dblDig10 = pow(10, DBL_DIG);
			int precision = binary >= -dblDig10 && binary <= dblDig10? DBL_DIG: 21;
			
			append(chars, "%.*g", precision, binary);
		}
		
		normalizeBinary(chars);
		return;
	}
	else
	{
		int sign = binary < 0;
		unsigned long integer = sign? -binary: binary;
		
		if (base == 8 || base == 16)
		{
			const char *format = sign? (base == 8? "-%lo": "-%lx"): (base == 8? "%lo": "%lx");
			
			append(chars, format, integer);
		}
		else
		{
			static char const digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
			char buffer[1 + sizeof(integer) * CHAR_BIT];
			char *p = buffer + sizeof(buffer) - 1;
			uint16_t count;
			
			while (integer) {
				*(--p) = digits[integer % base];
				integer /= base;
			}
			if (sign)
				*(--p) = '-';
			
			count = buffer + sizeof(buffer) - 1 - p;
			append(chars, "%.*s", count, p);
		}
	}
}

void normalizeBinary (struct io_libecc_chars_Append *chars)
{
	if (chars->value)
		chars->value->length = normalizeBinaryOfBytes(chars->value->bytes, chars->value->length);
	else
		chars->units = normalizeBinaryOfBytes(chars->buffer, chars->units);
}

eccvalue_t endAppend (struct io_libecc_chars_Append *chars)
{
	struct io_libecc_Chars *self = chars->value;
	
	if (chars->value)
	{
		self->bytes[self->length] = '\0';
		return ECCNSValue.chars(self);
	}
	else
		return ECCNSValue.buffer(chars->buffer, chars->units);
}

void destroy (struct io_libecc_Chars *self)
{
	assert(self);
	
	free(self), self = NULL;
}

uint8_t codepointLength (uint32_t cp)
{
	if (cp < 0x80)
		return 1;
	else if (cp < 0x800)
		return 2;
	else if (cp < 0x10000)
		return 3;
	else
		return 4;
	
	return 0;
}

uint8_t writeCodepoint (char *bytes, uint32_t cp)
{
	if (cp < 0x80)
	{
		bytes[0] = cp;
		return 1;
	}
	else if (cp < 0x800)
	{
		bytes[0] = 0xC0 | (cp >> 6);
		bytes[1] = 0x80 | (cp & 0x3F);
		return 2;
	}
	else if (cp < 0x10000)
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
