//
//  number.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//

#define Implementation
#include "builtins.h"
#include "ecc.h"
#include "pool.h"

// MARK: - Private

struct io_libecc_Object * io_libecc_number_prototype = NULL;
struct io_libecc_Function * io_libecc_number_constructor = NULL;

const struct io_libecc_object_Type io_libecc_number_type = {
	.text = &io_libecc_text_numberType,
};

// MARK: - Static Members

static void setup(void);
static void teardown(void);
static struct io_libecc_Number* create(double);
const struct type_io_libecc_Number io_libecc_Number = {
    setup,
    teardown,
    create,
};

static
struct io_libecc_Value toExponential (struct io_libecc_Context * const context)
{
	struct io_libecc_chars_Append chars;
	struct io_libecc_Value value;
	double binary, precision = 0;
	
	io_libecc_Context.assertThisMask(context, io_libecc_value_numberMask);
	
	binary = io_libecc_Value.toBinary(context, context->this).data.binary;
	value = io_libecc_Context.argument(context, 0);
	if (value.type != io_libecc_value_undefinedType)
	{
		precision = io_libecc_Value.toBinary(context, value).data.binary;
		if (precision <= -1 || precision >= 21)
			io_libecc_Context.rangeError(context, io_libecc_Chars.create("precision '%.0f' out of range", precision));
		
		if (isnan(precision))
			precision = 0;
	}
	
	if (isnan(binary))
		return io_libecc_Value.text(&io_libecc_text_nan);
	else if (binary == INFINITY)
		return io_libecc_Value.text(&io_libecc_text_infinity);
	else if (binary == -INFINITY)
		return io_libecc_Value.text(&io_libecc_text_negativeInfinity);
	
	io_libecc_Chars.beginAppend(&chars);
	
	if (value.type != io_libecc_value_undefinedType)
		io_libecc_Chars.append(&chars, "%.*e", (int32_t)precision, binary);
	else
		io_libecc_Chars.append(&chars, "%e", binary);
	
	io_libecc_Chars.normalizeBinary(&chars);
	return io_libecc_Chars.endAppend(&chars);
}

static
struct io_libecc_Value toFixed (struct io_libecc_Context * const context)
{
	struct io_libecc_chars_Append chars;
	struct io_libecc_Value value;
	double binary, precision = 0;
	
	io_libecc_Context.assertThisMask(context, io_libecc_value_numberMask);
	
	binary = io_libecc_Value.toBinary(context, context->this).data.binary;
	value = io_libecc_Context.argument(context, 0);
	if (value.type != io_libecc_value_undefinedType)
	{
		precision = io_libecc_Value.toBinary(context, value).data.binary;
		if (precision <= -1 || precision >= 21)
			io_libecc_Context.rangeError(context, io_libecc_Chars.create("precision '%.0f' out of range", precision));
		
		if (isnan(precision))
			precision = 0;
	}
	
	if (isnan(binary))
		return io_libecc_Value.text(&io_libecc_text_nan);
	else if (binary == INFINITY)
		return io_libecc_Value.text(&io_libecc_text_infinity);
	else if (binary == -INFINITY)
		return io_libecc_Value.text(&io_libecc_text_negativeInfinity);
	
	io_libecc_Chars.beginAppend(&chars);
	
	if (binary <= -1e+21 || binary >= 1e+21)
		io_libecc_Chars.appendBinary(&chars, binary, 10);
	else
		io_libecc_Chars.append(&chars, "%.*f", (int32_t)precision, binary);
	
	return io_libecc_Chars.endAppend(&chars);
}

static
struct io_libecc_Value toPrecision (struct io_libecc_Context * const context)
{
	struct io_libecc_chars_Append chars;
	struct io_libecc_Value value;
	double binary, precision = 0;
	
	io_libecc_Context.assertThisMask(context, io_libecc_value_numberMask);
	
	binary = io_libecc_Value.toBinary(context, context->this).data.binary;
	value = io_libecc_Context.argument(context, 0);
	if (value.type != io_libecc_value_undefinedType)
	{
		precision = io_libecc_Value.toBinary(context, value).data.binary;
		if (precision <= -1 || precision >= 101)
			io_libecc_Context.rangeError(context, io_libecc_Chars.create("precision '%.0f' out of range", precision));
		
		if (isnan(precision))
			precision = 0;
	}
	
	if (isnan(binary))
		return io_libecc_Value.text(&io_libecc_text_nan);
	else if (binary == INFINITY)
		return io_libecc_Value.text(&io_libecc_text_infinity);
	else if (binary == -INFINITY)
		return io_libecc_Value.text(&io_libecc_text_negativeInfinity);
	
