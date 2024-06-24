//
//  value.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"

// MARK: - Private

#define valueMake(T) { .type = io_libecc_value_##T, .check = 1 }

const struct eccvalue_t io_libecc_value_none = {{ 0 }};
const struct eccvalue_t io_libecc_value_undefined = valueMake(undefinedType);
const struct eccvalue_t io_libecc_value_true = valueMake(trueType);
const struct eccvalue_t io_libecc_value_false = valueMake(falseType);
const struct eccvalue_t io_libecc_value_null = valueMake(nullType);

// MARK: - Static Members

// MARK: - Methods


// make

static struct eccvalue_t truth(int truth);
static struct eccvalue_t integer(int32_t integer);
static struct eccvalue_t binary(double binary);
static struct eccvalue_t buffer(const char buffer[7], uint8_t units);
static struct eccvalue_t key(struct io_libecc_Key key);
static struct eccvalue_t text(const struct io_libecc_Text* text);
static struct eccvalue_t chars(struct io_libecc_Chars* chars);
static struct eccvalue_t object(struct eccobject_t*);
static struct eccvalue_t error(struct io_libecc_Error*);
static struct eccvalue_t string(struct io_libecc_String*);
static struct eccvalue_t regexp(struct io_libecc_RegExp*);
static struct eccvalue_t number(struct io_libecc_Number*);
static struct eccvalue_t boolean(struct io_libecc_Boolean*);
static struct eccvalue_t date(struct io_libecc_Date*);
static struct eccvalue_t function(struct io_libecc_Function*);
static struct eccvalue_t host(struct eccobject_t*);
static struct eccvalue_t reference(struct eccvalue_t*);
static int isPrimitive(struct eccvalue_t);
static int isBoolean(struct eccvalue_t);
static int isNumber(struct eccvalue_t);
static int isString(struct eccvalue_t);
static int isObject(struct eccvalue_t);
static int isDynamic(struct eccvalue_t);
static int isTrue(struct eccvalue_t);
static struct eccvalue_t toPrimitive(struct eccstate_t* const, struct eccvalue_t, enum io_libecc_value_hintPrimitive);
static struct eccvalue_t toBinary(struct eccstate_t* const, struct eccvalue_t);
static struct eccvalue_t toInteger(struct eccstate_t* const, struct eccvalue_t);
static struct eccvalue_t binaryToString(double binary, int base);
static struct eccvalue_t toString(struct eccstate_t* const, struct eccvalue_t);
static int32_t stringLength(const struct eccvalue_t*);
static const char* stringBytes(const struct eccvalue_t*);
static struct io_libecc_Text textOf(const struct eccvalue_t* string);
static struct eccvalue_t toObject(struct eccstate_t* const, struct eccvalue_t);
static struct eccvalue_t objectValue(struct eccobject_t*);
static int objectIsArray(struct eccobject_t*);
static struct eccvalue_t toType(struct eccvalue_t);
static struct eccvalue_t equals(struct eccstate_t* const, struct eccvalue_t, struct eccvalue_t);
static struct eccvalue_t same(struct eccstate_t* const, struct eccvalue_t, struct eccvalue_t);
static struct eccvalue_t add(struct eccstate_t* const, struct eccvalue_t, struct eccvalue_t);
static struct eccvalue_t subtract(struct eccstate_t* const, struct eccvalue_t, struct eccvalue_t);
static struct eccvalue_t less(struct eccstate_t* const, struct eccvalue_t, struct eccvalue_t);
static struct eccvalue_t more(struct eccstate_t* const, struct eccvalue_t, struct eccvalue_t);
static struct eccvalue_t lessOrEqual(struct eccstate_t* const, struct eccvalue_t, struct eccvalue_t);
static struct eccvalue_t moreOrEqual(struct eccstate_t* const, struct eccvalue_t, struct eccvalue_t);
static const char* typeName(enum io_libecc_value_Type);
static const char* maskName(enum io_libecc_value_Mask);
static void dumpTo(struct eccvalue_t, FILE*);
const struct type_io_libecc_Value io_libecc_Value = {
    truth,       integer,  binary,    buffer,         key,       text,         chars,       object,      error,    string,      regexp,        number,
    boolean,     date,     function,  host,           reference, isPrimitive,  isBoolean,   isNumber,    isString, isObject,    isDynamic,     isTrue,
    toPrimitive, toBinary, toInteger, binaryToString, toString,  stringLength, stringBytes, textOf,      toObject, objectValue, objectIsArray, toType,
    equals,      same,     add,       subtract,       less,      more,         lessOrEqual, moreOrEqual, typeName, maskName,    dumpTo,
};

