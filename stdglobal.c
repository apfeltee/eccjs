//
//  global.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//

#define Implementation
#include "builtins.h"

#include "ecc.h"
#include "lexer.h"

// MARK: - Private

static void setup(void);
static void teardown(void);
static struct io_libecc_Function* create(void);
const struct type_io_libecc_Global io_libecc_Global = {
    setup,
    teardown,
    create,
};

const struct io_libecc_object_Type io_libecc_global_type = {
	.text = &io_libecc_text_globalType,
};

// MARK: - Static Members

static
struct io_libecc_Value eval (struct io_libecc_Context * const context)
{
	struct io_libecc_Value value;
	struct io_libecc_Input *input;
	struct io_libecc_Context subContext = {
		.parent = context,
		.this = io_libecc_Value.object(&context->ecc->global->environment),
		.ecc = context->ecc,
		.depth = context->depth + 1,
		.environment = io_libecc_Context.environmentRoot(context->parent),
	};
	
	value = io_libecc_Context.argument(context, 0);
	if (!io_libecc_Value.isString(value) || !io_libecc_Value.isPrimitive(value))
		return value;
	
	input = io_libecc_Input.createFromBytes(io_libecc_Value.stringBytes(&value), io_libecc_Value.stringLength(&value), "(eval)");
	
	io_libecc_Context.setTextIndex(context, io_libecc_context_noIndex);
	io_libecc_Ecc.evalInputWithContext(context->ecc, input, &subContext);
	
	return context->ecc->result;
}

static
struct io_libecc_Value parseInt (struct io_libecc_Context * const context)
{
	struct io_libecc_Value value;
	struct io_libecc_Text text;
	int32_t base;
	
	value = io_libecc_Value.toString(context, io_libecc_Context.argument(context, 0));
	base = io_libecc_Value.toInteger(context, io_libecc_Context.argument(context, 1)).data.integer;
	text = io_libecc_Value.textOf(&value);
	
	if (!base)
	{
		// prevent octal auto-detection
		
		if (text.length > 2 && text.bytes[0] == '-')
		{
			if (text.bytes[1] == '0' && tolower(text.bytes[2]) != 'x')
				base = 10;
		}
		else if (text.length > 1 && text.bytes[0] == '0' && tolower(text.bytes[1]) != 'x')
			base = 10;
	}
	
	return io_libecc_Lexer.scanInteger(text, base, io_libecc_lexer_scanLazy | (context->ecc->sloppyMode? io_libecc_lexer_scanSloppy: 0));
}

static
struct io_libecc_Value parseFloat (struct io_libecc_Context * const context)
{
	struct io_libecc_Value value;
	struct io_libecc_Text text;
	
	value = io_libecc_Value.toString(context, io_libecc_Context.argument(context, 0));
	text = io_libecc_Value.textOf(&value);
	return io_libecc_Lexer.scanBinary(text, io_libecc_lexer_scanLazy | (context->ecc->sloppyMode? io_libecc_lexer_scanSloppy: 0));
}

static
struct io_libecc_Value isFinite (struct io_libecc_Context * const context)
{
	struct io_libecc_Value value;
	
	value = io_libecc_Value.toBinary(context, io_libecc_Context.argument(context, 0));
	return io_libecc_Value.truth(!isnan(value.data.binary) && !isinf(value.data.binary));
}

static
struct io_libecc_Value isNaN (struct io_libecc_Context * const context)
{
	struct io_libecc_Value value;
	
	value = io_libecc_Value.toBinary(context, io_libecc_Context.argument(context, 0));
	return io_libecc_Value.truth(isnan(value.data.binary));
}

static
struct io_libecc_Value decodeExcept (struct io_libecc_Context * const context, const char *exclude)
{
	char buffer[5], *b;
	struct io_libecc_Value value;
	const char *bytes;
	uint16_t index = 0, count;
	struct io_libecc_chars_Append chars;
	uint8_t byte;
	
	value = io_libecc_Value.toString(context, io_libecc_Context.argument(context, 0));
	bytes = io_libecc_Value.stringBytes(&value);
	count = io_libecc_Value.stringLength(&value);
	
