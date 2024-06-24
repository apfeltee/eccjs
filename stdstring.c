//
//  string.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"


// MARK: - Private

static void mark (struct eccobject_t *object);
static void capture (struct eccobject_t *object);
static void finalize (struct eccobject_t *object);

struct eccobject_t * io_libecc_string_prototype = NULL;
struct io_libecc_Function * io_libecc_string_constructor = NULL;

const struct io_libecc_object_Type io_libecc_string_type = {
	.text = &io_libecc_text_stringType,
	.mark = mark,
	.capture = capture,
	.finalize = finalize,
};

static void setup(void);
static void teardown(void);
static struct io_libecc_String* create(struct io_libecc_Chars*);
static struct eccvalue_t valueAtIndex(struct io_libecc_String*, int32_t index);
static struct io_libecc_Text textAtIndex(const char* chars, int32_t length, int32_t index, int enableReverse);
static int32_t unitIndex(const char* chars, int32_t max, int32_t unit);
const struct type_io_libecc_String io_libecc_String = {
    setup, teardown, create, valueAtIndex, textAtIndex, unitIndex,
};

static
void mark (struct eccobject_t *object)
{
	struct io_libecc_String *self = (struct io_libecc_String *)object;
	
	io_libecc_Pool.markValue(io_libecc_Value.chars(self->value));
}

static
void capture (struct eccobject_t *object)
{
	struct io_libecc_String *self = (struct io_libecc_String *)object;
	
	++self->value->referenceCount;
}

static
void finalize (struct eccobject_t *object)
{
	struct io_libecc_String *self = (struct io_libecc_String *)object;
	
	--self->value->referenceCount;
}

// MARK: - Static Members

static
struct eccvalue_t toString (struct eccstate_t * const context)
{
	io_libecc_Context.assertThisType(context, io_libecc_value_stringType);
	
	return io_libecc_Value.chars(context->this.data.string->value);
}

static
struct eccvalue_t valueOf (struct eccstate_t * const context)
{
	io_libecc_Context.assertThisType(context, io_libecc_value_stringType);
	
	return io_libecc_Value.chars(context->this.data.string->value);
}

static
struct eccvalue_t charAt (struct eccstate_t * const context)
{
	int32_t index, length;
	const char *chars;
	struct io_libecc_Text text;
	
	io_libecc_Context.assertThisCoerciblePrimitive(context);
	
	context->this = io_libecc_Value.toString(context, context->this);
	chars = io_libecc_Value.stringBytes(&context->this);
	length = io_libecc_Value.stringLength(&context->this);
	index = io_libecc_Value.toInteger(context, io_libecc_Context.argument(context, 0)).data.integer;
	
	text = textAtIndex(chars, length, index, 0);
	if (!text.length)
		return io_libecc_Value.text(&io_libecc_text_empty);
	else
	{
		struct io_libecc_text_Char c = io_libecc_Text.character(text);
		
		if (c.codepoint < 0x010000)
			return io_libecc_Value.buffer(text.bytes, c.units);
		else
		{
			char buffer[7];
			
			/* simulate 16-bit surrogate */
			
			c.codepoint -= 0x010000;
			if (text.flags & io_libecc_text_breakFlag)
				c.codepoint = ((c.codepoint >>  0) & 0x3ff) + 0xdc00;
			else
				c.codepoint = ((c.codepoint >> 10) & 0x3ff) + 0xd800;
			
			io_libecc_Chars.writeCodepoint(buffer, c.codepoint);
			return io_libecc_Value.buffer(buffer, 3);
		}
	}
}

static
struct eccvalue_t charCodeAt (struct eccstate_t * const context)
{
	int32_t index, length;
	const char *chars;
	struct io_libecc_Text text;
	
	io_libecc_Context.assertThisCoerciblePrimitive(context);
	
	context->this = io_libecc_Value.toString(context, context->this);
	chars = io_libecc_Value.stringBytes(&context->this);
	length = io_libecc_Value.stringLength(&context->this);
	index = io_libecc_Value.toInteger(context, io_libecc_Context.argument(context, 0)).data.integer;
	
	text = textAtIndex(chars, length, index, 0);
	if (!text.length)
		return io_libecc_Value.binary(NAN);
	else
	{
		struct io_libecc_text_Char c = io_libecc_Text.character(text);
		
		if (c.codepoint < 0x010000)
			return io_libecc_Value.binary(c.codepoint);
		else
		{
			/* simulate 16-bit surrogate */
			
			c.codepoint -= 0x010000;
			if (text.flags & io_libecc_text_breakFlag)
				return io_libecc_Value.binary(((c.codepoint >>  0) & 0x3ff) + 0xdc00);
			else
				return io_libecc_Value.binary(((c.codepoint >> 10) & 0x3ff) + 0xd800);
		}
	}
}

static
struct eccvalue_t concat (struct eccstate_t * const context)
{
	struct io_libecc_chars_Append chars;
	int32_t index, count;
	
	io_libecc_Context.assertThisCoerciblePrimitive(context);
	
	count = io_libecc_Context.argumentCount(context);
	
	io_libecc_Chars.beginAppend(&chars);
	io_libecc_Chars.appendValue(&chars, context, io_libecc_Context.this(context));
	for (index = 0; index < count; ++index)
		io_libecc_Chars.appendValue(&chars, context, io_libecc_Context.argument(context, index));
	