struct eccvalue_t truth (int truth)
{
	return (struct eccvalue_t){
		.type = truth? io_libecc_value_trueType: io_libecc_value_falseType,
		.check = 1,
	};
}

struct eccvalue_t integer (int32_t integer)
{
	return (struct eccvalue_t){
		.data = { .integer = integer },
		.type = io_libecc_value_integerType,
		.check = 1,
	};
}

struct eccvalue_t binary (double binary)
{
	return (struct eccvalue_t){
		.data = { .binary = binary },
		.type = io_libecc_value_binaryType,
		.check = 1,
	};
}

struct eccvalue_t buffer (const char b[7], uint8_t units)
{
	struct eccvalue_t value = {
		.type = io_libecc_value_bufferType,
		.check = 1,
	};
	memcpy(value.data.buffer, b, units);
	value.data.buffer[7] = units;
	return value;
}

struct eccvalue_t key (struct io_libecc_Key key)
{
	return (struct eccvalue_t){
		.data = { .key = key },
		.type = io_libecc_value_keyType,
		.check = 0,
	};
}

struct eccvalue_t text (const struct io_libecc_Text *text)
{
	return (struct eccvalue_t){
		.data = { .text = text },
		.type = io_libecc_value_textType,
		.check = 1,
	};
}

struct eccvalue_t chars (struct io_libecc_Chars *chars)
{
	assert(chars);
	
	return (struct eccvalue_t){
		.data = { .chars = chars },
		.type = io_libecc_value_charsType,
		.check = 1,
	};
}

struct eccvalue_t object (struct eccobject_t *object)
{
	assert(object);
	
	return (struct eccvalue_t){
		.data = { .object = object },
		.type = io_libecc_value_objectType,
		.check = 1,
	};
}

struct eccvalue_t error (struct io_libecc_Error *error)
{
	assert(error);
	
	return (struct eccvalue_t){
		.data = { .error = error },
		.type = io_libecc_value_errorType,
		.check = 1,
	};
}

struct eccvalue_t string (struct io_libecc_String *string)
{
	assert(string);
	
	return (struct eccvalue_t){
		.data = { .string = string },
		.type = io_libecc_value_stringType,
		.check = 1,
	};
}

struct eccvalue_t regexp (struct io_libecc_RegExp *regexp)
{
	assert(regexp);
	
	return (struct eccvalue_t){
		.data = { .regexp = regexp },
		.type = io_libecc_value_regexpType,
		.check = 1,
	};
}

struct eccvalue_t number (struct io_libecc_Number *number)
{
	assert(number);
	
	return (struct eccvalue_t){
		.data = { .number = number },
		.type = io_libecc_value_numberType,
		.check = 1,
	};
}

struct eccvalue_t boolean (struct io_libecc_Boolean *boolean)
{
	assert(boolean);
	
	return (struct eccvalue_t){
		.data = { .boolean = boolean },
		.type = io_libecc_value_booleanType,
		.check = 1,
	};
}

struct eccvalue_t date (struct io_libecc_Date *date)
{
	assert(date);
	
	return (struct eccvalue_t){
		.data = { .date = date },
		.type = io_libecc_value_dateType,
		.check = 1,
	};
}

struct eccvalue_t function (struct io_libecc_Function *function)
{
	assert(function);
	
	return (struct eccvalue_t){
		.data = { .function = function },
		.type = io_libecc_value_functionType,
		.check = 1,
	};
}

struct eccvalue_t host (struct eccobject_t *object)
{
	assert(object);
	
