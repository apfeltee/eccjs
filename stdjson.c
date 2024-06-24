//
//  json.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"

// MARK: - Private

struct Parse {
	struct io_libecc_Text text;
	const char *start;
	int line;
	
	// reviver
	
	struct eccstate_t context;
	struct io_libecc_Function * function;
	struct eccobject_t * arguments;
	const struct io_libecc_Op * ops;
};

struct Stringify {
	struct io_libecc_chars_Append chars;
	char spaces[11];
	int level;
	struct eccobject_t *filter;
	
	// replacer
	
	struct eccstate_t context;
	struct io_libecc_Function * function;
	struct eccobject_t * arguments;
	const struct io_libecc_Op * ops;
};

const struct io_libecc_object_Type io_libecc_json_type = {
	.text = &io_libecc_text_jsonType,
};

static void setup(void);
static void teardown(void);
const struct type_io_libecc_JSON io_libecc_JSON = {
    setup,
    teardown,
};

static
struct eccvalue_t error (struct Parse *parse, int length, struct io_libecc_Chars *chars)
{
	return io_libecc_Value.error(io_libecc_Error.syntaxError(io_libecc_Text.make(parse->text.bytes + (length < 0? length: 0), abs(length)), chars));
}

static
struct io_libecc_Text errorOfLine (struct Parse *parse)
{
	struct io_libecc_Text text = { 0 };
	text.bytes = parse->start;
	while (parse->text.length)
	{
		struct io_libecc_text_Char c = io_libecc_Text.character(parse->text);
		if (c.codepoint == '\r' || c.codepoint == '\n')
			break;
		
		io_libecc_Text.advance(&parse->text, 1);
	}
	text.length = (int32_t)(parse->text.bytes - parse->start);
	return text;
}

static
struct io_libecc_text_Char nextc (struct Parse *parse)
{
	struct io_libecc_text_Char c = { 0 };
	while (parse->text.length)
	{
		c = io_libecc_Text.nextCharacter(&parse->text);
		
		if (c.codepoint == '\r' && io_libecc_Text.character(parse->text).codepoint == '\n')
			io_libecc_Text.advance(&parse->text, 1);
		
		if (c.codepoint == '\r' || c.codepoint == '\n')
		{
			parse->start = parse->text.bytes;
			++parse->line;
		}
		
		if (!isspace(c.codepoint))
			break;
	}
	return c;
}

static
struct io_libecc_Text string (struct Parse *parse)
{
	struct io_libecc_Text text = { parse->text.bytes };
	struct io_libecc_text_Char c;
	
	do
	{
		c = io_libecc_Text.nextCharacter(&parse->text);
		if (c.codepoint == '\\')
			io_libecc_Text.advance(&parse->text, 1);
	}
	while (c.codepoint != '\"' && parse->text.length);
	
	text.length = (int32_t)(parse->text.bytes - text.bytes - 1);
	return text;
}

static struct eccvalue_t object (struct Parse *parse);
static struct eccvalue_t array (struct Parse *parse);

static
struct eccvalue_t literal (struct Parse *parse)
{
	struct io_libecc_text_Char c = nextc(parse);
	
	switch (c.codepoint)
	{
		case 't':
			if (!memcmp(parse->text.bytes, "rue", 3))
			{
				io_libecc_Text.advance(&parse->text, 3);
				return io_libecc_value_true;
			}
			break;
			
		case 'f':
			if (!memcmp(parse->text.bytes, "alse", 4))
			{
				io_libecc_Text.advance(&parse->text, 4);
				return io_libecc_value_false;
			}
			break;
			
		case 'n':
			if (!memcmp(parse->text.bytes, "ull", 3))
			{
				io_libecc_Text.advance(&parse->text, 3);
				return io_libecc_value_null;
			}
			break;
			
		case '-':
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
			struct io_libecc_Text text = io_libecc_Text.make(parse->text.bytes - 1, 1);
			
			if (c.codepoint == '-')
				c = io_libecc_Text.nextCharacter(&parse->text);
			
			if (c.codepoint != '0')
				while (parse->text.length)
				{
					c = io_libecc_Text.nextCharacter(&parse->text);
					if (!isdigit(c.codepoint))
						break;
				}
			else
				c = io_libecc_Text.nextCharacter(&parse->text);
			
			if (c.codepoint == '.')
			{
				while (parse->text.length)
				{
					c = io_libecc_Text.nextCharacter(&parse->text);
					if (!isdigit(c.codepoint))
						break;
				}
			}
			
			if (c.codepoint == 'e' || c.codepoint == 'E')
			{
				c = io_libecc_Text.nextCharacter(&parse->text);
				if (c.codepoint == '+' || c.codepoint == '-')
					c = io_libecc_Text.nextCharacter(&parse->text);
				
				while (parse->text.length)
				{
					c = io_libecc_Text.nextCharacter(&parse->text);
					if (!isdigit(c.codepoint))
						break;
				}
			}
			
			io_libecc_Text.advance(&parse->text, -c.units);
			text.length = (int32_t)(parse->text.bytes - text.bytes);
			
			return io_libecc_Lexer.scanBinary(text, 0);
		}
			