	return io_libecc_Chars.endAppend(&chars);
}

static
struct eccvalue_t indexOf (struct eccstate_t * const context)
{
	struct io_libecc_Text text;
	struct eccvalue_t search, start;
	int32_t index, length, searchLength;
	const char *chars, *searchChars;
	
	io_libecc_Context.assertThisCoerciblePrimitive(context);
	
	context->this = io_libecc_Value.toString(context, io_libecc_Context.this(context));
	chars = io_libecc_Value.stringBytes(&context->this);
	length = io_libecc_Value.stringLength(&context->this);
	
	search = io_libecc_Value.toString(context, io_libecc_Context.argument(context, 0));
	searchChars = io_libecc_Value.stringBytes(&search);
	searchLength = io_libecc_Value.stringLength(&search);
	start = io_libecc_Value.toInteger(context, io_libecc_Context.argument(context, 1));
	index = start.data.integer < 0? length + start.data.integer: start.data.integer;
	if (index < 0)
		index = 0;
	
	text = textAtIndex(chars, length, index, 0);
	if (text.flags & io_libecc_text_breakFlag)
	{
		io_libecc_Text.nextCharacter(&text);
		++index;
	}
	
	while (text.length)
	{
		if (!memcmp(text.bytes, searchChars, searchLength))
			return io_libecc_Value.integer(index);
		
		++index;
		if (io_libecc_Text.nextCharacter(&text).codepoint > 0xffff)
			++index;
	}
	
	return io_libecc_Value.integer(-1);
}

static
struct eccvalue_t lastIndexOf (struct eccstate_t * const context)
{
	struct io_libecc_Text text;
	struct eccvalue_t search, start;
	int32_t index, length, searchLength;
	const char *chars, *searchChars;
	
	io_libecc_Context.assertThisCoerciblePrimitive(context);
	
	context->this = io_libecc_Value.toString(context, io_libecc_Context.this(context));
	chars = io_libecc_Value.stringBytes(&context->this);
	length = io_libecc_Value.stringLength(&context->this);
	
	search = io_libecc_Value.toString(context, io_libecc_Context.argument(context, 0));
	searchChars = io_libecc_Value.stringBytes(&search);
	searchLength = io_libecc_Value.stringLength(&search);
	
	start = io_libecc_Value.toBinary(context, io_libecc_Context.argument(context, 1));
	index = unitIndex(chars, length, length);
	if (!isnan(start.data.binary) && start.data.binary < index)
		index = start.data.binary < 0? 0: start.data.binary;
	
	text = textAtIndex(chars, length, index, 0);
	if (text.flags & io_libecc_text_breakFlag)
		--index;
	
	text.length = (int32_t)(text.bytes - chars);
	
	for (;;)
	{
		if (length - (text.bytes - chars) >= searchLength && !memcmp(text.bytes, searchChars, searchLength))
			return io_libecc_Value.integer(index);
		
		if (!text.length)
			break;
		
		--index;
		if (io_libecc_Text.prevCharacter(&text).codepoint > 0xffff)
			--index;
	}
	
	return io_libecc_Value.integer(-1);
}

static
struct eccvalue_t localeCompare (struct eccstate_t * const context)
{
	struct eccvalue_t that;
	struct io_libecc_Text a, b;
	
	io_libecc_Context.assertThisCoerciblePrimitive(context);
	
	context->this = io_libecc_Value.toString(context, io_libecc_Context.this(context));
	a = io_libecc_Value.textOf(&context->this);
	
	that = io_libecc_Value.toString(context, io_libecc_Context.argument(context, 0));
	b = io_libecc_Value.textOf(&that);
	
	if (a.length < b.length && !memcmp(a.bytes, b.bytes, a.length))
		return io_libecc_Value.integer(-1);
	
	if (a.length > b.length && !memcmp(a.bytes, b.bytes, b.length))
		return io_libecc_Value.integer(1);
	
	return io_libecc_Value.integer(memcmp(a.bytes, b.bytes, a.length));
}

static
struct eccvalue_t match (struct eccstate_t * const context)
{
	struct io_libecc_RegExp *regexp;
	struct eccvalue_t value, lastIndex;
	
	context->this = io_libecc_Value.toString(context, io_libecc_Context.this(context));
	
	value = io_libecc_Context.argument(context, 0);
	if (value.type == io_libecc_value_regexpType)
		regexp = value.data.regexp;
	else
		regexp = io_libecc_RegExp.createWith(context, value, io_libecc_value_undefined);
	
	lastIndex = regexp->global? io_libecc_Value.integer(0): io_libecc_Value.toInteger(context, io_libecc_Object.getMember(context, &regexp->object, io_libecc_key_lastIndex));
	
	io_libecc_Object.putMember(context, &regexp->object, io_libecc_key_lastIndex, io_libecc_Value.integer(0));
	