	return (struct eccvalue_t){
		.data = { .object = object },
		.type = io_libecc_value_hostType,
		.check = 1,
	};
}

struct eccvalue_t reference (struct eccvalue_t *reference)
{
	assert(reference);
	
	return (struct eccvalue_t){
		.data = { .reference = reference },
		.type = io_libecc_value_referenceType,
		.check = 0,
	};
}

// check

int isPrimitive (struct eccvalue_t value)
{
	return !(value.type & io_libecc_value_objectMask);
}

int isBoolean (struct eccvalue_t value)
{
	return value.type & io_libecc_value_booleanMask;
}

int isNumber (struct eccvalue_t value)
{
	return value.type & io_libecc_value_numberMask;
}

int isString (struct eccvalue_t value)
{
	return value.type & io_libecc_value_stringMask;
}

int isObject (struct eccvalue_t value)
{
	return value.type & io_libecc_value_objectMask;
}

int isDynamic (struct eccvalue_t value)
{
	return value.type & io_libecc_value_dynamicMask;
}

int isTrue (struct eccvalue_t value)
{
	if (value.type <= io_libecc_value_undefinedType)
		return 0;
	if (value.type >= io_libecc_value_trueType)
		return 1;
	else if (value.type == io_libecc_value_integerType)
		return value.data.integer != 0;
	else if (value.type == io_libecc_value_binaryType)
		return !isnan(value.data.binary) && value.data.binary != 0;
	else if (isString(value))
		return stringLength(&value) > 0;
	
	io_libecc_Ecc.fatal("Invalid io_libecc_value_Type : %u", value.type);
}


// convert

struct eccvalue_t toPrimitive (struct eccstate_t * const context, struct eccvalue_t value, enum io_libecc_value_hintPrimitive hint)
{
	struct eccobject_t *object;
	struct io_libecc_Key aKey;
	struct io_libecc_Key bKey;
	struct eccvalue_t aFunction;
	struct eccvalue_t bFunction;
	struct eccvalue_t result;
	struct io_libecc_Text text;
	
	if (value.type < io_libecc_value_objectType)
		return value;
	
	if (!context)
		io_libecc_Ecc.fatal("cannot use toPrimitive outside context");
	
	object = value.data.object;
	hint = hint? hint: value.type == io_libecc_value_dateType? io_libecc_value_hintString: io_libecc_value_hintNumber;
	aKey = hint > 0? io_libecc_key_toString: io_libecc_key_valueOf;
	bKey = hint > 0? io_libecc_key_valueOf: io_libecc_key_toString;
	
	aFunction = io_libecc_Object.getMember(context, object, aKey);
	if (aFunction.type == io_libecc_value_functionType)
	{
		struct eccvalue_t result = io_libecc_Context.callFunction(context, aFunction.data.function, value, 0 | io_libecc_context_asAccessor);
		if (isPrimitive(result))
			return result;
	}
	
	bFunction = io_libecc_Object.getMember(context, object, bKey);
	if (bFunction.type == io_libecc_value_functionType)
	{
		result = io_libecc_Context.callFunction(context, bFunction.data.function, value, 0 | io_libecc_context_asAccessor);
		if (isPrimitive(result))
			return result;
	}
	
	text = io_libecc_Context.textSeek(context);
	if (context->textIndex != io_libecc_context_callIndex && text.length)
		io_libecc_Context.typeError(context, io_libecc_Chars.create("cannot convert '%.*s' to primitive", text.length, text.bytes));
	else
		io_libecc_Context.typeError(context, io_libecc_Chars.create("cannot convert value to primitive"));
}