	io_libecc_Chars.beginAppend(&chars);
	
	if (value.type != io_libecc_value_undefinedType)
	{
		io_libecc_Chars.append(&chars, "%.*g", (int32_t)precision, binary);
		io_libecc_Chars.normalizeBinary(&chars);
	}
	else
		io_libecc_Chars.appendBinary(&chars, binary, 10);
	
	return io_libecc_Chars.endAppend(&chars);
}

static
struct io_libecc_Value toString (struct io_libecc_Context * const context)
{
	struct io_libecc_Value value;
	int32_t radix = 10;
	double binary;
	
	io_libecc_Context.assertThisMask(context, io_libecc_value_numberMask);
	
	binary = io_libecc_Value.toBinary(context, context->this).data.binary;
	value = io_libecc_Context.argument(context, 0);
	if (value.type != io_libecc_value_undefinedType)
	{
		radix = io_libecc_Value.toInteger(context, value).data.integer;
		if (radix < 2 || radix > 36)
			io_libecc_Context.rangeError(context, io_libecc_Chars.create("radix must be an integer at least 2 and no greater than 36"));
		
		if (radix != 10 && (binary < LONG_MIN || binary > LONG_MAX))
			io_libecc_Env.printWarning("%g.toString(%d) out of bounds; only long int are supported by radices other than 10", binary, radix);
	}
	
	return io_libecc_Value.binaryToString(binary, radix);
}

static
struct io_libecc_Value valueOf (struct io_libecc_Context * const context)
{
	io_libecc_Context.assertThisType(context, io_libecc_value_numberType);
	
	return io_libecc_Value.binary(context->this.data.number->value);
}

static
struct io_libecc_Value constructor (struct io_libecc_Context * const context)
{
	struct io_libecc_Value value;
	
	value = io_libecc_Context.argument(context, 0);
	if (value.type == io_libecc_value_undefinedType)
		value = io_libecc_Value.binary(value.check == 1? NAN: 0);
	else
		value = io_libecc_Value.toBinary(context, value);
	
	if (context->construct)
		return io_libecc_Value.number(io_libecc_Number.create(value.data.binary));
	else
		return value;
}

// MARK: - Methods

void setup ()
{
	const enum io_libecc_value_Flags r = io_libecc_value_readonly;
	const enum io_libecc_value_Flags h = io_libecc_value_hidden;
	const enum io_libecc_value_Flags s = io_libecc_value_sealed;
	
	io_libecc_Function.setupBuiltinObject(
		&io_libecc_number_constructor, constructor, 1,
		&io_libecc_number_prototype, io_libecc_Value.number(create(0)),
		&io_libecc_number_type);
	
	io_libecc_Function.addMember(io_libecc_number_constructor, "MAX_VALUE", io_libecc_Value.binary(DBL_MAX), r|h|s);
	io_libecc_Function.addMember(io_libecc_number_constructor, "MIN_VALUE", io_libecc_Value.binary(DBL_MIN * DBL_EPSILON), r|h|s);
	io_libecc_Function.addMember(io_libecc_number_constructor, "NaN", io_libecc_Value.binary(NAN), r|h|s);
	io_libecc_Function.addMember(io_libecc_number_constructor, "NEGATIVE_INFINITY", io_libecc_Value.binary(-INFINITY), r|h|s);
	io_libecc_Function.addMember(io_libecc_number_constructor, "POSITIVE_INFINITY", io_libecc_Value.binary(INFINITY), r|h|s);
	
	io_libecc_Function.addToObject(io_libecc_number_prototype, "toString", toString, 1, h);
	io_libecc_Function.addToObject(io_libecc_number_prototype, "toLocaleString", toString, 1, h);
	io_libecc_Function.addToObject(io_libecc_number_prototype, "valueOf", valueOf, 0, h);
	io_libecc_Function.addToObject(io_libecc_number_prototype, "toFixed", toFixed, 1, h);
	io_libecc_Function.addToObject(io_libecc_number_prototype, "toExponential", toExponential, 1, h);
	io_libecc_Function.addToObject(io_libecc_number_prototype, "toPrecision", toPrecision, 1, h);
}

void teardown (void)
{
	io_libecc_number_prototype = NULL;
	io_libecc_number_constructor = NULL;
}

struct io_libecc_Number * create (double binary)
{
	struct io_libecc_Number *self = malloc(sizeof(*self));
	*self = io_libecc_Number.identity;
	io_libecc_Pool.addObject(&self->object);
	io_libecc_Object.initialize(&self->object, io_libecc_number_prototype);
	
	self->value = binary;
	
	return self;
}