		case '"':
		{
			struct io_libecc_Text text = string(parse);
			return io_libecc_Value.chars(io_libecc_Chars.createWithBytes(text.length, text.bytes));
			break;
		}
			
		case '{':
			return object(parse);
			break;
			
		case '[':
			return array(parse);
			break;
	}
	return error(parse, -c.units, io_libecc_Chars.create("unexpected '%.*s'", c.units, parse->text.bytes - c.units));
}

static
struct eccvalue_t object (struct Parse *parse)
{
	struct eccobject_t *object = io_libecc_Object.create(io_libecc_object_prototype);
	struct io_libecc_text_Char c;
	struct eccvalue_t value;
	struct io_libecc_Key key;
	
	c = nextc(parse);
	if (c.codepoint != '}')
		for (;;)
		{
			if (c.codepoint != '"')
				return error(parse, -c.units, io_libecc_Chars.create("expect property name"));
			
			key = io_libecc_Key.makeWithText(string(parse), io_libecc_key_copyOnCreate);
			
			c = nextc(parse);
			if (c.codepoint != ':')
				return error(parse, -c.units, io_libecc_Chars.create("expect colon"));
			
			value = literal(parse);
			if (value.type == io_libecc_value_errorType)
				return value;
			
			io_libecc_Object.addMember(object, key, value, 0);
			
			c = nextc(parse);
			
			if (c.codepoint == '}')
				break;
			else if (c.codepoint == ',')
				c = nextc(parse);
			else
				return error(parse, -c.units, io_libecc_Chars.create("unexpected '%.*s'", c.units, parse->text.bytes - c.units));
		}
	
	return io_libecc_Value.object(object);
}

static
struct eccvalue_t array (struct Parse *parse)
{
	struct eccobject_t *object = io_libecc_Array.create();
	struct io_libecc_text_Char c;
	struct eccvalue_t value;
	
	for (;;)
	{
		value = literal(parse);
		if (value.type == io_libecc_value_errorType)
			return value;
		
		io_libecc_Object.addElement(object, object->elementCount, value, 0);
		
		c = nextc(parse);
		
		if (c.codepoint == ',')
			continue;
		
		if (c.codepoint == ']')
			break;
		
		return error(parse, -c.units, io_libecc_Chars.create("unexpected '%.*s'", c.units, parse->text.bytes - c.units));
	}
	return io_libecc_Value.object(object);
}

static
struct eccvalue_t json (struct Parse *parse)
{
	struct io_libecc_text_Char c = nextc(parse);
	
	if (c.codepoint == '{')
		return object(parse);
	else if (c.codepoint == '[')
		return array(parse);
	
	return error(parse, -c.units, io_libecc_Chars.create("expect { or ["));
}

// MARK: - Static Members

static
struct eccvalue_t revive (struct Parse *parse, struct eccvalue_t this, struct eccvalue_t property, struct eccvalue_t value)
{
	uint16_t hashmapCount;
	
