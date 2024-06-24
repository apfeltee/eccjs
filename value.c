//
//  value.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"

// MARK: - Private

#define valueMake(T) { .type = T, .check = 1 }

const eccvalue_t ECCValConstNone = {{ 0 }};
const eccvalue_t ECCValConstUndefined = valueMake(ECC_VALTYPE_UNDEFINED);
const eccvalue_t ECCValConstTrue = valueMake(ECC_VALTYPE_TRUE);
const eccvalue_t ECCValConstFalse = valueMake(ECC_VALTYPE_FALSE);
const eccvalue_t ECCValConstNull = valueMake(ECC_VALTYPE_NULL);

// MARK: - Static Members

// MARK: - Methods


// make

static eccvalue_t truth(int truth);
static eccvalue_t integer(int32_t integer);
static eccvalue_t binary(double binary);
static eccvalue_t buffer(const char buffer[7], uint8_t units);
static eccvalue_t key(struct io_libecc_Key key);
static eccvalue_t text(const ecctextstring_t* text);
static eccvalue_t chars(struct io_libecc_Chars* chars);
static eccvalue_t object(eccobject_t*);
static eccvalue_t error(struct io_libecc_Error*);
static eccvalue_t string(struct io_libecc_String*);
static eccvalue_t regexp(struct io_libecc_RegExp*);
static eccvalue_t number(struct io_libecc_Number*);
static eccvalue_t boolean(struct io_libecc_Boolean*);
static eccvalue_t date(struct io_libecc_Date*);
static eccvalue_t function(struct io_libecc_Function*);
static eccvalue_t host(eccobject_t*);
static eccvalue_t reference(eccvalue_t*);
static int isPrimitive(eccvalue_t);
static int isBoolean(eccvalue_t);
static int isNumber(eccvalue_t);
static int isString(eccvalue_t);
static int isObject(eccvalue_t);
static int isDynamic(eccvalue_t);
static int isTrue(eccvalue_t);
static eccvalue_t toPrimitive(eccstate_t* const, eccvalue_t, enum io_libecc_value_hintPrimitive);
static eccvalue_t toBinary(eccstate_t* const, eccvalue_t);
static eccvalue_t toInteger(eccstate_t* const, eccvalue_t);
static eccvalue_t binaryToString(double binary, int base);
static eccvalue_t toString(eccstate_t* const, eccvalue_t);
static int32_t stringLength(const eccvalue_t*);
static const char* stringBytes(const eccvalue_t*);
static ecctextstring_t textOf(const eccvalue_t* string);
static eccvalue_t toObject(eccstate_t* const, eccvalue_t);
static eccvalue_t objectValue(eccobject_t*);
static int objectIsArray(eccobject_t*);
static eccvalue_t toType(eccvalue_t);
static eccvalue_t equals(eccstate_t* const, eccvalue_t, eccvalue_t);
static eccvalue_t same(eccstate_t* const, eccvalue_t, eccvalue_t);
static eccvalue_t add(eccstate_t* const, eccvalue_t, eccvalue_t);
static eccvalue_t subtract(eccstate_t* const, eccvalue_t, eccvalue_t);
static eccvalue_t less(eccstate_t* const, eccvalue_t, eccvalue_t);
static eccvalue_t more(eccstate_t* const, eccvalue_t, eccvalue_t);
static eccvalue_t lessOrEqual(eccstate_t* const, eccvalue_t, eccvalue_t);
static eccvalue_t moreOrEqual(eccstate_t* const, eccvalue_t, eccvalue_t);
static const char* typeName(eccvaltype_t);
static const char* maskName(enum io_libecc_value_Mask);
static void dumpTo(eccvalue_t, FILE*);
const struct type_io_libecc_Value ECCNSValue = {
    truth,       integer,  binary,    buffer,         key,       text,         chars,       object,      error,    string,      regexp,        number,
    boolean,     date,     function,  host,           reference, isPrimitive,  isBoolean,   isNumber,    isString, isObject,    isDynamic,     isTrue,
    toPrimitive, toBinary, toInteger, binaryToString, toString,  stringLength, stringBytes, textOf,      toObject, objectValue, objectIsArray, toType,
    equals,      same,     add,       subtract,       less,      more,         lessOrEqual, moreOrEqual, typeName, maskName,    dumpTo,
};