	if (lastIndex.data.integer >= 0)
	{
		const char *bytes = io_libecc_Value.stringBytes(&context->this);
		int32_t length = io_libecc_Value.stringLength(&context->this);
		struct io_libecc_Text text = textAtIndex(bytes, length, 0, 0);
		const char *capture[regexp->count * 2];
		const char *index[regexp->count * 2];
		struct eccobject_t *array = io_libecc_Array.create();
		struct io_libecc_chars_Append chars;
		uint32_t size = 0;
		
		do
		{
			struct io_libecc_regexp_State state = { text.bytes, text.bytes + text.length, capture, index };
			
			if (io_libecc_RegExp.matchWithState(regexp, &state))
			{
				io_libecc_Chars.beginAppend(&chars);
				io_libecc_Chars.append(&chars, "%.*s", capture[1] - capture[0], capture[0]);
				io_libecc_Object.addElement(array, size++, io_libecc_Chars.endAppend(&chars), 0);
				
				if (!regexp->global)
				{
					int32_t index, count;
					
					for (index = 1, count = regexp->count; index < count; ++index)
					{
						if (capture[index * 2])
						{
							io_libecc_Chars.beginAppend(&chars);
							io_libecc_Chars.append(&chars, "%.*s", capture[index * 2 + 1] - capture[index * 2], capture[index * 2]);
							io_libecc_Object.addElement(array, size++, io_libecc_Chars.endAppend(&chars), 0);
						}
						else
							io_libecc_Object.addElement(array, size++, io_libecc_value_undefined, 0);
					}
					break;
				}
				
				if (capture[1] - text.bytes > 0)
					io_libecc_Text.advance(&text, (int32_t)(capture[1] - text.bytes));
				else
					io_libecc_Text.nextCharacter(&text);
			}
			else
				break;
		}
		while (text.length);
		
		if (size)
		{
			io_libecc_Object.addMember(array, io_libecc_key_input, context->this, 0);
			io_libecc_Object.addMember(array, io_libecc_key_index, io_libecc_Value.integer(io_libecc_String.unitIndex(bytes, length, (int32_t)(capture[0] - bytes))), 0);
			
			if (regexp->global)
				io_libecc_Object.putMember(context, &regexp->object, io_libecc_key_lastIndex, io_libecc_Value.integer(io_libecc_String.unitIndex(bytes, length, (int32_t)(text.bytes - bytes))));
			
			return io_libecc_Value.object(array);
		}
	}
	return io_libecc_value_null;
}

static
void replaceText (struct io_libecc_chars_Append *chars, struct io_libecc_Text replace, struct io_libecc_Text before, struct io_libecc_Text match, struct io_libecc_Text after, int count, const char *capture[])
{
	struct io_libecc_text_Char c;
	
	while (replace.length)
	{
		c = io_libecc_Text.character(replace);
		
		if (c.codepoint == '$')
		{
			int index;
			
			io_libecc_Text.advance(&replace, 1);
			
			switch (io_libecc_Text.character(replace).codepoint)
			{
				case '$':
					io_libecc_Chars.append(chars, "$");
					break;
					
				case '&':
					io_libecc_Chars.append(chars, "%.*s", match.length, match.bytes);
					break;
					
				case '`':
					io_libecc_Chars.append(chars, "%.*s", before.length, before.bytes);
					break;
					
				case '\'':
					io_libecc_Chars.append(chars, "%.*s", after.length, after.bytes);
					break;
					
				case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
					index = replace.bytes[0] - '0';
					if (index < count)
					{
						if (isdigit(replace.bytes[1]) && index * 10 <= count)
						{
							index = index * 10 + replace.bytes[1] - '0';
							io_libecc_Text.advance(&replace, 1);
						}
						
						if (capture && index && index < count)
						{
							if (capture[index * 2])
								io_libecc_Chars.append(chars, "%.*s", capture[index * 2 + 1] - capture[index * 2], capture[index * 2]);
							else
								io_libecc_Chars.append(chars, "");
							
							break;
						}
					}
					/* vvv */
				default:
					io_libecc_Chars.append(chars, "$");
					continue;
			}
		}
		else
			io_libecc_Chars.append(chars, "%.*s", c.units, replace.bytes);
		
		io_libecc_Text.advance(&replace, c.units);
	}
}

static
struct eccvalue_t replace (struct eccstate_t * const context)
{
	struct io_libecc_RegExp *regexp = NULL;
	struct io_libecc_chars_Append chars;
	struct eccvalue_t value, replace;
	struct io_libecc_Text text;
	const char *bytes, *searchBytes;
	int32_t length, searchLength;
	
	io_libecc_Context.assertThisCoerciblePrimitive(context);
	
	context->this = io_libecc_Value.toString(context, io_libecc_Context.this(context));
	bytes = io_libecc_Value.stringBytes(&context->this);
	length = io_libecc_Value.stringLength(&context->this);
	text = io_libecc_Text.make(bytes, length);
	
	value = io_libecc_Context.argument(context, 0);
	if (value.type == io_libecc_value_regexpType)
		regexp = value.data.regexp;
	else
		value = io_libecc_Value.toString(context, value);
	
	replace = io_libecc_Context.argument(context, 1);
	if (replace.type != io_libecc_value_functionType)
		replace = io_libecc_Value.toString(context, replace);
	