	hashmapCount = parse->context.environment->hashmapCount;
	switch (hashmapCount) {
		default:
			memcpy(parse->context.environment->hashmap + 5,
				   parse->function->environment.hashmap,
				   sizeof(*parse->context.environment->hashmap) * (hashmapCount - 5));
		case 5:
			parse->context.environment->hashmap[3 + 1].value = value;
		case 4:
			parse->context.environment->hashmap[3 + 0].value = property;
		case 3:
			break;
		case 2:
		case 1:
		case 0:
			assert(0);
			break;
	}
	
	parse->context.ops = parse->ops;
	parse->context.this = this;
	parse->arguments->element[0].value = property;
	parse->arguments->element[1].value = value;
	return parse->context.ops->native(&parse->context);
}

static
struct eccvalue_t walker (struct Parse *parse, struct eccvalue_t this, struct eccvalue_t property, struct eccvalue_t value)
{
	uint32_t index, count;
	struct io_libecc_chars_Append chars;
	
	if (io_libecc_Value.isObject(value))
	{
		struct eccobject_t *object = value.data.object;
		
		for (index = 0, count = object->elementCount < io_libecc_object_ElementMax? object->elementCount: io_libecc_object_ElementMax; index < count; ++index)
		{
			if (object->element[index].value.check == 1)
			{
				io_libecc_Chars.beginAppend(&chars);
				io_libecc_Chars.append(&chars, "%d", index);
				object->element[index].value = walker(parse, this, io_libecc_Chars.endAppend(&chars), object->element[index].value);
			}
		}
		
		for (index = 2; index < object->hashmapCount; ++index)
		{
			if (object->hashmap[index].value.check == 1)
				object->hashmap[index].value = walker(parse, this, io_libecc_Value.key(object->hashmap[index].value.key), object->hashmap[index].value);
		}
	}
	return revive(parse, this, property, value);
}

static
struct eccvalue_t jsonParse (struct eccstate_t * const context)
{
	struct eccvalue_t value, reviver, result;
	struct Parse parse = {
		.context = {
			.parent = context,
			.ecc = context->ecc,
			.depth = context->depth + 1,
			.textIndex = io_libecc_context_callIndex,
		},
	};
	
	value = io_libecc_Value.toString(context, io_libecc_Context.argument(context, 0));
	reviver = io_libecc_Context.argument(context, 1);
	
	parse.text = io_libecc_Text.make(io_libecc_Value.stringBytes(&value), io_libecc_Value.stringLength(&value));
	parse.start = parse.text.bytes;
	parse.line = 1;
	parse.function = reviver.type == io_libecc_value_functionType? reviver.data.function: NULL;
	parse.ops = parse.function? parse.function->oplist->ops: NULL;
	
	result = json(&parse);
	
	if (result.type != io_libecc_value_errorType && parse.text.length)
	{
		struct io_libecc_text_Char c = io_libecc_Text.character(parse.text);
		result = error(&parse, c.units, io_libecc_Chars.create("unexpected '%.*s'", c.units, parse.text.bytes));
	}
	
	if (result.type == io_libecc_value_errorType)
	{
		io_libecc_Context.setTextIndex(context, io_libecc_context_noIndex);
		context->ecc->ofLine = parse.line;
		context->ecc->ofText = errorOfLine(&parse);
		context->ecc->ofInput = "(parse)";
		io_libecc_Context.throw(context, result);
	}
	
	if (parse.function && parse.function->flags & io_libecc_function_needHeap)
	{
		struct eccobject_t *environment = io_libecc_Object.copy(&parse.function->environment);
		
		parse.context.environment = environment;
		parse.arguments = io_libecc_Arguments.createSized(2);
		++parse.arguments->referenceCount;
		
		environment->hashmap[2].value = io_libecc_Value.object(parse.arguments);
		
		result = walker(&parse, result, io_libecc_Value.text(&io_libecc_text_empty), result);
	}
	else if (parse.function)
	{
		struct eccobject_t environment = parse.function->environment;
		struct eccobject_t arguments = io_libecc_Object.identity;
		union io_libecc_object_Hashmap hashmap[parse.function->environment.hashmapCapacity];
		union io_libecc_object_Element element[2];
		
		memcpy(hashmap, parse.function->environment.hashmap, sizeof(hashmap));
		parse.context.environment = &environment;
		parse.arguments = &arguments;
		
		arguments.element = element;
		arguments.elementCount = 2;
		environment.hashmap = hashmap;
		environment.hashmap[2].value = io_libecc_Value.object(&arguments);
		
		result = walker(&parse, result, io_libecc_Value.text(&io_libecc_text_empty), result);
	}
	return result;
}