eccvalue_t truth (int truth)
{
	return (eccvalue_t){
		.type = truth? ECC_VALTYPE_TRUE: ECC_VALTYPE_FALSE,
		.check = 1,
	};
}

eccvalue_t integer (int32_t integer)
{
	return (eccvalue_t){
		.data = { .integer = integer },
		.type = ECC_VALTYPE_INTEGER,
		.check = 1,
	};
}

eccvalue_t binary (double binary)
{
	return (eccvalue_t){
		.data = { .binary = binary },
		.type = ECC_VALTYPE_BINARY,
		.check = 1,
	};
}

eccvalue_t buffer (const char b[7], uint8_t units)
{
	eccvalue_t value = {
		.type = ECC_VALTYPE_BUFFER,
		.check = 1,
	};
	memcpy(value.data.buffer, b, units);
	value.data.buffer[7] = units;
	return value;
}

eccvalue_t key (struct io_libecc_Key key)
{
	return (eccvalue_t){
		.data = { .key = key },
		.type = ECC_VALTYPE_KEY,
		.check = 0,
	};
}

eccvalue_t text (const ecctextstring_t *text)
{
	return (eccvalue_t){
		.data = { .text = text },
		.type = ECC_VALTYPE_TEXT,
		.check = 1,
	};
}

eccvalue_t chars (struct io_libecc_Chars *chars)
{
	assert(chars);
	
	return (eccvalue_t){
		.data = { .chars = chars },
		.type = ECC_VALTYPE_CHARS,
		.check = 1,
	};
}

eccvalue_t object (eccobject_t *object)
{
	assert(object);
	
	return (eccvalue_t){
		.data = { .object = object },
		.type = ECC_VALTYPE_OBJECT,
		.check = 1,
	};
}

eccvalue_t error (struct io_libecc_Error *error)
{
	assert(error);
	
	return (eccvalue_t){
		.data = { .error = error },
		.type = ECC_VALTYPE_ERROR,
		.check = 1,
	};
}

eccvalue_t string (struct io_libecc_String *string)
{
	assert(string);
	
	return (eccvalue_t){
		.data = { .string = string },
		.type = ECC_VALTYPE_STRING,
		.check = 1,
	};
}

eccvalue_t regexp (struct io_libecc_RegExp *regexp)
{
	assert(regexp);
	
	return (eccvalue_t){
		.data = { .regexp = regexp },
		.type = ECC_VALTYPE_REGEXP,
		.check = 1,
	};
}

eccvalue_t number (struct io_libecc_Number *number)
{
	assert(number);
	
	return (eccvalue_t){
		.data = { .number = number },
		.type = ECC_VALTYPE_NUMBER,
		.check = 1,
	};
}

eccvalue_t boolean (struct io_libecc_Boolean *boolean)
{
	assert(boolean);
	
	return (eccvalue_t){
		.data = { .boolean = boolean },
		.type = ECC_VALTYPE_BOOLEAN,
		.check = 1,
	};
}

eccvalue_t date (struct io_libecc_Date *date)
{
	assert(date);
	
	return (eccvalue_t){
		.data = { .date = date },
		.type = ECC_VALTYPE_DATE,
		.check = 1,
	};
}

eccvalue_t function (struct io_libecc_Function *function)
{
	assert(function);
	
	return (eccvalue_t){
		.data = { .function = function },
		.type = ECC_VALTYPE_FUNCTION,
		.check = 1,
	};
}

eccvalue_t host (eccobject_t *object)
{
	assert(object);
	
	return (eccvalue_t){
		.data = { .object = object },
		.type = ECC_VALTYPE_HOST,
		.check = 1,
	};
}