struct eccvalue_t toBinary (struct eccstate_t * const context, struct eccvalue_t value)
{
	switch ((enum io_libecc_value_Type)value.type)
	{
		case io_libecc_value_binaryType:
			return value;
		
		case io_libecc_value_integerType:
			return binary(value.data.integer);
		
		case io_libecc_value_numberType:
			return binary(value.data.number->value);
		
		case io_libecc_value_nullType:
		case io_libecc_value_falseType:
			return binary(0);
		
		case io_libecc_value_trueType:
			return binary(1);
		
		case io_libecc_value_booleanType:
			return binary(value.data.boolean->truth? 1: 0);
		
		case io_libecc_value_undefinedType:
			return binary(NAN);
		
		case io_libecc_value_textType:
			if (value.data.text == &io_libecc_text_zero)
				return binary(0);
			else if (value.data.text == &io_libecc_text_one)
				return binary(1);
			else if (value.data.text == &io_libecc_text_nan)
				return binary(NAN);
			else if (value.data.text == &io_libecc_text_infinity)
				return binary(INFINITY);
			else if (value.data.text == &io_libecc_text_negativeInfinity)
				return binary(-INFINITY);
			
			/*vvv*/
			
		case io_libecc_value_keyType:
		case io_libecc_value_charsType:
		case io_libecc_value_stringType:
		case io_libecc_value_bufferType:
			return io_libecc_Lexer.scanBinary(textOf(&value), context && context->ecc->sloppyMode? io_libecc_lexer_scanSloppy: 0);
			
		case io_libecc_value_objectType:
		case io_libecc_value_errorType:
		case io_libecc_value_dateType:
		case io_libecc_value_functionType:
		case io_libecc_value_regexpType:
		case io_libecc_value_hostType:
			return toBinary(context, toPrimitive(context, value, io_libecc_value_hintNumber));
		
		case io_libecc_value_referenceType:
			break;
	}
	io_libecc_Ecc.fatal("Invalid io_libecc_value_Type : %u", value.type);
}

struct eccvalue_t toInteger (struct eccstate_t * const context, struct eccvalue_t value)
{
	const double modulus = (double)UINT32_MAX + 1;
	double binary = toBinary(context, value).data.binary;
	
	if (!binary || !isfinite(binary))
		return integer(0);
	
	binary = fmod(binary, modulus);
	if (binary >= 0)
		binary = floor(binary);
	else
		binary = ceil(binary) + modulus;
	
	if (binary > INT32_MAX)
		return integer(binary - modulus);
	
	return integer(binary);
}

struct eccvalue_t binaryToString (double binary, int base)
{
	struct io_libecc_chars_Append chars;
	
	if (binary == 0)
		return text(&io_libecc_text_zero);
	else if (binary == 1)
		return text(&io_libecc_text_one);
	else if (isnan(binary))
		return text(&io_libecc_text_nan);
	else if (isinf(binary))
	{
		if (binary < 0)
			return text(&io_libecc_text_negativeInfinity);
		else
			return text(&io_libecc_text_infinity);
	}
	
	io_libecc_Chars.beginAppend(&chars);
	io_libecc_Chars.appendBinary(&chars, binary, base);
	return io_libecc_Chars.endAppend(&chars);
}

struct eccvalue_t toString (struct eccstate_t * const context, struct eccvalue_t value)
{
	switch ((enum io_libecc_value_Type)value.type)
	{
		case io_libecc_value_textType:
		case io_libecc_value_charsType:
		case io_libecc_value_bufferType:
			return value;
			
		case io_libecc_value_keyType:
			return text(io_libecc_Key.textOf(value.data.key));
		
		case io_libecc_value_stringType:
			return chars(value.data.string->value);
		
		case io_libecc_value_nullType:
			return text(&io_libecc_text_null);
		
		case io_libecc_value_undefinedType:
			return text(&io_libecc_text_undefined);
		
		case io_libecc_value_falseType:
			return text(&io_libecc_text_false);
		
		case io_libecc_value_trueType:
			return text(&io_libecc_text_true);
		
		case io_libecc_value_booleanType:
			return value.data.boolean->truth? text(&io_libecc_text_true): text(&io_libecc_text_false);
		
		case io_libecc_value_integerType:
			return binaryToString(value.data.integer, 10);
		
		case io_libecc_value_numberType:
			value.data.binary = value.data.number->value;
			/*vvv*/
		
		case io_libecc_value_binaryType:
			return binaryToString(value.data.binary, 10);
		
		case io_libecc_value_objectType:
		case io_libecc_value_dateType:
		case io_libecc_value_functionType:
		case io_libecc_value_errorType:
		case io_libecc_value_regexpType:
		case io_libecc_value_hostType:
			return toString(context, toPrimitive(context, value, io_libecc_value_hintString));
		
		case io_libecc_value_referenceType:
			break;
	}
	io_libecc_Ecc.fatal("Invalid io_libecc_value_Type : %u", value.type);
}