	if (regexp)
	{
		const char *capture[regexp->count * 2];
		const char *index[regexp->count * 2];
		struct io_libecc_Text seek = text;
		
		io_libecc_Chars.beginAppend(&chars);
		do
		{
			struct io_libecc_regexp_State state = { seek.bytes, text.bytes + text.length, capture, index };
			
			if (io_libecc_RegExp.matchWithState(regexp, &state))
			{
				io_libecc_Chars.append(&chars, "%.*s", capture[0] - text.bytes, text.bytes);
				
				if (replace.type == io_libecc_value_functionType)
				{
					struct eccobject_t *arguments = io_libecc_Array.createSized(regexp->count + 2);
					int32_t index, count;
					struct eccvalue_t result;
					
					for (index = 0, count = regexp->count; index < count; ++index)
					{
						if (capture[index * 2])
							arguments->element[index].value = io_libecc_Value.chars(io_libecc_Chars.createWithBytes((int32_t)(capture[index * 2 + 1] - capture[index * 2]), capture[index * 2]));
						else
							arguments->element[index].value = io_libecc_value_undefined;
					}
					arguments->element[regexp->count].value = io_libecc_Value.integer(io_libecc_String.unitIndex(bytes, length, (int32_t)(capture[0] - bytes)));
					arguments->element[regexp->count + 1].value = context->this;
					
					result = io_libecc_Value.toString(context, io_libecc_Op.callFunctionArguments(context, 0, replace.data.function, io_libecc_value_undefined, arguments));
					io_libecc_Chars.append(&chars, "%.*s", io_libecc_Value.stringLength(&result), io_libecc_Value.stringBytes(&result));
				}
				else
					replaceText(&chars,
								io_libecc_Text.make(io_libecc_Value.stringBytes(&replace), io_libecc_Value.stringLength(&replace)),
								io_libecc_Text.make(bytes, (int32_t)(capture[0] - bytes)),
								io_libecc_Text.make(capture[0], (int32_t)(capture[1] - capture[0])),
								io_libecc_Text.make(capture[1], (int32_t)((bytes + length) - capture[1])),
								regexp->count,
								capture);
				
				io_libecc_Text.advance(&text, (int32_t)(state.capture[1] - text.bytes));
				
				seek = text;
				if (text.bytes == state.capture[1])
					io_libecc_Text.nextCharacter(&seek);
			}
			else
				break;
		}
		while (text.length && regexp->global);
		
		io_libecc_Chars.append(&chars, "%.*s", text.length, text.bytes);
		
		return io_libecc_Chars.endAppend(&chars);
	}
	else
	{
		searchBytes = io_libecc_Value.stringBytes(&value);
		searchLength = io_libecc_Value.stringLength(&value);
		
		for (;;)
		{
			if (!text.length)
				return context->this;
			
			if (!memcmp(text.bytes, searchBytes, searchLength))
			{
				text.length = searchLength;
				break;
			}
			io_libecc_Text.nextCharacter(&text);
		}
		
		io_libecc_Chars.beginAppend(&chars);
		io_libecc_Chars.append(&chars, "%.*s", text.bytes - bytes, bytes);
		
		if (replace.type == io_libecc_value_functionType)
		{
			struct eccobject_t *arguments = io_libecc_Array.createSized(1 + 2);
			struct eccvalue_t result;
			
			arguments->element[0].value = io_libecc_Value.chars(io_libecc_Chars.createWithBytes(text.length, text.bytes));
			arguments->element[1].value = io_libecc_Value.integer(io_libecc_String.unitIndex(bytes, length, (int32_t)(text.bytes - bytes)));
			arguments->element[2].value = context->this;
			
			result = io_libecc_Value.toString(context, io_libecc_Op.callFunctionArguments(context, 0, replace.data.function, io_libecc_value_undefined, arguments));
			io_libecc_Chars.append(&chars, "%.*s", io_libecc_Value.stringLength(&result), io_libecc_Value.stringBytes(&result));
		}
		else
			replaceText(&chars,
						io_libecc_Text.make(io_libecc_Value.stringBytes(&replace), io_libecc_Value.stringLength(&replace)),
						io_libecc_Text.make(text.bytes, (int32_t)(text.bytes - bytes)),
						io_libecc_Text.make(text.bytes, text.length),
						io_libecc_Text.make(text.bytes, (int32_t)(length - (text.bytes - bytes))),
						0,
						NULL);
		
		io_libecc_Chars.append(&chars, "%.*s", length - (text.bytes - bytes), text.bytes + text.length);
		
		return io_libecc_Chars.endAppend(&chars);
	}
}

static
struct eccvalue_t search (struct eccstate_t * const context)
{
	struct io_libecc_RegExp *regexp;
	struct eccvalue_t value;
	
	io_libecc_Context.assertThisCoerciblePrimitive(context);
	
	context->this = io_libecc_Value.toString(context, io_libecc_Context.this(context));
	
	value = io_libecc_Context.argument(context, 0);
	if (value.type == io_libecc_value_regexpType)
		regexp = value.data.regexp;
	else
		regexp = io_libecc_RegExp.createWith(context, value, io_libecc_value_undefined);
	