eccvalue_t reference (eccvalue_t *reference)
{
	assert(reference);
	
	return (eccvalue_t){
		.data = { .reference = reference },
		.type = ECC_VALTYPE_REFERENCE,
		.check = 0,
	};
}

// check

int isPrimitive (eccvalue_t value)
{
	return !(value.type & ECC_VALMASK_OBJECT);
}

int isBoolean (eccvalue_t value)
{
	return value.type & ECC_VALMASK_BOOLEAN;
}

int isNumber (eccvalue_t value)
{
	return value.type & ECC_VALMASK_NUMBER;
}

int isString (eccvalue_t value)
{
	return value.type & ECC_VALMASK_STRING;
}

int isObject (eccvalue_t value)
{
	return value.type & ECC_VALMASK_OBJECT;
}

int isDynamic (eccvalue_t value)
{
	return value.type & ECC_VALMASK_DYNAMIC;
}

int isTrue (eccvalue_t value)
{
	if (value.type <= ECC_VALTYPE_UNDEFINED)
		return 0;
	if (value.type >= ECC_VALTYPE_TRUE)
		return 1;
	else if (value.type == ECC_VALTYPE_INTEGER)
		return value.data.integer != 0;
	else if (value.type == ECC_VALTYPE_BINARY)
		return !isnan(value.data.binary) && value.data.binary != 0;
	else if (isString(value))
		return stringLength(&value) > 0;
	
	ECCNSScript.fatal("Invalid io_libecc_value_Type : %u", value.type);
}


// convert

eccvalue_t toPrimitive (eccstate_t * const context, eccvalue_t value, enum io_libecc_value_hintPrimitive hint)
{
	eccobject_t *object;
	struct io_libecc_Key aKey;
	struct io_libecc_Key bKey;
	eccvalue_t aFunction;
	eccvalue_t bFunction;
	eccvalue_t result;
	ecctextstring_t text;
	
	if (value.type < ECC_VALTYPE_OBJECT)
		return value;
	
	if (!context)
		ECCNSScript.fatal("cannot use toPrimitive outside context");
	
	object = value.data.object;
	hint = hint? hint: value.type == ECC_VALTYPE_DATE? io_libecc_value_hintString: io_libecc_value_hintNumber;
	aKey = hint > 0? io_libecc_key_toString: io_libecc_key_valueOf;
	bKey = hint > 0? io_libecc_key_valueOf: io_libecc_key_toString;
	
	aFunction = ECCNSObject.getMember(context, object, aKey);
	if (aFunction.type == ECC_VALTYPE_FUNCTION)
	{
		eccvalue_t result = ECCNSContext.callFunction(context, aFunction.data.function, value, 0 | io_libecc_context_asAccessor);
		if (isPrimitive(result))
			return result;
	}
	
	bFunction = ECCNSObject.getMember(context, object, bKey);
	if (bFunction.type == ECC_VALTYPE_FUNCTION)
	{
		result = ECCNSContext.callFunction(context, bFunction.data.function, value, 0 | io_libecc_context_asAccessor);
		if (isPrimitive(result))
			return result;
	}
	
	text = ECCNSContext.textSeek(context);
	if (context->textIndex != io_libecc_context_callIndex && text.length)
		ECCNSContext.typeError(context, io_libecc_Chars.create("cannot convert '%.*s' to primitive", text.length, text.bytes));
	else
		ECCNSContext.typeError(context, io_libecc_Chars.create("cannot convert value to primitive"));
}