int32_t stringLength (const struct eccvalue_t *value)
{
	switch (value->type)
	{
		case io_libecc_value_charsType:
			return value->data.chars->length;
			
		case io_libecc_value_textType:
			return value->data.text->length;
			
		case io_libecc_value_stringType:
			return value->data.string->value->length;
			
		case io_libecc_value_bufferType:
			return value->data.buffer[7];
			
		default:
			return 0;
	}
}

const char * stringBytes (const struct eccvalue_t *value)
{
	switch (value->type)
	{
		case io_libecc_value_charsType:
			return value->data.chars->bytes;
			
		case io_libecc_value_textType:
			return value->data.text->bytes;
			
		case io_libecc_value_stringType:
			return value->data.string->value->bytes;
			
		case io_libecc_value_bufferType:
			return value->data.buffer;
			
		default:
			return NULL;
	}
}

struct io_libecc_Text textOf (const struct eccvalue_t *value)
{
	switch (value->type)
	{
		case io_libecc_value_charsType:
			return io_libecc_Text.make(value->data.chars->bytes, value->data.chars->length);
			
		case io_libecc_value_textType:
			return *value->data.text;
			
		case io_libecc_value_stringType:
			return io_libecc_Text.make(value->data.string->value->bytes, value->data.string->value->length);
			
		case io_libecc_value_keyType:
			return *io_libecc_Key.textOf(value->data.key);
			
		case io_libecc_value_bufferType:
			return io_libecc_Text.make(value->data.buffer, value->data.buffer[7]);
			
		default:
			return io_libecc_text_empty;
	}
}

struct eccvalue_t toObject (struct eccstate_t * const context, struct eccvalue_t value)
{
	if (value.type >= io_libecc_value_objectType)
		return value;
	
	switch ((enum io_libecc_value_Type)value.type)
	{
		case io_libecc_value_binaryType:
			return number(io_libecc_Number.create(value.data.binary));
		
		case io_libecc_value_integerType:
			return number(io_libecc_Number.create(value.data.integer));
		
		case io_libecc_value_textType:
		case io_libecc_value_charsType:
		case io_libecc_value_bufferType:
			return string(io_libecc_String.create(io_libecc_Chars.createWithBytes(stringLength(&value), stringBytes(&value))));
			
		case io_libecc_value_falseType:
		case io_libecc_value_trueType:
			return boolean(io_libecc_Boolean.create(value.type == io_libecc_value_trueType));
		
		case io_libecc_value_nullType:
			goto error;
		
		case io_libecc_value_undefinedType:
			goto error;
		
		case io_libecc_value_keyType:
		case io_libecc_value_referenceType:
		case io_libecc_value_functionType:
		case io_libecc_value_objectType:
		case io_libecc_value_errorType:
		case io_libecc_value_stringType:
		case io_libecc_value_numberType:
		case io_libecc_value_dateType:
		case io_libecc_value_booleanType:
		case io_libecc_value_regexpType:
		case io_libecc_value_hostType:
			break;
	}
	io_libecc_Ecc.fatal("Invalid io_libecc_value_Type : %u", value.type);
	
error:
	{
		struct io_libecc_Text text = io_libecc_Context.textSeek(context);
		
		if (context->textIndex != io_libecc_context_callIndex && text.length)
			io_libecc_Context.typeError(context, io_libecc_Chars.create("cannot convert '%.*s' to object", text.length, text.bytes));
		else
			io_libecc_Context.typeError(context, io_libecc_Chars.create("cannot convert %s to object", typeName(value.type)));
	}
}

