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
	ecctextstring_t text;
	const char *start;
	int line;
	
	// reviver
	
	eccstate_t context;
	struct io_libecc_Function * function;
	eccobject_t * arguments;
	const struct io_libecc_Op * ops;
};

struct Stringify {
	struct io_libecc_chars_Append chars;
	char spaces[11];
	int level;
	eccobject_t *filter;
	
	// replacer
	
	eccstate_t context;
	struct io_libecc_Function * function;
	eccobject_t * arguments;
	const struct io_libecc_Op * ops;
};

const struct io_libecc_object_Type io_libecc_json_type = {
	.text = &ECC_ConstString_JsonType,
};

static void setup(void);
static void teardown(void);
const struct type_io_libecc_JSON io_libecc_JSON = {
    setup,
    teardown,
};

static
eccvalue_t error (struct Parse *parse, int length, struct io_libecc_Chars *chars)
{
	return ECCNSValue.error(io_libecc_Error.syntaxError(ECCNSText.make(parse->text.bytes + (length < 0? length: 0), abs(length)), chars));
}

static
ecctextstring_t errorOfLine (struct Parse *parse)
{
	ecctextstring_t text = { 0 };
	text.bytes = parse->start;
	while (parse->text.length)
	{
		ecctextchar_t c = ECCNSText.character(parse->text);
		if (c.codepoint == '\r' || c.codepoint == '\n')
			break;
		
		ECCNSText.advance(&parse->text, 1);
	}
	text.length = (int32_t)(parse->text.bytes - parse->start);
	return text;
}