eccvalue_t toBinary (eccstate_t * const context, eccvalue_t value)
{
	switch ((eccvaltype_t)value.type)
	{
		case ECC_VALTYPE_BINARY:
			return value;
		
		case ECC_VALTYPE_INTEGER:
			return binary(value.data.integer);
		
		case ECC_VALTYPE_NUMBER:
			return binary(value.data.number->value);
		
		case ECC_VALTYPE_NULL:
		case ECC_VALTYPE_FALSE:
			return binary(0);
		
		case ECC_VALTYPE_TRUE:
			return binary(1);
		
		case ECC_VALTYPE_BOOLEAN:
			return binary(value.data.boolean->truth? 1: 0);
		
		case ECC_VALTYPE_UNDEFINED:
			return binary(NAN);
		
		case ECC_VALTYPE_TEXT:
			if (value.data.text == &ECC_ConstString_Zero)
				return binary(0);
			else if (value.data.text == &ECC_ConstString_One)
				return binary(1);
			else if (value.data.text == &ECC_ConstString_Nan)
				return binary(NAN);
			else if (value.data.text == &ECC_ConstString_Infinity)
				return binary(INFINITY);
			else if (value.data.text == &ECC_ConstString_NegativeInfinity)
				return binary(-INFINITY);
			
			/*vvv*/
			
		case ECC_VALTYPE_KEY:
		case ECC_VALTYPE_CHARS:
		case ECC_VALTYPE_STRING:
		case ECC_VALTYPE_BUFFER:
			return io_libecc_Lexer.scanBinary(textOf(&value), context && context->ecc->sloppyMode? io_libecc_lexer_scanSloppy: 0);
			
		case ECC_VALTYPE_OBJECT:
		case ECC_VALTYPE_ERROR:
		case ECC_VALTYPE_DATE:
		case ECC_VALTYPE_FUNCTION:
		case ECC_VALTYPE_REGEXP:
		case ECC_VALTYPE_HOST:
			return toBinary(context, toPrimitive(context, value, io_libecc_value_hintNumber));
		
		case ECC_VALTYPE_REFERENCE:
			break;
	}
	ECCNSScript.fatal("Invalid io_libecc_value_Type : %u", value.type);
}

eccvalue_t toInteger (eccstate_t * const context, eccvalue_t value)
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

eccvalue_t binaryToString (double binary, int base)
{
	struct io_libecc_chars_Append chars;
	
	if (binary == 0)
		return text(&ECC_ConstString_Zero);
	else if (binary == 1)
		return text(&ECC_ConstString_One);
	else if (isnan(binary))
		return text(&ECC_ConstString_Nan);
	else if (isinf(binary))
	{
		if (binary < 0)
			return text(&ECC_ConstString_NegativeInfinity);
		else
			return text(&ECC_ConstString_Infinity);
	}
	
	io_libecc_Chars.beginAppend(&chars);
	io_libecc_Chars.appendBinary(&chars, binary, base);
	return io_libecc_Chars.endAppend(&chars);
}

eccvalue_t toString (eccstate_t * const context, eccvalue_t value)
{
	switch ((eccvaltype_t)value.type)
	{
		case ECC_VALTYPE_TEXT:
		case ECC_VALTYPE_CHARS:
		case ECC_VALTYPE_BUFFER:
			return value;
			
		case ECC_VALTYPE_KEY:
			return text(io_libecc_Key.textOf(value.data.key));
		
		case ECC_VALTYPE_STRING:
			return chars(value.data.string->value);
		
		case ECC_VALTYPE_NULL:
			return text(&ECC_ConstString_Null);
		
		case ECC_VALTYPE_UNDEFINED:
			return text(&ECC_ConstString_Undefined);
		
		case ECC_VALTYPE_FALSE:
			return text(&ECC_ConstString_False);
		
		case ECC_VALTYPE_TRUE:
			return text(&ECC_ConstString_True);
		
		case ECC_VALTYPE_BOOLEAN:
			return value.data.boolean->truth? text(&ECC_ConstString_True): text(&ECC_ConstString_False);
		
		case ECC_VALTYPE_INTEGER:
			return binaryToString(value.data.integer, 10);
		
		case ECC_VALTYPE_NUMBER:
			value.data.binary = value.data.number->value;
			/*vvv*/
		
		case ECC_VALTYPE_BINARY:
			return binaryToString(value.data.binary, 10);
		
		case ECC_VALTYPE_OBJECT:
		case ECC_VALTYPE_DATE:
		case ECC_VALTYPE_FUNCTION:
		case ECC_VALTYPE_ERROR:
		case ECC_VALTYPE_REGEXP:
		case ECC_VALTYPE_HOST:
			return toString(context, toPrimitive(context, value, io_libecc_value_hintString));
		
		case ECC_VALTYPE_REFERENCE:
			break;
	}
	ECCNSScript.fatal("Invalid io_libecc_value_Type : %u", value.type);
}