	io_libecc_Chars.beginAppend(&chars);
	
	while (index < count)
	{
		byte = bytes[index++];
		
		if (byte != '%')
			io_libecc_Chars.append(&chars, "%c", byte);
		else if (index + 2 > count || !isxdigit(bytes[index]) || !isxdigit(bytes[index + 1]))
			goto error;
		else
		{
			byte = io_libecc_Lexer.uint8Hex(bytes[index], bytes[index + 1]);
			index += 2;
			
			if (byte >= 0x80)
			{
				struct io_libecc_text_Char c;
				int continuation = (byte & 0xf8) == 0xf0? 3: (byte & 0xf0) == 0xe0? 2: (byte & 0xe0) == 0xc0? 1: 0;
				
				if (!continuation || index + continuation * 3 > count)
					goto error;
				
				b = buffer;
				(*b++) = byte;
				while (continuation--)
				{
					if (bytes[index++] != '%' || !isxdigit(bytes[index]) || !isxdigit(bytes[index + 1]))
						goto error;
					
					byte = io_libecc_Lexer.uint8Hex(bytes[index], bytes[index + 1]);
					index += 2;
					
					if ((byte & 0xc0) != 0x80)
						goto error;
					
					(*b++) = byte;
				}
				*b = '\0';
				
				c = io_libecc_Text.character(io_libecc_Text.make(buffer, (int32_t)(b - buffer)));
				io_libecc_Chars.appendCodepoint(&chars, c.codepoint);
			}
			else if (byte && exclude && strchr(exclude, byte))
				io_libecc_Chars.append(&chars, "%%%c%c", bytes[index - 2], bytes[index - 1]);
			else
				io_libecc_Chars.append(&chars, "%c", byte);
		}
	}
	
	return io_libecc_Chars.endAppend(&chars);
	
	error:
	io_libecc_Context.uriError(context, io_libecc_Chars.create("malformed URI"));
}

static
struct io_libecc_Value decodeURI (struct io_libecc_Context * const context)
{
	return decodeExcept(context, ";/?:@&=+$,#");
}

static
struct io_libecc_Value decodeURIComponent (struct io_libecc_Context * const context)
{
	return decodeExcept(context, NULL);
}

static
struct io_libecc_Value encodeExpect (struct io_libecc_Context * const context, const char *exclude)
{
	const char hex[] = "0123456789ABCDEF";
	struct io_libecc_Value value;
	const char *bytes;
	uint16_t offset = 0, unit, length;
	struct io_libecc_Chars *chars;
	struct io_libecc_Text text;
	struct io_libecc_text_Char c;
	int needPair = 0;
	
	value = io_libecc_Value.toString(context, io_libecc_Context.argument(context, 0));
	bytes = io_libecc_Value.stringBytes(&value);
	length = io_libecc_Value.stringLength(&value);
	text = io_libecc_Text.make(bytes, length);
	
	chars = io_libecc_Chars.createSized(length * 3);
	
	while (text.length)
	{
		c = io_libecc_Text.character(text);
		
		if (c.codepoint < 0x80 && c.codepoint && strchr(exclude, c.codepoint))
			chars->bytes[offset++] = c.codepoint;
		else
		{
			if (c.codepoint >= 0xDC00 && c.codepoint <= 0xDFFF)
			{
				if (!needPair)
					goto error;
			}
			else if (needPair)
				goto error;
			else if (c.codepoint >= 0xD800 && c.codepoint <= 0xDBFF)
				needPair = 1;
			
			for (unit = 0; unit < c.units; ++unit)
			{
				chars->bytes[offset++] = '%';
				chars->bytes[offset++] = hex[(uint8_t)text.bytes[unit] >> 4];
				chars->bytes[offset++] = hex[(uint8_t)text.bytes[unit] & 0xf];
			}
		}
		
		io_libecc_Text.advance(&text, c.units);
	}
	
	if (needPair)
		goto error;
	
	chars->length = offset;
	return io_libecc_Value.chars(chars);
	
	error:
	io_libecc_Context.uriError(context, io_libecc_Chars.create("malformed URI"));
}