static
struct eccvalue_t replace (struct Stringify *stringify, struct eccvalue_t this, struct eccvalue_t property, struct eccvalue_t value)
{
	uint16_t hashmapCount;
	
	hashmapCount = stringify->context.environment->hashmapCount;
	switch (hashmapCount) {
		default:
			memcpy(stringify->context.environment->hashmap + 5,
				   stringify->function->environment.hashmap,
				   sizeof(*stringify->context.environment->hashmap) * (hashmapCount - 5));
		case 5:
			stringify->context.environment->hashmap[3 + 1].value = value;
		case 4:
			stringify->context.environment->hashmap[3 + 0].value = property;
		case 3:
			break;
		case 2:
		case 1:
		case 0:
			assert(0);
			break;
	}
	
	stringify->context.ops = stringify->ops;
	stringify->context.this = this;
	stringify->arguments->element[0].value = property;
	stringify->arguments->element[1].value = value;
	return stringify->context.ops->native(&stringify->context);
}

static
int stringifyValue (struct Stringify *stringify, struct eccvalue_t this, struct eccvalue_t property, struct eccvalue_t value, int isArray, int addComa)
{
	uint32_t index, count;
	
	if (stringify->function)
		value = replace(stringify, this, property, value);
	
	if (!isArray)
	{
		if (value.type == io_libecc_value_undefinedType)
			return 0;
		
		if (stringify->filter)
		{
			struct eccobject_t *object = stringify->filter;
			int found = 0;
			
			for (index = 0, count = object->elementCount < io_libecc_object_ElementMax? object->elementCount: io_libecc_object_ElementMax; index < count; ++index)
			{
				if (object->element[index].value.check == 1)
				{
					if (io_libecc_Value.isTrue(io_libecc_Value.equals(&stringify->context, property, object->element[index].value)))
					{
						found = 1;
						break;
					}
				}
			}
			
			if (!found)
				return 0;
		}
	}
	
	if (addComa)
		io_libecc_Chars.append(&stringify->chars, ",%s", strlen(stringify->spaces)? "\n": "");
	
	for (index = 0, count = stringify->level; index < count; ++index)
		io_libecc_Chars.append(&stringify->chars, "%s", stringify->spaces);
	
	if (!isArray)
		io_libecc_Chars.append(&stringify->chars, "\"%.*s\":%s", io_libecc_Value.stringLength(&property), io_libecc_Value.stringBytes(&property), strlen(stringify->spaces)? " ": "");
	
	if (value.type == io_libecc_value_functionType || value.type == io_libecc_value_undefinedType)
		io_libecc_Chars.append(&stringify->chars, "null");
	else if (io_libecc_Value.isObject(value))
	{
		struct eccobject_t *object = value.data.object;
		int isArray = io_libecc_Value.objectIsArray(object);
		struct io_libecc_chars_Append chars;
		const struct io_libecc_Text *property;
		int hasValue = 0;
		
		io_libecc_Chars.append(&stringify->chars, "%s%s", isArray? "[": "{", strlen(stringify->spaces)? "\n": "");
		++stringify->level;
		
		for (index = 0, count = object->elementCount < io_libecc_object_ElementMax? object->elementCount: io_libecc_object_ElementMax; index < count; ++index)
		{
			if (object->element[index].value.check == 1)
			{
				io_libecc_Chars.beginAppend(&chars);
				io_libecc_Chars.append(&chars, "%d", index);
				hasValue |= stringifyValue(stringify, value, io_libecc_Chars.endAppend(&chars), object->element[index].value, isArray, hasValue);
			}
		}
		
		if (!isArray)
		{
			for (index = 0; index < object->hashmapCount; ++index)
			{
				if (object->hashmap[index].value.check == 1)
				{
					property = io_libecc_Key.textOf(object->hashmap[index].value.key);
					hasValue |= stringifyValue(stringify, value, io_libecc_Value.text(property), object->hashmap[index].value, isArray, hasValue);
				}
			}
		}
		
		io_libecc_Chars.append(&stringify->chars, "%s", strlen(stringify->spaces)? "\n": "");
		
		--stringify->level;
		for (index = 0, count = stringify->level; index < count; ++index)
			io_libecc_Chars.append(&stringify->chars, "%s", stringify->spaces);
		
		io_libecc_Chars.append(&stringify->chars, "%s", isArray? "]": "}");
	}
	else
		io_libecc_Chars.appendValue(&stringify->chars, &stringify->context, value);
	
	return 1;
}