int32_t stringLength (const eccvalue_t *value)
{
	switch (value->type)
	{
		case ECC_VALTYPE_CHARS:
			return value->data.chars->length;
			
		case ECC_VALTYPE_TEXT:
			return value->data.text->length;
			
		case ECC_VALTYPE_STRING:
			return value->data.string->value->length;
			
		case ECC_VALTYPE_BUFFER:
			return value->data.buffer[7];
			
		default:
			return 0;
	}
}

const char * stringBytes (const eccvalue_t *value)
{
	switch (value->type)
	{
		case ECC_VALTYPE_CHARS:
			return value->data.chars->bytes;
			
		case ECC_VALTYPE_TEXT:
			return value->data.text->bytes;
			
		case ECC_VALTYPE_STRING:
			return value->data.string->value->bytes;
			
		case ECC_VALTYPE_BUFFER:
			return value->data.buffer;
			
		default:
			return NULL;
	}
}

ecctextstring_t textOf (const eccvalue_t *value)
{
	switch (value->type)
	{
		case ECC_VALTYPE_CHARS:
			return ECCNSText.make(value->data.chars->bytes, value->data.chars->length);
			
		case ECC_VALTYPE_TEXT:
			return *value->data.text;
			
		case ECC_VALTYPE_STRING:
			return ECCNSText.make(value->data.string->value->bytes, value->data.string->value->length);
			
		case ECC_VALTYPE_KEY:
			return *io_libecc_Key.textOf(value->data.key);
			
		case ECC_VALTYPE_BUFFER:
			return ECCNSText.make(value->data.buffer, value->data.buffer[7]);
			
		default:
			return ECC_ConstString_Empty;
	}
}

eccvalue_t toObject (eccstate_t * const context, eccvalue_t value)
{
	if (value.type >= ECC_VALTYPE_OBJECT)
		return value;
	
	switch ((eccvaltype_t)value.type)
	{
		case ECC_VALTYPE_BINARY:
			return number(io_libecc_Number.create(value.data.binary));
		
		case ECC_VALTYPE_INTEGER:
			return number(io_libecc_Number.create(value.data.integer));
		
		case ECC_VALTYPE_TEXT:
		case ECC_VALTYPE_CHARS:
		case ECC_VALTYPE_BUFFER:
			return string(io_libecc_String.create(io_libecc_Chars.createWithBytes(stringLength(&value), stringBytes(&value))));
			
		case ECC_VALTYPE_FALSE:
		case ECC_VALTYPE_TRUE:
			return boolean(io_libecc_Boolean.create(value.type == ECC_VALTYPE_TRUE));
		
		case ECC_VALTYPE_NULL:
			goto error;
		
		case ECC_VALTYPE_UNDEFINED:
			goto error;
		
		case ECC_VALTYPE_KEY:
		case ECC_VALTYPE_REFERENCE:
		case ECC_VALTYPE_FUNCTION:
		case ECC_VALTYPE_OBJECT:
		case ECC_VALTYPE_ERROR:
		case ECC_VALTYPE_STRING:
		case ECC_VALTYPE_NUMBER:
		case ECC_VALTYPE_DATE:
		case ECC_VALTYPE_BOOLEAN:
		case ECC_VALTYPE_REGEXP:
		case ECC_VALTYPE_HOST:
			break;
	}
	ECCNSScript.fatal("Invalid io_libecc_value_Type : %u", value.type);
	
error:
	{
		ecctextstring_t text = ECCNSContext.textSeek(context);
		
		if (context->textIndex != io_libecc_context_callIndex && text.length)
			ECCNSContext.typeError(context, io_libecc_Chars.create("cannot convert '%.*s' to object", text.length, text.bytes));
		else
			ECCNSContext.typeError(context, io_libecc_Chars.create("cannot convert %s to object", typeName(value.type)));
	}
}