struct eccvalue_t objectValue (struct eccobject_t *object)
{
	if (!object)
		return io_libecc_value_undefined;
	else if (object->type == &io_libecc_function_type)
		return io_libecc_Value.function((struct io_libecc_Function *)object);
	else if (object->type == &io_libecc_string_type)
		return io_libecc_Value.string((struct io_libecc_String *)object);
	else if (object->type == &io_libecc_boolean_type)
		return io_libecc_Value.boolean((struct io_libecc_Boolean *)object);
	else if (object->type == &io_libecc_number_type)
		return io_libecc_Value.number((struct io_libecc_Number *)object);
	else if (object->type == &io_libecc_date_type)
		return io_libecc_Value.date((struct io_libecc_Date *)object);
	else if (object->type == &io_libecc_regexp_type)
		return io_libecc_Value.regexp((struct io_libecc_RegExp *)object);
	else if (object->type == &io_libecc_error_type)
		return io_libecc_Value.error((struct io_libecc_Error *)object);
	else if (object->type == &io_libecc_object_type
			|| object->type == &io_libecc_array_type
			|| object->type == &io_libecc_arguments_type
			|| object->type == &io_libecc_math_type
			)
		return io_libecc_Value.object((struct eccobject_t *)object);
	else
		return io_libecc_Value.host(object);
}

int objectIsArray (struct eccobject_t *object)
{
	return object->type == &io_libecc_array_type || object->type == &io_libecc_arguments_type;
}

struct eccvalue_t toType (struct eccvalue_t value)
{
	switch ((enum io_libecc_value_Type)value.type)
	{
		case io_libecc_value_trueType:
		case io_libecc_value_falseType:
			return text(&io_libecc_text_boolean);
		
		case io_libecc_value_undefinedType:
			return text(&io_libecc_text_undefined);
		
		case io_libecc_value_integerType:
		case io_libecc_value_binaryType:
			return text(&io_libecc_text_number);
		
		case io_libecc_value_keyType:
		case io_libecc_value_textType:
		case io_libecc_value_charsType:
		case io_libecc_value_bufferType:
			return text(&io_libecc_text_string);
			
		case io_libecc_value_nullType:
		case io_libecc_value_objectType:
		case io_libecc_value_stringType:
		case io_libecc_value_numberType:
		case io_libecc_value_booleanType:
		case io_libecc_value_errorType:
		case io_libecc_value_dateType:
		case io_libecc_value_regexpType:
		case io_libecc_value_hostType:
			return text(&io_libecc_text_object);
		
		case io_libecc_value_functionType:
			return text(&io_libecc_text_function);
		
		case io_libecc_value_referenceType:
			break;
	}
	io_libecc_Ecc.fatal("Invalid io_libecc_value_Type : %u", value.type);
}

struct eccvalue_t equals (struct eccstate_t * const context, struct eccvalue_t a, struct eccvalue_t b)
{
	if (isObject(a) && isObject(b))
		return truth(a.data.object == b.data.object);
	else if (((isString(a) || isNumber(a)) && isObject(b))
			 || (isObject(a) && (isString(b) || isNumber(b)))
			 )
	{
		a = toPrimitive(context, a, io_libecc_value_hintAuto);
		io_libecc_Context.setTextIndex(context, io_libecc_context_savedIndexAlt);
		b = toPrimitive(context, b, io_libecc_value_hintAuto);
		
		return equals(context, a, b);
	}
	else if (isNumber(a) && isNumber(b))
		return truth(toBinary(context, a).data.binary == toBinary(context, b).data.binary);
	else if (isString(a) && isString(b))
	{
		int32_t aLength = stringLength(&a);
		int32_t bLength = stringLength(&b);
		if (aLength != bLength)
			return io_libecc_value_false;
		
		return truth(!memcmp(stringBytes(&a), stringBytes(&b), aLength));
	}
	else if (a.type == b.type)
		return io_libecc_value_true;
	else if (a.type == io_libecc_value_nullType && b.type == io_libecc_value_undefinedType)
		return io_libecc_value_true;
	else if (a.type == io_libecc_value_undefinedType && b.type == io_libecc_value_nullType)
		return io_libecc_value_true;
	else if (isNumber(a) && isString(b))
		return equals(context, a, toBinary(context, b));
	else if (isString(a) && isNumber(b))
		return equals(context, toBinary(context, a), b);
	else if (isBoolean(a))
		return equals(context, toBinary(context, a), b);
	else if (isBoolean(b))
		return equals(context, a, toBinary(context, b));
	