static
struct eccvalue_t jsonStringify (struct eccstate_t * const context)
{
	struct eccvalue_t value, replacer, space;
	struct Stringify stringify = {
		.context = {
			.parent = context,
			.ecc = context->ecc,
			.depth = context->depth + 1,
			.textIndex = io_libecc_context_callIndex,
		},
		.spaces = { 0 },
	};
	
	value = io_libecc_Context.argument(context, 0);
	replacer = io_libecc_Context.argument(context, 1);
	space = io_libecc_Context.argument(context, 2);
	
	stringify.filter = replacer.type == io_libecc_value_objectType && replacer.data.object->type == &io_libecc_array_type? replacer.data.object: NULL;
	stringify.function = replacer.type == io_libecc_value_functionType? replacer.data.function: NULL;
	stringify.ops = stringify.function? stringify.function->oplist->ops: NULL;
	
	if (io_libecc_Value.isString(space))
		snprintf(stringify.spaces, sizeof(stringify.spaces), "%.*s", (int)io_libecc_Value.stringLength(&space), io_libecc_Value.stringBytes(&space));
	else if (io_libecc_Value.isNumber(space))
	{
		int i = io_libecc_Value.toInteger(context, space).data.integer;
		
		if (i < 0)
			i = 0;
		
		if (i > 10)
			i = 10;
		
		while (i--)
			strcat(stringify.spaces, " ");
	}
	
	io_libecc_Chars.beginAppend(&stringify.chars);
	
	if (stringify.function && stringify.function->flags & io_libecc_function_needHeap)
	{
		struct eccobject_t *environment = io_libecc_Object.copy(&stringify.function->environment);
		
		stringify.context.environment = environment;
		stringify.arguments = io_libecc_Arguments.createSized(2);
		++stringify.arguments->referenceCount;
		
		environment->hashmap[2].value = io_libecc_Value.object(stringify.arguments);
		
		stringifyValue(&stringify, value, io_libecc_Value.text(&io_libecc_text_empty), value, 1, 0);
	}
	else if (stringify.function)
	{
		struct eccobject_t environment = stringify.function->environment;
		struct eccobject_t arguments = io_libecc_Object.identity;
		union io_libecc_object_Hashmap hashmap[stringify.function->environment.hashmapCapacity];
		union io_libecc_object_Element element[2];
		
		memcpy(hashmap, stringify.function->environment.hashmap, sizeof(hashmap));
		stringify.context.environment = &environment;
		stringify.arguments = &arguments;
		
		arguments.element = element;
		arguments.elementCount = 2;
		environment.hashmap = hashmap;
		environment.hashmap[2].value = io_libecc_Value.object(&arguments);
		
		stringifyValue(&stringify, value, io_libecc_Value.text(&io_libecc_text_empty), value, 1, 0);
	}
	else
		stringifyValue(&stringify, value, io_libecc_Value.text(&io_libecc_text_empty), value, 1, 0);
	
	return io_libecc_Chars.endAppend(&stringify.chars);
}

// MARK: - Public

struct eccobject_t * io_libecc_json_object = NULL;

void setup ()
{
	const enum io_libecc_value_Flags h = io_libecc_value_hidden;
	
	io_libecc_json_object = io_libecc_Object.createTyped(&io_libecc_json_type);
	
	io_libecc_Function.addToObject(io_libecc_json_object, "parse", jsonParse, -1, h);
	io_libecc_Function.addToObject(io_libecc_json_object, "stringify", jsonStringify, -1, h);
}

void teardown (void)
{
	io_libecc_json_object = NULL;
}