eccvalue_t objectValue (eccobject_t *object)
{
	if (!object)
		return ECCValConstUndefined;
	else if (object->type == &io_libecc_function_type)
		return ECCNSValue.function((struct io_libecc_Function *)object);
	else if (object->type == &io_libecc_string_type)
		return ECCNSValue.string((struct io_libecc_String *)object);
	else if (object->type == &io_libecc_boolean_type)
		return ECCNSValue.boolean((struct io_libecc_Boolean *)object);
	else if (object->type == &io_libecc_number_type)
		return ECCNSValue.number((struct io_libecc_Number *)object);
	else if (object->type == &io_libecc_date_type)
		return ECCNSValue.date((struct io_libecc_Date *)object);
	else if (object->type == &io_libecc_regexp_type)
		return ECCNSValue.regexp((struct io_libecc_RegExp *)object);
	else if (object->type == &io_libecc_error_type)
		return ECCNSValue.error((struct io_libecc_Error *)object);
	else if (object->type == &io_libecc_object_type
			|| object->type == &io_libecc_array_type
			|| object->type == &io_libecc_arguments_type
			|| object->type == &io_libecc_math_type
			)
		return ECCNSValue.object((eccobject_t *)object);
	else
		return ECCNSValue.host(object);
}

int objectIsArray (eccobject_t *object)
{
	return object->type == &io_libecc_array_type || object->type == &io_libecc_arguments_type;
}

eccvalue_t toType (eccvalue_t value)
{
	switch ((eccvaltype_t)value.type)
	{
		case ECC_VALTYPE_TRUE:
		case ECC_VALTYPE_FALSE:
			return text(&ECC_ConstString_Boolean);
		
		case ECC_VALTYPE_UNDEFINED:
			return text(&ECC_ConstString_Undefined);
		
		case ECC_VALTYPE_INTEGER:
		case ECC_VALTYPE_BINARY:
			return text(&ECC_ConstString_Number);
		
		case ECC_VALTYPE_KEY:
		case ECC_VALTYPE_TEXT:
		case ECC_VALTYPE_CHARS:
		case ECC_VALTYPE_BUFFER:
			return text(&ECC_ConstString_String);
			
		case ECC_VALTYPE_NULL:
		case ECC_VALTYPE_OBJECT:
		case ECC_VALTYPE_STRING:
		case ECC_VALTYPE_NUMBER:
		case ECC_VALTYPE_BOOLEAN:
		case ECC_VALTYPE_ERROR:
		case ECC_VALTYPE_DATE:
		case ECC_VALTYPE_REGEXP:
		case ECC_VALTYPE_HOST:
			return text(&ECC_ConstString_Object);
		
		case ECC_VALTYPE_FUNCTION:
			return text(&ECC_ConstString_Function);
		
		case ECC_VALTYPE_REFERENCE:
			break;
	}
	ECCNSScript.fatal("Invalid io_libecc_value_Type : %u", value.type);
}