	{
		const char *bytes = io_libecc_Value.stringBytes(&context->this);
		int32_t length = io_libecc_Value.stringLength(&context->this);
		struct io_libecc_Text text = textAtIndex(bytes, length, 0, 0);
		const char *capture[regexp->count * 2];
		const char *index[regexp->count * 2];
		
		struct io_libecc_regexp_State state = { text.bytes, text.bytes + text.length, capture, index };
		
		if (io_libecc_RegExp.matchWithState(regexp, &state))
			return io_libecc_Value.integer(unitIndex(bytes, length, (int32_t)(capture[0] - bytes)));
	}
	return io_libecc_Value.integer(-1);
}

static
struct eccvalue_t slice (struct eccstate_t * const context)
{
	struct eccvalue_t from, to;
	struct io_libecc_Text start, end;
	const char *chars;
	int32_t length;
	uint16_t head = 0, tail = 0;
	uint32_t headcp = 0;
	
	if (!io_libecc_Value.isString(context->this))
		context->this = io_libecc_Value.toString(context, io_libecc_Context.this(context));
	
	chars = io_libecc_Value.stringBytes(&context->this);
	length = io_libecc_Value.stringLength(&context->this);
	
	from = io_libecc_Context.argument(context, 0);
	if (from.type == io_libecc_value_undefinedType)
		start = io_libecc_Text.make(chars, length);
	else if (from.type == io_libecc_value_binaryType && from.data.binary == INFINITY)
		start = io_libecc_Text.make(chars + length, 0);
	else
		start = textAtIndex(chars, length, io_libecc_Value.toInteger(context, from).data.integer, 1);
	
	to = io_libecc_Context.argument(context, 1);
	if (to.type == io_libecc_value_undefinedType || (to.type == io_libecc_value_binaryType && (isnan(to.data.binary) || to.data.binary == INFINITY)))
		end = io_libecc_Text.make(chars + length, 0);
	else if (to.type == io_libecc_value_binaryType && to.data.binary == -INFINITY)
		end = io_libecc_Text.make(chars, length);
	else
		end = textAtIndex(chars, length, io_libecc_Value.toInteger(context, to).data.integer, 1);
	
	if (start.flags & io_libecc_text_breakFlag)
		headcp = io_libecc_Text.nextCharacter(&start).codepoint;
	
	length = (int32_t)(end.bytes - start.bytes);
	
	if (start.flags & io_libecc_text_breakFlag)
		head = 3;
	
	if (end.flags & io_libecc_text_breakFlag)
		tail = 3;
	
	if (head + length + tail <= 0)
		return io_libecc_Value.text(&io_libecc_text_empty);
	else
	{
		struct io_libecc_Chars *result = io_libecc_Chars.createSized(length + head + tail);
		
		if (start.flags & io_libecc_text_breakFlag)
		{
			/* simulate 16-bit surrogate */
			io_libecc_Chars.writeCodepoint(result->bytes, (((headcp - 0x010000) >> 0) & 0x3ff) + 0xdc00);
		}
		
		if (length > 0)
			memcpy(result->bytes + head, start.bytes, length);
		
		if (end.flags & io_libecc_text_breakFlag)
		{
			/* simulate 16-bit surrogate */
			io_libecc_Chars.writeCodepoint(result->bytes + head + length, (((io_libecc_Text.character(end).codepoint - 0x010000) >> 10) & 0x3ff) + 0xd800);
		}
		
		return io_libecc_Value.chars(result);
	}
}

static
struct eccvalue_t split (struct eccstate_t * const context)
{
	struct eccvalue_t separatorValue, limitValue;
	struct io_libecc_RegExp *regexp = NULL;
	struct eccobject_t *array;
	struct io_libecc_Chars *element;
	struct io_libecc_Text text, separator = { 0 };
	uint32_t size = 0, limit = UINT32_MAX;
	
	io_libecc_Context.assertThisCoerciblePrimitive(context);
	
	context->this = io_libecc_Value.toString(context, io_libecc_Context.this(context));
	text = io_libecc_Value.textOf(&context->this);
	
	limitValue = io_libecc_Context.argument(context, 1);
	if (limitValue.type != io_libecc_value_undefinedType)
	{
		limit = io_libecc_Value.toInteger(context, limitValue).data.integer;
		if (!limit)
			return io_libecc_Value.object(io_libecc_Array.createSized(0));
	}
	
	separatorValue = io_libecc_Context.argument(context, 0);
	if (separatorValue.type == io_libecc_value_undefinedType)
	{
		struct eccobject_t *array = io_libecc_Array.createSized(1);
		array->element[0].value = context->this;
		return io_libecc_Value.object(array);
	}
	else if (separatorValue.type == io_libecc_value_regexpType)
		regexp = separatorValue.data.regexp;
	else
	{
		separatorValue = io_libecc_Value.toString(context, separatorValue);
		separator = io_libecc_Value.textOf(&separatorValue);
	}
	
	io_libecc_Context.setTextIndex(context, io_libecc_context_callIndex);
	
	array = io_libecc_Array.create();
	