static
ecctextchar_t nextc (struct Parse *parse)
{
	ecctextchar_t c = { 0 };
	while (parse->text.length)
	{
		c = ECCNSText.nextCharacter(&parse->text);
		
		if (c.codepoint == '\r' && ECCNSText.character(parse->text).codepoint == '\n')
			ECCNSText.advance(&parse->text, 1);
		
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
ecctextstring_t string (struct Parse *parse)
{
	ecctextstring_t text = { parse->text.bytes };
	ecctextchar_t c;
	
	do
	{
		c = ECCNSText.nextCharacter(&parse->text);
		if (c.codepoint == '\\')
			ECCNSText.advance(&parse->text, 1);
	}
	while (c.codepoint != '\"' && parse->text.length);
	
	text.length = (int32_t)(parse->text.bytes - text.bytes - 1);
	return text;
}

static eccvalue_t object (struct Parse *parse);
static eccvalue_t array (struct Parse *parse);

static
eccvalue_t literal (struct Parse *parse)
{
	ecctextchar_t c = nextc(parse);
	
	switch (c.codepoint)
	{
		case 't':
			if (!memcmp(parse->text.bytes, "rue", 3))
			{
				ECCNSText.advance(&parse->text, 3);
				return ECCValConstTrue;
			}
			break;
			
		case 'f':
			if (!memcmp(parse->text.bytes, "alse", 4))
			{
				ECCNSText.advance(&parse->text, 4);
				return ECCValConstFalse;
			}
			break;
			
		case 'n':
			if (!memcmp(parse->text.bytes, "ull", 3))
			{
				ECCNSText.advance(&parse->text, 3);
				return ECCValConstNull;
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
			ecctextstring_t text = ECCNSText.make(parse->text.bytes - 1, 1);
			
			if (c.codepoint == '-')
				c = ECCNSText.nextCharacter(&parse->text);
			
			if (c.codepoint != '0')
				while (parse->text.length)
				{
					c = ECCNSText.nextCharacter(&parse->text);
					if (!isdigit(c.codepoint))
						break;
				}
			else
				c = ECCNSText.nextCharacter(&parse->text);
			
			if (c.codepoint == '.')
			{
				while (parse->text.length)
				{
					c = ECCNSText.nextCharacter(&parse->text);
					if (!isdigit(c.codepoint))
						break;
				}
			}
			
			if (c.codepoint == 'e' || c.codepoint == 'E')
			{
				c = ECCNSText.nextCharacter(&parse->text);
				if (c.codepoint == '+' || c.codepoint == '-')
					c = ECCNSText.nextCharacter(&parse->text);
				
				while (parse->text.length)
				{
					c = ECCNSText.nextCharacter(&parse->text);
					if (!isdigit(c.codepoint))
						break;
				}
			}
			
			ECCNSText.advance(&parse->text, -c.units);
			text.length = (int32_t)(parse->text.bytes - text.bytes);
			
			return io_libecc_Lexer.scanBinary(text, 0);
		}
			
		case '"':
		{
			ecctextstring_t text = string(parse);
			return ECCNSValue.chars(io_libecc_Chars.createWithBytes(text.length, text.bytes));
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
eccvalue_t object (struct Parse *parse)
{
	eccobject_t *object = ECCNSObject.create(io_libecc_object_prototype);
	ecctextchar_t c;
	eccvalue_t value;
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
			if (value.type == ECC_VALTYPE_ERROR)
				return value;
			
			ECCNSObject.addMember(object, key, value, 0);
			
			c = nextc(parse);
			
			if (c.codepoint == '}')
				break;
			else if (c.codepoint == ',')
				c = nextc(parse);
			else
				return error(parse, -c.units, io_libecc_Chars.create("unexpected '%.*s'", c.units, parse->text.bytes - c.units));
		}
	
	return ECCNSValue.object(object);
}

static
eccvalue_t array (struct Parse *parse)
{
	eccobject_t *object = io_libecc_Array.create();
	ecctextchar_t c;
	eccvalue_t value;
	
	for (;;)
	{
		value = literal(parse);
		if (value.type == ECC_VALTYPE_ERROR)
			return value;
		
		ECCNSObject.addElement(object, object->elementCount, value, 0);
		
		c = nextc(parse);
		
		if (c.codepoint == ',')
			continue;
		
		if (c.codepoint == ']')
			break;
		
		return error(parse, -c.units, io_libecc_Chars.create("unexpected '%.*s'", c.units, parse->text.bytes - c.units));
	}
	return ECCNSValue.object(object);
}

static
eccvalue_t json (struct Parse *parse)
{
	ecctextchar_t c = nextc(parse);
	
	if (c.codepoint == '{')
		return object(parse);
	else if (c.codepoint == '[')
		return array(parse);
	
	return error(parse, -c.units, io_libecc_Chars.create("expect { or ["));
}

// MARK: - Static Members

static
eccvalue_t revive (struct Parse *parse, eccvalue_t this, eccvalue_t property, eccvalue_t value)
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
eccvalue_t walker (struct Parse *parse, eccvalue_t this, eccvalue_t property, eccvalue_t value)
{
	uint32_t index, count;
	struct io_libecc_chars_Append chars;
	
	if (ECCNSValue.isObject(value))
	{
		eccobject_t *object = value.data.object;
		
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
				object->hashmap[index].value = walker(parse, this, ECCNSValue.key(object->hashmap[index].value.key), object->hashmap[index].value);
		}
	}
	return revive(parse, this, property, value);
}

static
eccvalue_t jsonParse (eccstate_t * const context)
{
	eccvalue_t value, reviver, result;
	struct Parse parse = {
		.context = {
			.parent = context,
			.ecc = context->ecc,
			.depth = context->depth + 1,
			.textIndex = io_libecc_context_callIndex,
		},
	};
	
	value = ECCNSValue.toString(context, ECCNSContext.argument(context, 0));
	reviver = ECCNSContext.argument(context, 1);
	
	parse.text = ECCNSText.make(ECCNSValue.stringBytes(&value), ECCNSValue.stringLength(&value));
	parse.start = parse.text.bytes;
	parse.line = 1;
	parse.function = reviver.type == ECC_VALTYPE_FUNCTION? reviver.data.function: NULL;
	parse.ops = parse.function? parse.function->oplist->ops: NULL;
	
	result = json(&parse);
	
	if (result.type != ECC_VALTYPE_ERROR && parse.text.length)
	{
		ecctextchar_t c = ECCNSText.character(parse.text);
		result = error(&parse, c.units, io_libecc_Chars.create("unexpected '%.*s'", c.units, parse.text.bytes));
	}
	
	if (result.type == ECC_VALTYPE_ERROR)
	{
		ECCNSContext.setTextIndex(context, io_libecc_context_noIndex);
		context->ecc->ofLine = parse.line;
		context->ecc->ofText = errorOfLine(&parse);
		context->ecc->ofInput = "(parse)";
		ECCNSContext.throw(context, result);
	}
	
	if (parse.function && parse.function->flags & io_libecc_function_needHeap)
	{
		eccobject_t *environment = ECCNSObject.copy(&parse.function->environment);
		
		parse.context.environment = environment;
		parse.arguments = ECCNSArguments.createSized(2);
		++parse.arguments->referenceCount;
		
		environment->hashmap[2].value = ECCNSValue.object(parse.arguments);
		
		result = walker(&parse, result, ECCNSValue.text(&ECC_ConstString_Empty), result);
	}
	else if (parse.function)
	{
		eccobject_t environment = parse.function->environment;
		eccobject_t arguments = ECCNSObject.identity;
		union io_libecc_object_Hashmap hashmap[parse.function->environment.hashmapCapacity];
		union io_libecc_object_Element element[2];
		
		memcpy(hashmap, parse.function->environment.hashmap, sizeof(hashmap));
		parse.context.environment = &environment;
		parse.arguments = &arguments;
		
		arguments.element = element;
		arguments.elementCount = 2;
		environment.hashmap = hashmap;
		environment.hashmap[2].value = ECCNSValue.object(&arguments);
		
		result = walker(&parse, result, ECCNSValue.text(&ECC_ConstString_Empty), result);
	}
	return result;
}

static
eccvalue_t replace (struct Stringify *stringify, eccvalue_t this, eccvalue_t property, eccvalue_t value)
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
int stringifyValue (struct Stringify *stringify, eccvalue_t this, eccvalue_t property, eccvalue_t value, int isArray, int addComa)
{
	uint32_t index, count;
	
	if (stringify->function)
		value = replace(stringify, this, property, value);
	
	if (!isArray)
	{
		if (value.type == ECC_VALTYPE_UNDEFINED)
			return 0;
		
		if (stringify->filter)
		{
			eccobject_t *object = stringify->filter;
			int found = 0;
			
			for (index = 0, count = object->elementCount < io_libecc_object_ElementMax? object->elementCount: io_libecc_object_ElementMax; index < count; ++index)
			{
				if (object->element[index].value.check == 1)
				{
					if (ECCNSValue.isTrue(ECCNSValue.equals(&stringify->context, property, object->element[index].value)))
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
		io_libecc_Chars.append(&stringify->chars, "\"%.*s\":%s", ECCNSValue.stringLength(&property), ECCNSValue.stringBytes(&property), strlen(stringify->spaces)? " ": "");
	
	if (value.type == ECC_VALTYPE_FUNCTION || value.type == ECC_VALTYPE_UNDEFINED)
		io_libecc_Chars.append(&stringify->chars, "null");
	else if (ECCNSValue.isObject(value))
	{
		eccobject_t *object = value.data.object;
		int isArray = ECCNSValue.objectIsArray(object);
		struct io_libecc_chars_Append chars;
		const ecctextstring_t *property;
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
					hasValue |= stringifyValue(stringify, value, ECCNSValue.text(property), object->hashmap[index].value, isArray, hasValue);
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
eccvalue_t jsonStringify (eccstate_t * const context)
{
	eccvalue_t value, replacer, space;
	struct Stringify stringify = {
		.context = {
			.parent = context,
			.ecc = context->ecc,
			.depth = context->depth + 1,
			.textIndex = io_libecc_context_callIndex,
		},
		.spaces = { 0 },
	};
	
	value = ECCNSContext.argument(context, 0);
	replacer = ECCNSContext.argument(context, 1);
	space = ECCNSContext.argument(context, 2);
	
	stringify.filter = replacer.type == ECC_VALTYPE_OBJECT && replacer.data.object->type == &io_libecc_array_type? replacer.data.object: NULL;
	stringify.function = replacer.type == ECC_VALTYPE_FUNCTION? replacer.data.function: NULL;
	stringify.ops = stringify.function? stringify.function->oplist->ops: NULL;
	
	if (ECCNSValue.isString(space))
		snprintf(stringify.spaces, sizeof(stringify.spaces), "%.*s", (int)ECCNSValue.stringLength(&space), ECCNSValue.stringBytes(&space));
	else if (ECCNSValue.isNumber(space))
	{
		int i = ECCNSValue.toInteger(context, space).data.integer;
		
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
		eccobject_t *environment = ECCNSObject.copy(&stringify.function->environment);
		
		stringify.context.environment = environment;
		stringify.arguments = ECCNSArguments.createSized(2);
		++stringify.arguments->referenceCount;
		
		environment->hashmap[2].value = ECCNSValue.object(stringify.arguments);
		
		stringifyValue(&stringify, value, ECCNSValue.text(&ECC_ConstString_Empty), value, 1, 0);
	}
	else if (stringify.function)
	{
		eccobject_t environment = stringify.function->environment;
		eccobject_t arguments = ECCNSObject.identity;
		union io_libecc_object_Hashmap hashmap[stringify.function->environment.hashmapCapacity];
		union io_libecc_object_Element element[2];
		
		memcpy(hashmap, stringify.function->environment.hashmap, sizeof(hashmap));
		stringify.context.environment = &environment;
		stringify.arguments = &arguments;
		
		arguments.element = element;
		arguments.elementCount = 2;
		environment.hashmap = hashmap;
		environment.hashmap[2].value = ECCNSValue.object(&arguments);
		
		stringifyValue(&stringify, value, ECCNSValue.text(&ECC_ConstString_Empty), value, 1, 0);
	}
	else
		stringifyValue(&stringify, value, ECCNSValue.text(&ECC_ConstString_Empty), value, 1, 0);
	
	return io_libecc_Chars.endAppend(&stringify.chars);
}

// MARK: - Public

eccobject_t * io_libecc_json_object = NULL;

void setup ()
{
	const enum io_libecc_value_Flags h = io_libecc_value_hidden;
	
	io_libecc_json_object = ECCNSObject.createTyped(&io_libecc_json_type);
	
	io_libecc_Function.addToObject(io_libecc_json_object, "parse", jsonParse, -1, h);
	io_libecc_Function.addToObject(io_libecc_json_object, "stringify", jsonStringify, -1, h);
}

void teardown (void)
{
	io_libecc_json_object = NULL;
}