eccvalue_t equals (eccstate_t * const context, eccvalue_t a, eccvalue_t b)
{
	if (isObject(a) && isObject(b))
		return truth(a.data.object == b.data.object);
	else if (((isString(a) || isNumber(a)) && isObject(b))
			 || (isObject(a) && (isString(b) || isNumber(b)))
			 )
	{
		a = toPrimitive(context, a, io_libecc_value_hintAuto);
		ECCNSContext.setTextIndex(context, io_libecc_context_savedIndexAlt);
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
			return ECCValConstFalse;
		
		return truth(!memcmp(stringBytes(&a), stringBytes(&b), aLength));
	}
	else if (a.type == b.type)
		return ECCValConstTrue;
	else if (a.type == ECC_VALTYPE_NULL && b.type == ECC_VALTYPE_UNDEFINED)
		return ECCValConstTrue;
	else if (a.type == ECC_VALTYPE_UNDEFINED && b.type == ECC_VALTYPE_NULL)
		return ECCValConstTrue;
	else if (isNumber(a) && isString(b))
		return equals(context, a, toBinary(context, b));
	else if (isString(a) && isNumber(b))
		return equals(context, toBinary(context, a), b);
	else if (isBoolean(a))
		return equals(context, toBinary(context, a), b);
	else if (isBoolean(b))
		return equals(context, a, toBinary(context, b));
	
	return ECCValConstFalse;
}

eccvalue_t same (eccstate_t * const context, eccvalue_t a, eccvalue_t b)
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
			return ECCValConstFalse;
		
		return truth(!memcmp(stringBytes(&a), stringBytes(&b), aLength));
	}
	else if (a.type == b.type)
		return ECCValConstTrue;
	
	return ECCValConstFalse;
}