	if (regexp)
	{
		const char *capture[regexp->count * 2];
		const char *index[regexp->count * 2];
		struct io_libecc_Text seek = text;
		
		for (;;)
		{
			struct io_libecc_regexp_State state = { seek.bytes, seek.bytes + seek.length, capture, index };
			int32_t index, count;
			
			if (size >= limit)
				break;
			
			if (seek.length && io_libecc_RegExp.matchWithState(regexp, &state))
			{
				if (capture[1] <= text.bytes)
				{
					io_libecc_Text.advance(&seek, 1);
					continue;
				}
				
				element = io_libecc_Chars.createWithBytes((int32_t)(capture[0] - text.bytes), text.bytes);
				io_libecc_Object.addElement(array, size++, io_libecc_Value.chars(element), 0);
				
				for (index = 1, count = regexp->count; index < count; ++index)
				{
					if (size >= limit)
						break;
					
					if (capture[index * 2])
					{
						element = io_libecc_Chars.createWithBytes((int32_t)(capture[index * 2 + 1] - capture[index * 2]), capture[index * 2]);
						io_libecc_Object.addElement(array, size++, io_libecc_Value.chars(element), 0);
					}
					else
						io_libecc_Object.addElement(array, size++, io_libecc_value_undefined, 0);
				}
				
				io_libecc_Text.advance(&text, (int32_t)(capture[1] - text.bytes));
				seek = text;
			}
			else
			{
				element = io_libecc_Chars.createWithBytes(text.length, text.bytes);
				io_libecc_Object.addElement(array, size++, io_libecc_Value.chars(element), 0);
				break;
			}
		}
		return io_libecc_Value.object(array);
	}
	else if (!separator.length)
	{
		struct io_libecc_text_Char c;
		
		while (text.length)
		{
			if (size >= limit)
				break;
			
			c = io_libecc_Text.character(text);
			if (c.codepoint < 0x010000)
				io_libecc_Object.addElement(array, size++, io_libecc_Value.buffer(text.bytes, c.units), 0);
			else
			{
				char buffer[7];
				
				/* simulate 16-bit surrogate */
				
				io_libecc_Chars.writeCodepoint(buffer, (((c.codepoint - 0x010000) >> 10) & 0x3ff) + 0xd800);
				io_libecc_Object.addElement(array, size++, io_libecc_Value.buffer(buffer, 3), 0);
				
				io_libecc_Chars.writeCodepoint(buffer, (((c.codepoint - 0x010000) >> 0) & 0x3ff) + 0xdc00);
				io_libecc_Object.addElement(array, size++, io_libecc_Value.buffer(buffer, 3), 0);
			}
			io_libecc_Text.advance(&text, c.units);
		}
		
		return io_libecc_Value.object(array);
	}
	else
	{
		struct io_libecc_Text seek = text;
		int32_t length;
		
		while (seek.length >= separator.length)
		{
			if (size >= limit)
				break;
			
			if (!memcmp(seek.bytes, separator.bytes, separator.length))
			{
				length = (int32_t)(seek.bytes - text.bytes);
				element = io_libecc_Chars.createSized(length);
				memcpy(element->bytes, text.bytes, length);
				io_libecc_Object.addElement(array, size++, io_libecc_Value.chars(element), 0);
				
				io_libecc_Text.advance(&text, length + separator.length);
				seek = text;
				continue;
			}
			io_libecc_Text.nextCharacter(&seek);
		}
		
		if (size < limit)
		{
			element = io_libecc_Chars.createSized(text.length);
			memcpy(element->bytes, text.bytes, text.length);
			io_libecc_Object.addElement(array, size++, io_libecc_Value.chars(element), 0);
		}
	}
	
	return io_libecc_Value.object(array);
}

static
struct eccvalue_t substring (struct eccstate_t * const context)
{
	struct eccvalue_t from, to;
	struct io_libecc_Text start, end;
	const char *chars;
	int32_t length, head = 0, tail = 0;
	uint32_t headcp = 0;
	
	context->this = io_libecc_Value.toString(context, io_libecc_Context.this(context));
	chars = io_libecc_Value.stringBytes(&context->this);
	length = io_libecc_Value.stringLength(&context->this);
	
	from = io_libecc_Context.argument(context, 0);
	if (from.type == io_libecc_value_undefinedType || (from.type == io_libecc_value_binaryType && (isnan(from.data.binary) || from.data.binary == -INFINITY)))
		start = io_libecc_Text.make(chars, length);
	else if (from.type == io_libecc_value_binaryType && from.data.binary == INFINITY)
		start = io_libecc_Text.make(chars + length, 0);
	else
		start = textAtIndex(chars, length, io_libecc_Value.toInteger(context, from).data.integer, 0);
	
	to = io_libecc_Context.argument(context, 1);
	if (to.type == io_libecc_value_undefinedType || (to.type == io_libecc_value_binaryType && to.data.binary == INFINITY))
		end = io_libecc_Text.make(chars + length, 0);
	else if (to.type == io_libecc_value_binaryType && !isfinite(to.data.binary))
		end = io_libecc_Text.make(chars, length);
	else
		end = textAtIndex(chars, length, io_libecc_Value.toInteger(context, to).data.integer, 0);
	
	if (start.bytes > end.bytes)
	{
		struct io_libecc_Text temp = start;
		start = end;
		end = temp;
	}
	
	if (start.flags & io_libecc_text_breakFlag)
		headcp = io_libecc_Text.nextCharacter(&start).codepoint;
	
	length = (int32_t)(end.bytes - start.bytes);
	
	if (start.flags & io_libecc_text_breakFlag)
		head = 3;
	
	if (end.flags & io_libecc_text_breakFlag)
		tail = 3;
	