	return io_libecc_value_false;
}

struct eccvalue_t same (struct eccstate_t * const context, struct eccvalue_t a, struct eccvalue_t b)
{
	if (isObject(a) || isObject(b))
		return truth(isObject(a) && isObject(b) && a.data.object == b.data.object);
	else if (isNumber(a) && isNumber(b))
		return truth(toBinary(context, a).data.binary == toBinary(context, b).data.binary);
	else if (isString(a) && isString(b))
	{
		int32_t aLength = stringLength(&a);
		int32_t bLength = stringLength(&b);
		if (aLength != bLength)
			return io_libecc_value_false;
		
		return truth(!memcmp(stringBytes(&a), stringBytes(&b), aLength));
	}
	else if (a.type == b.type)
		return io_libecc_value_true;
	
	return io_libecc_value_false;
}

struct eccvalue_t add (struct eccstate_t * const context, struct eccvalue_t a, struct eccvalue_t b)
{
	if (!isNumber(a) || !isNumber(b))
	{
		a = toPrimitive(context, a, io_libecc_value_hintAuto);
		io_libecc_Context.setTextIndex(context, io_libecc_context_savedIndexAlt);
		b = toPrimitive(context, b, io_libecc_value_hintAuto);
		
		if (isString(a) || isString(b))
		{
			struct io_libecc_chars_Append chars;
			
			io_libecc_Chars.beginAppend(&chars);
			io_libecc_Chars.appendValue(&chars, context, a);
			io_libecc_Chars.appendValue(&chars, context, b);
			return io_libecc_Chars.endAppend(&chars);
		}
	}
	return binary(toBinary(context, a).data.binary + toBinary(context, b).data.binary);
}

struct eccvalue_t subtract (struct eccstate_t * const context, struct eccvalue_t a, struct eccvalue_t b)
{
	return binary(toBinary(context, a).data.binary - toBinary(context, b).data.binary);
}

static
struct eccvalue_t compare (struct eccstate_t * const context, struct eccvalue_t a, struct eccvalue_t b)
{
	a = toPrimitive(context, a, io_libecc_value_hintNumber);
	io_libecc_Context.setTextIndex(context, io_libecc_context_savedIndexAlt);
	b = toPrimitive(context, b, io_libecc_value_hintNumber);
	
	if (isString(a) && isString(b))
	{
		int32_t aLength = stringLength(&a);
		int32_t bLength = stringLength(&b);
		
		if (aLength < bLength && !memcmp(stringBytes(&a), stringBytes(&b), aLength))
			return io_libecc_value_true;
		
		if (aLength > bLength && !memcmp(stringBytes(&a), stringBytes(&b), bLength))
			return io_libecc_value_false;
		
		return truth(memcmp(stringBytes(&a), stringBytes(&b), aLength) < 0);
	}
	else
	{
		a = toBinary(context, a);
		b = toBinary(context, b);
		
		if (isnan(a.data.binary) || isnan(b.data.binary))
			return io_libecc_value_undefined;
		
		return truth(a.data.binary < b.data.binary);
	}
}

struct eccvalue_t less (struct eccstate_t * const context, struct eccvalue_t a, struct eccvalue_t b)
{
	a = compare(context, a, b);
	if (a.type == io_libecc_value_undefinedType)
		return io_libecc_value_false;
	else
		return a;
}

struct eccvalue_t more (struct eccstate_t * const context, struct eccvalue_t a, struct eccvalue_t b)
{
	a = compare(context, b, a);
	if (a.type == io_libecc_value_undefinedType)
		return io_libecc_value_false;
	else
		return a;
}

struct eccvalue_t lessOrEqual (struct eccstate_t * const context, struct eccvalue_t a, struct eccvalue_t b)
{
	a = compare(context, b, a);
	if (a.type == io_libecc_value_undefinedType || a.type == io_libecc_value_trueType)
		return io_libecc_value_false;
	else
		return io_libecc_value_true;
}