eccvalue_t add (eccstate_t * const context, eccvalue_t a, eccvalue_t b)
{
	if (!isNumber(a) || !isNumber(b))
	{
		a = toPrimitive(context, a, io_libecc_value_hintAuto);
		ECCNSContext.setTextIndex(context, io_libecc_context_savedIndexAlt);
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

eccvalue_t subtract (eccstate_t * const context, eccvalue_t a, eccvalue_t b)
{
	return binary(toBinary(context, a).data.binary - toBinary(context, b).data.binary);
}

static
eccvalue_t compare (eccstate_t * const context, eccvalue_t a, eccvalue_t b)
{
	a = toPrimitive(context, a, io_libecc_value_hintNumber);
	ECCNSContext.setTextIndex(context, io_libecc_context_savedIndexAlt);
	b = toPrimitive(context, b, io_libecc_value_hintNumber);
	
	if (isString(a) && isString(b))
	{
		int32_t aLength = stringLength(&a);
		int32_t bLength = stringLength(&b);
		
		if (aLength < bLength && !memcmp(stringBytes(&a), stringBytes(&b), aLength))
			return ECCValConstTrue;
		
		if (aLength > bLength && !memcmp(stringBytes(&a), stringBytes(&b), bLength))
			return ECCValConstFalse;
		
		return truth(memcmp(stringBytes(&a), stringBytes(&b), aLength) < 0);
	}
	else
	{
		a = toBinary(context, a);
		b = toBinary(context, b);
		
		if (isnan(a.data.binary) || isnan(b.data.binary))
			return ECCValConstUndefined;
		
		return truth(a.data.binary < b.data.binary);
	}
}

eccvalue_t less (eccstate_t * const context, eccvalue_t a, eccvalue_t b)
{
	a = compare(context, a, b);
	if (a.type == ECC_VALTYPE_UNDEFINED)
		return ECCValConstFalse;
	else
		return a;
}

eccvalue_t more (eccstate_t * const context, eccvalue_t a, eccvalue_t b)
{
	a = compare(context, b, a);
	if (a.type == ECC_VALTYPE_UNDEFINED)
		return ECCValConstFalse;
	else
		return a;
}

eccvalue_t lessOrEqual (eccstate_t * const context, eccvalue_t a, eccvalue_t b)
{
	a = compare(context, b, a);
	if (a.type == ECC_VALTYPE_UNDEFINED || a.type == ECC_VALTYPE_TRUE)
		return ECCValConstFalse;
	else
		return ECCValConstTrue;
}

eccvalue_t moreOrEqual (eccstate_t * const context, eccvalue_t a, eccvalue_t b)
{
	a = compare(context, a, b);
	if (a.type == ECC_VALTYPE_UNDEFINED || a.type == ECC_VALTYPE_TRUE)
		return ECCValConstFalse;
	else
		return ECCValConstTrue;
}

const char * typeName (eccvaltype_t type)
{
	switch (type)
	{
		case ECC_VALTYPE_NULL:
			return "null";
		
		case ECC_VALTYPE_UNDEFINED:
			return "undefined";
		
		case ECC_VALTYPE_FALSE:
		case ECC_VALTYPE_TRUE:
			return "boolean";
		
		case ECC_VALTYPE_INTEGER:
		case ECC_VALTYPE_BINARY:
			return "number";
		
		case ECC_VALTYPE_KEY:
		case ECC_VALTYPE_TEXT:
		case ECC_VALTYPE_CHARS:
		case ECC_VALTYPE_BUFFER:
			return "string";
			
		case ECC_VALTYPE_OBJECT:
		case ECC_VALTYPE_HOST:
			return "object";
		
		case ECC_VALTYPE_ERROR:
			return "error";
		
		case ECC_VALTYPE_FUNCTION:
			return "function";
		
		case ECC_VALTYPE_DATE:
			return "date";
		
		case ECC_VALTYPE_NUMBER:
			return "number";
		
		case ECC_VALTYPE_STRING:
			return "string";
		
		case ECC_VALTYPE_BOOLEAN:
			return "boolean";
		
		case ECC_VALTYPE_REGEXP:
			return "regexp";
			
		case ECC_VALTYPE_REFERENCE:
			break;
	}
	ECCNSScript.fatal("Invalid io_libecc_value_Type : %u", type);
}

const char * maskName (enum io_libecc_value_Mask mask)
{
	switch (mask)
	{
		case ECC_VALMASK_NUMBER:
			return "number";
		
		case ECC_VALMASK_STRING:
			return "string";
		
		case ECC_VALMASK_BOOLEAN:
			return "boolean";
		
		case ECC_VALMASK_OBJECT:
			return "object";
		
		case ECC_VALMASK_DYNAMIC:
			return "dynamic";
	}
	ECCNSScript.fatal("Invalid io_libecc_value_Mask : %u", mask);
}

void dumpTo (eccvalue_t value, FILE *file)
{
	switch ((eccvaltype_t)value.type)
	{
		case ECC_VALTYPE_NULL:
			fputs("null", file);
			return;
		
		case ECC_VALTYPE_UNDEFINED:
			fputs("undefined", file);
			return;
		
		case ECC_VALTYPE_FALSE:
			fputs("false", file);
			return;
		
		case ECC_VALTYPE_TRUE:
			fputs("true", file);
			return;
		
		case ECC_VALTYPE_BOOLEAN:
			fputs(value.data.boolean->truth? "true": "false", file);
			return;
		
		case ECC_VALTYPE_INTEGER:
			fprintf(file, "%d", (int)value.data.integer);
			return;
		
		case ECC_VALTYPE_NUMBER:
			value.data.binary = value.data.number->value;
			/*vvv*/
			
		case ECC_VALTYPE_BINARY:
			fprintf(file, "%g", value.data.binary);
			return;
		
		case ECC_VALTYPE_KEY:
		case ECC_VALTYPE_TEXT:
		case ECC_VALTYPE_CHARS:
		case ECC_VALTYPE_STRING:
		case ECC_VALTYPE_BUFFER:
		{
			const ecctextstring_t text = textOf(&value);
			//putc('\'', file);
			fwrite(text.bytes, sizeof(char), text.length, file);
			//putc('\'', file);
			return;
		}
		
		case ECC_VALTYPE_OBJECT:
		case ECC_VALTYPE_DATE:
		case ECC_VALTYPE_ERROR:
		case ECC_VALTYPE_REGEXP:
		case ECC_VALTYPE_HOST:
			ECCNSObject.dumpTo(value.data.object, file);
			return;
		
		case ECC_VALTYPE_FUNCTION:
			fwrite(value.data.function->text.bytes, sizeof(char), value.data.function->text.length, file);
			return;
		
		case ECC_VALTYPE_REFERENCE:
			fputs("-> ", file);
			dumpTo(*value.data.reference, file);
			return;
	}
}