static
struct io_libecc_Value encodeURI (struct io_libecc_Context * const context)
{
	return encodeExpect(context, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.!~*'()" ";/?:@&=+$,#");
}

static
struct io_libecc_Value encodeURIComponent (struct io_libecc_Context * const context)
{
	return encodeExpect(context, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.!~*'()");
}

static
struct io_libecc_Value escape (struct io_libecc_Context * const context)
{
	const char *exclude = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 @*_+-./";
	struct io_libecc_Value value;
	struct io_libecc_chars_Append chars;
	struct io_libecc_Text text;
	struct io_libecc_text_Char c;
	
	value = io_libecc_Value.toString(context, io_libecc_Context.argument(context, 0));
	text = io_libecc_Value.textOf(&value);
	
	io_libecc_Chars.beginAppend(&chars);
	
	while (text.length)
	{
		c = io_libecc_Text.nextCharacter(&text);
		
		if (c.codepoint < 0x80 && c.codepoint && strchr(exclude, c.codepoint))
			io_libecc_Chars.append(&chars, "%c", c.codepoint);
		else
		{
			if (c.codepoint <= 0xff)
				io_libecc_Chars.append(&chars, "%%%02X", c.codepoint);
			else if (c.codepoint < 0x010000)
				io_libecc_Chars.append(&chars, "%%u%04X", c.codepoint);
			else
			{
				c.codepoint -= 0x010000;
				io_libecc_Chars.append(&chars, "%%u%04X", ((c.codepoint >> 10) & 0x3ff) + 0xd800);
				io_libecc_Chars.append(&chars, "%%u%04X", ((c.codepoint >>  0) & 0x3ff) + 0xdc00);
			}
		}
	}
	
	return io_libecc_Chars.endAppend(&chars);
}

static
struct io_libecc_Value unescape (struct io_libecc_Context * const context)
{
	struct io_libecc_Value value;
	struct io_libecc_chars_Append chars;
	struct io_libecc_Text text;
	struct io_libecc_text_Char c;
	
	value = io_libecc_Value.toString(context, io_libecc_Context.argument(context, 0));
	text = io_libecc_Value.textOf(&value);
	
	io_libecc_Chars.beginAppend(&chars);
	
	while (text.length)
	{
		c = io_libecc_Text.nextCharacter(&text);
		
		if (c.codepoint == '%')
		{
			switch (io_libecc_Text.character(text).codepoint)
			{
				case '%':
					io_libecc_Chars.append(&chars, "%%");
					break;
					
				case 'u':
				{
					uint32_t cp = io_libecc_Lexer.uint16Hex(text.bytes[1], text.bytes[2], text.bytes[3], text.bytes[4]);
					
					io_libecc_Chars.appendCodepoint(&chars, cp);
					io_libecc_Text.advance(&text, 5);
					break;
				}
					
				default:
				{
					uint32_t cp = io_libecc_Lexer.uint8Hex(text.bytes[0], text.bytes[1]);
					
					io_libecc_Chars.appendCodepoint(&chars, cp);
					io_libecc_Text.advance(&text, 2);
					break;
				}
			}
		}
		else
			io_libecc_Chars.append(&chars, "%c", c.codepoint);
	}
	
	return io_libecc_Chars.endAppend(&chars);
}

// MARK: - Methods

void setup (void)
{
	io_libecc_function_prototype = io_libecc_object_prototype = io_libecc_Object.create(NULL);
	
	io_libecc_Function.setup();
	io_libecc_Object.setup();
	io_libecc_String.setup();
	io_libecc_Error.setup();
	io_libecc_Array.setup();
	io_libecc_Date.setup();
	io_libecc_Math.setup();
	io_libecc_Number.setup();
	io_libecc_Boolean.setup();
	io_libecc_RegExp.setup();
	io_libecc_JSON.setup();
	io_libecc_Arguments.setup();
}

void teardown (void)
{
	io_libecc_Function.teardown();
	io_libecc_Object.teardown();
	io_libecc_String.teardown();
	io_libecc_Error.teardown();
	io_libecc_Array.teardown();
	io_libecc_Date.teardown();
	io_libecc_Math.teardown();
	io_libecc_Number.teardown();
	io_libecc_Boolean.teardown();
	io_libecc_RegExp.teardown();
	io_libecc_JSON.teardown();
	io_libecc_Arguments.teardown();
}

struct io_libecc_Function * create (void)
{
	const enum io_libecc_value_Flags r = io_libecc_value_readonly;
	const enum io_libecc_value_Flags h = io_libecc_value_hidden;
	const enum io_libecc_value_Flags s = io_libecc_value_sealed;
	