	if (head + length + tail <= 0)
		return io_libecc_Value.text(&io_libecc_text_empty);
	else
	{
		struct io_libecc_Chars *result = io_libecc_Chars.createSized(length + head + tail);
		
		if (start.flags & io_libecc_text_breakFlag)
		{
			/* simulate 16-bit surrogate */
			io_libecc_Chars.writeCodepoint(result->bytes, (((headcp - 0x010000) >> 0) & 0x3ff) + 0xdc00);
		}
		
		if (length > 0)
			memcpy(result->bytes + head, start.bytes, length);
		
		if (end.flags & io_libecc_text_breakFlag)
		{
			/* simulate 16-bit surrogate */
			io_libecc_Chars.writeCodepoint(result->bytes + head + length, (((io_libecc_Text.character(end).codepoint - 0x010000) >> 10) & 0x3ff) + 0xd800);
		}
		
		return io_libecc_Value.chars(result);
	}
}

static
struct eccvalue_t toLowerCase (struct eccstate_t * const context)
{
	struct io_libecc_Chars *chars;
	struct io_libecc_Text text;
	
	if (!io_libecc_Value.isString(context->this))
		context->this = io_libecc_Value.toString(context, io_libecc_Context.this(context));
	
	text = io_libecc_Value.textOf(&context->this);
	{
		char buffer[text.length * 2];
		char *end = io_libecc_Text.toLower(text, buffer);
		chars = io_libecc_Chars.createWithBytes((int32_t)(end - buffer), buffer);
	}
	
	return io_libecc_Value.chars(chars);
}

static
struct eccvalue_t toUpperCase (struct eccstate_t * const context)
{
	struct io_libecc_Chars *chars;
	struct io_libecc_Text text;
	
	context->this = io_libecc_Value.toString(context, io_libecc_Context.this(context));
	text = io_libecc_Value.textOf(&context->this);
	{
		char buffer[text.length * 3];
		char *end = io_libecc_Text.toUpper(text, buffer);
		chars = io_libecc_Chars.createWithBytes((int32_t)(end - buffer), buffer);
	}
	
	return io_libecc_Value.chars(chars);
}

static
struct eccvalue_t trim (struct eccstate_t * const context)
{
	struct io_libecc_Chars *chars;
	struct io_libecc_Text text, last;
	struct io_libecc_text_Char c;
	
	if (!io_libecc_Value.isString(context->this))
		context->this = io_libecc_Value.toString(context, io_libecc_Context.this(context));
	
	text = io_libecc_Value.textOf(&context->this);
	while (text.length)
	{
		c = io_libecc_Text.character(text);
		if (!io_libecc_Text.isSpace(c))
			break;
		
		io_libecc_Text.advance(&text, c.units);
	}
	
	last = io_libecc_Text.make(text.bytes + text.length, text.length);
	while (last.length)
	{
		c = io_libecc_Text.prevCharacter(&last);
		if (!io_libecc_Text.isSpace(c))
			break;
		
		text.length = last.length;
	}
	
	chars = io_libecc_Chars.createWithBytes(text.length, text.bytes);
	
	return io_libecc_Value.chars(chars);
}

static
struct eccvalue_t constructor (struct eccstate_t * const context)
{
	struct eccvalue_t value;
	
	value = io_libecc_Context.argument(context, 0);
	if (value.type == io_libecc_value_undefinedType)
		value = io_libecc_Value.text(value.check == 1? &io_libecc_text_undefined: &io_libecc_text_empty);
	else
		value = io_libecc_Value.toString(context, value);
	
	if (context->construct)
		return io_libecc_Value.string(create(io_libecc_Chars.createWithBytes(io_libecc_Value.stringLength(&value), io_libecc_Value.stringBytes(&value))));
	else
		return value;
}

static
struct eccvalue_t fromCharCode (struct eccstate_t * const context)
{
	struct io_libecc_chars_Append chars;
	int32_t index, count;
	
	count = io_libecc_Context.argumentCount(context);
	
	io_libecc_Chars.beginAppend(&chars);
	
	for (index = 0; index < count; ++index)
		io_libecc_Chars.appendCodepoint(&chars, (uint16_t)io_libecc_Value.toInteger(context, io_libecc_Context.argument(context, index)).data.integer);
	
	return io_libecc_Chars.endAppend(&chars);
}

// MARK: - Methods