struct eccvalue_t moreOrEqual (struct eccstate_t * const context, struct eccvalue_t a, struct eccvalue_t b)
{
	a = compare(context, a, b);
	if (a.type == io_libecc_value_undefinedType || a.type == io_libecc_value_trueType)
		return io_libecc_value_false;
	else
		return io_libecc_value_true;
}

const char * typeName (enum io_libecc_value_Type type)
{
	switch (type)
	{
		case io_libecc_value_nullType:
			return "null";
		
		case io_libecc_value_undefinedType:
			return "undefined";
		
		case io_libecc_value_falseType:
		case io_libecc_value_trueType:
			return "boolean";
		
		case io_libecc_value_integerType:
		case io_libecc_value_binaryType:
			return "number";
		
		case io_libecc_value_keyType:
		case io_libecc_value_textType:
		case io_libecc_value_charsType:
		case io_libecc_value_bufferType:
			return "string";
			
		case io_libecc_value_objectType:
		case io_libecc_value_hostType:
			return "object";
		
		case io_libecc_value_errorType:
			return "error";
		
		case io_libecc_value_functionType:
			return "function";
		
		case io_libecc_value_dateType:
			return "date";
		
		case io_libecc_value_numberType:
			return "number";
		
		case io_libecc_value_stringType:
			return "string";
		
		case io_libecc_value_booleanType:
			return "boolean";
		
		case io_libecc_value_regexpType:
			return "regexp";
			
		case io_libecc_value_referenceType:
			break;
	}
	io_libecc_Ecc.fatal("Invalid io_libecc_value_Type : %u", type);
}

const char * maskName (enum io_libecc_value_Mask mask)
{
	switch (mask)
	{
		case io_libecc_value_numberMask:
			return "number";
		
		case io_libecc_value_stringMask:
			return "string";
		
		case io_libecc_value_booleanMask:
			return "boolean";
		
		case io_libecc_value_objectMask:
			return "object";
		
		case io_libecc_value_dynamicMask:
			return "dynamic";
	}
	io_libecc_Ecc.fatal("Invalid io_libecc_value_Mask : %u", mask);
}

void dumpTo (struct eccvalue_t value, FILE *file)
{
	switch ((enum io_libecc_value_Type)value.type)
	{
		case io_libecc_value_nullType:
			fputs("null", file);
			return;
		
		case io_libecc_value_undefinedType:
			fputs("undefined", file);
			return;
		
		case io_libecc_value_falseType:
			fputs("false", file);
			return;
		
		case io_libecc_value_trueType:
			fputs("true", file);
			return;
		
		case io_libecc_value_booleanType:
			fputs(value.data.boolean->truth? "true": "false", file);
			return;
		
		case io_libecc_value_integerType:
			fprintf(file, "%d", (int)value.data.integer);
			return;
		
		case io_libecc_value_numberType:
			value.data.binary = value.data.number->value;
			/*vvv*/
			
		case io_libecc_value_binaryType:
			fprintf(file, "%g", value.data.binary);
			return;
		
		case io_libecc_value_keyType:
		case io_libecc_value_textType:
		case io_libecc_value_charsType:
		case io_libecc_value_stringType:
		case io_libecc_value_bufferType:
		{
			const struct io_libecc_Text text = textOf(&value);
			//putc('\'', file);
			fwrite(text.bytes, sizeof(char), text.length, file);
			//putc('\'', file);
			return;
		}
		
		case io_libecc_value_objectType:
		case io_libecc_value_dateType:
		case io_libecc_value_errorType:
		case io_libecc_value_regexpType:
		case io_libecc_value_hostType:
			io_libecc_Object.dumpTo(value.data.object, file);
			return;
		
		case io_libecc_value_functionType:
			fwrite(value.data.function->text.bytes, sizeof(char), value.data.function->text.length, file);
			return;
		
		case io_libecc_value_referenceType:
			fputs("-> ", file);
			dumpTo(*value.data.reference, file);
			return;
	}
}