	struct io_libecc_Function * self = io_libecc_Function.create(io_libecc_object_prototype);
	self->environment.type = &io_libecc_global_type;
	
	io_libecc_Function.addValue(self, "NaN", io_libecc_Value.binary(NAN), r|h|s);
	io_libecc_Function.addValue(self, "Infinity", io_libecc_Value.binary(INFINITY), r|h|s);
	io_libecc_Function.addValue(self, "undefined", io_libecc_value_undefined, r|h|s);
	
	io_libecc_Function.addFunction(self, "eval", eval, 1, h);
	io_libecc_Function.addFunction(self, "escape", escape, 1, h);
	io_libecc_Function.addFunction(self, "unescape", unescape, 1, h);
	io_libecc_Function.addFunction(self, "parseInt", parseInt, 2, h);
	io_libecc_Function.addFunction(self, "parseFloat", parseFloat, 1, h);
	io_libecc_Function.addFunction(self, "isNaN", isNaN, 1, h);
	io_libecc_Function.addFunction(self, "isFinite", isFinite, 1, h);
	io_libecc_Function.addFunction(self, "decodeURI", decodeURI, 1, h);
	io_libecc_Function.addFunction(self, "decodeURIComponent", decodeURIComponent, 1, h);
	io_libecc_Function.addFunction(self, "encodeURI", encodeURI, 1, h);
	io_libecc_Function.addFunction(self, "encodeURIComponent", encodeURIComponent, 1, h);
	io_libecc_Function.addValue(self, "Object", io_libecc_Value.function(io_libecc_object_constructor), h);
	io_libecc_Function.addValue(self, "Function", io_libecc_Value.function(io_libecc_function_constructor), h);
	io_libecc_Function.addValue(self, "Array", io_libecc_Value.function(io_libecc_array_constructor), h);
	io_libecc_Function.addValue(self, "String", io_libecc_Value.function(io_libecc_string_constructor), h);
	io_libecc_Function.addValue(self, "Boolean", io_libecc_Value.function(io_libecc_boolean_constructor), h);
	io_libecc_Function.addValue(self, "Number", io_libecc_Value.function(io_libecc_number_constructor), h);
	io_libecc_Function.addValue(self, "Date", io_libecc_Value.function(io_libecc_date_constructor), h);
	io_libecc_Function.addValue(self, "RegExp", io_libecc_Value.function(io_libecc_regexp_constructor), h);
	io_libecc_Function.addValue(self, "Error", io_libecc_Value.function(io_libecc_error_constructor), h);
	io_libecc_Function.addValue(self, "RangeError", io_libecc_Value.function(io_libecc_error_rangeConstructor), h);
	io_libecc_Function.addValue(self, "ReferenceError", io_libecc_Value.function(io_libecc_error_referenceConstructor), h);
	io_libecc_Function.addValue(self, "SyntaxError", io_libecc_Value.function(io_libecc_error_syntaxConstructor), h);
	io_libecc_Function.addValue(self, "TypeError", io_libecc_Value.function(io_libecc_error_typeConstructor), h);
	io_libecc_Function.addValue(self, "URIError", io_libecc_Value.function(io_libecc_error_uriConstructor), h);
	io_libecc_Function.addValue(self, "EvalError", io_libecc_Value.function(io_libecc_error_evalConstructor), h);
	io_libecc_Function.addValue(self, "Math", io_libecc_Value.object(io_libecc_math_object), h);
	io_libecc_Function.addValue(self, "JSON", io_libecc_Value.object(io_libecc_json_object), h);
	
	return self;
}