void setup ()
{
	const enum io_libecc_value_Flags h = io_libecc_value_hidden;
	
	io_libecc_Function.setupBuiltinObject(
		&io_libecc_string_constructor, constructor, 1,
		&io_libecc_string_prototype, io_libecc_Value.string(create(io_libecc_Chars.createSized(0))),
		&io_libecc_string_type);
	
	io_libecc_Function.addMethod(io_libecc_string_constructor, "fromCharCode", fromCharCode, -1, h);
	
	io_libecc_Function.addToObject(io_libecc_string_prototype, "toString", toString, 0, h);
	io_libecc_Function.addToObject(io_libecc_string_prototype, "valueOf", valueOf, 0, h);
	io_libecc_Function.addToObject(io_libecc_string_prototype, "charAt", charAt, 1, h);
	io_libecc_Function.addToObject(io_libecc_string_prototype, "charCodeAt", charCodeAt, 1, h);
	io_libecc_Function.addToObject(io_libecc_string_prototype, "concat", concat, -1, h);
	io_libecc_Function.addToObject(io_libecc_string_prototype, "indexOf", indexOf, -1, h);
	io_libecc_Function.addToObject(io_libecc_string_prototype, "lastIndexOf", lastIndexOf, -1, h);
	io_libecc_Function.addToObject(io_libecc_string_prototype, "localeCompare", localeCompare, 1, h);
	io_libecc_Function.addToObject(io_libecc_string_prototype, "match", match, 1, h);
	io_libecc_Function.addToObject(io_libecc_string_prototype, "replace", replace, 2, h);
	io_libecc_Function.addToObject(io_libecc_string_prototype, "search", search, 1, h);
	io_libecc_Function.addToObject(io_libecc_string_prototype, "slice", slice, 2, h);
	io_libecc_Function.addToObject(io_libecc_string_prototype, "split", split, 2, h);
	io_libecc_Function.addToObject(io_libecc_string_prototype, "substring", substring, 2, h);
	io_libecc_Function.addToObject(io_libecc_string_prototype, "toLowerCase", toLowerCase, 0, h);
	io_libecc_Function.addToObject(io_libecc_string_prototype, "toLocaleLowerCase", toLowerCase, 0, h);
	io_libecc_Function.addToObject(io_libecc_string_prototype, "toUpperCase", toUpperCase, 0, h);
	io_libecc_Function.addToObject(io_libecc_string_prototype, "toLocaleUpperCase", toUpperCase, 0, h);
	io_libecc_Function.addToObject(io_libecc_string_prototype, "trim", trim, 0, h);
}

void teardown (void)
{
	io_libecc_string_prototype = NULL;
	io_libecc_string_constructor = NULL;
}

struct io_libecc_String * create (struct io_libecc_Chars *chars)
{
	const enum io_libecc_value_Flags r = io_libecc_value_readonly;
	const enum io_libecc_value_Flags h = io_libecc_value_hidden;
	const enum io_libecc_value_Flags s = io_libecc_value_sealed;
	uint32_t length;
	
	struct io_libecc_String *self = malloc(sizeof(*self));
	*self = io_libecc_String.identity;
	io_libecc_Pool.addObject(&self->object);
	
	io_libecc_Object.initialize(&self->object, io_libecc_string_prototype);
	
	length = unitIndex(chars->bytes, chars->length, chars->length);
	io_libecc_Object.addMember(&self->object, io_libecc_key_length, io_libecc_Value.integer(length), r|h|s);
	
	self->value = chars;
	if (length == chars->length)
		chars->flags |= io_libecc_chars_asciiOnly;
	
	return self;
}

struct eccvalue_t valueAtIndex (struct io_libecc_String *self, int32_t index)
{
	struct io_libecc_text_Char c;
	struct io_libecc_Text text;
	
	text = textAtIndex(self->value->bytes, self->value->length, index, 0);
	c = io_libecc_Text.character(text);
	
	if (c.units <= 0)
		return io_libecc_value_undefined;
	else
	{
		if (c.codepoint < 0x010000)
			return io_libecc_Value.buffer(text.bytes, c.units);
		else
		{
			char buffer[7];
			
			/* simulate 16-bit surrogate */
			
			c.codepoint -= 0x010000;
			if (text.flags & io_libecc_text_breakFlag)
				c.codepoint = ((c.codepoint >>  0) & 0x3ff) + 0xdc00;
			else
				c.codepoint = ((c.codepoint >> 10) & 0x3ff) + 0xd800;
			
			io_libecc_Chars.writeCodepoint(buffer, c.codepoint);
			return io_libecc_Value.buffer(buffer, 3);
		}
	}
}

struct io_libecc_Text textAtIndex (const char *chars, int32_t length, int32_t position, int enableReverse)
{
	struct io_libecc_Text text = io_libecc_Text.make(chars, length), prev;
	struct io_libecc_text_Char c;
	
	if (position >= 0)
	{
		while (position-- > 0)
		{
			prev = text;
			c = io_libecc_Text.nextCharacter(&text);
			
			if (c.codepoint > 0xffff && !position--)
			{
				/* simulate 16-bit surrogate */
				text = prev;
				text.flags = io_libecc_text_breakFlag;
			}
		}
	}
	else if (enableReverse)
	{
		text.bytes += length;
		
		while (position++ < 0)
		{
			c = io_libecc_Text.prevCharacter(&text);
			
			if (c.codepoint > 0xffff && position++ >= 0)
			{
				/* simulate 16-bit surrogate */
				text.flags = io_libecc_text_breakFlag;
			}
		}
		
		text.length = length - (int32_t)(text.bytes - chars);
	}
	else
		text.length = 0;
	
	return text;
}

int32_t unitIndex (const char *chars, int32_t max, int32_t unit)
{
	struct io_libecc_Text text = io_libecc_Text.make(chars, max);
	int32_t position = 0;
	struct io_libecc_text_Char c;
	
	while (unit > 0)
	{
		if (text.length)
		{
			++position;
			c = io_libecc_Text.nextCharacter(&text);
			unit -= c.units;
			
			if (c.codepoint > 0xffff) /* simulate 16-bit surrogate */
				++position;
		}
	}
	
	return position;
}
