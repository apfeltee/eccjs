//
//  math.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//

#define Implementation
#include "builtins.h"

// MARK: - Private

const struct io_libecc_object_Type io_libecc_math_type = {
	.text = &io_libecc_text_mathType,
};

// MARK: - Static Members
static void setup(void);
static void teardown(void);
const struct type_io_libecc_Math io_libecc_Math = {
    setup,
    teardown,
};

static
struct io_libecc_Value mathAbs (struct io_libecc_Context * const context)
{
	struct io_libecc_Value value;
	
	value = io_libecc_Context.argument(context, 0);
	if (value.type != io_libecc_value_binaryType)
		value = io_libecc_Value.toBinary(context, value);
	
	return io_libecc_Value.binary(fabs(value.data.binary));
}

static
struct io_libecc_Value mathACos (struct io_libecc_Context * const context)
{
	struct io_libecc_Value value;
	
	value = io_libecc_Context.argument(context, 0);
	if (value.type != io_libecc_value_binaryType)
		value = io_libecc_Value.toBinary(context, value);
	
	return io_libecc_Value.binary(acos(value.data.binary));
}

static
struct io_libecc_Value mathASin (struct io_libecc_Context * const context)
{
	struct io_libecc_Value value;
	
	value = io_libecc_Context.argument(context, 0);
	if (value.type != io_libecc_value_binaryType)
		value = io_libecc_Value.toBinary(context, value);
	
	return io_libecc_Value.binary(asin(value.data.binary));
}

static
struct io_libecc_Value mathATan (struct io_libecc_Context * const context)
{
	struct io_libecc_Value value;
	
	value = io_libecc_Context.argument(context, 0);
	if (value.type != io_libecc_value_binaryType)
		value = io_libecc_Value.toBinary(context, value);
	
	return io_libecc_Value.binary(atan(value.data.binary));
}

static
struct io_libecc_Value mathATan2 (struct io_libecc_Context * const context)
{
	struct io_libecc_Value x, y;
	
	x = io_libecc_Context.argument(context, 0);
	if (x.type != io_libecc_value_binaryType)
		x = io_libecc_Value.toBinary(context, x);
	
	y = io_libecc_Context.argument(context, 1);
	if (y.type != io_libecc_value_binaryType)
		y = io_libecc_Value.toBinary(context, y);
	
	return io_libecc_Value.binary(atan2(x.data.binary, y.data.binary));
}

static
struct io_libecc_Value mathCeil (struct io_libecc_Context * const context)
{
	struct io_libecc_Value value;
	
	value = io_libecc_Context.argument(context, 0);
	if (value.type != io_libecc_value_binaryType)
		value = io_libecc_Value.toBinary(context, value);
	
	return io_libecc_Value.binary(ceil(value.data.binary));
}

static
struct io_libecc_Value mathCos (struct io_libecc_Context * const context)
{
	struct io_libecc_Value value;
	
	value = io_libecc_Context.argument(context, 0);
	if (value.type != io_libecc_value_binaryType)
		value = io_libecc_Value.toBinary(context, value);
	
	return io_libecc_Value.binary(cos(value.data.binary));
}

static
struct io_libecc_Value mathExp (struct io_libecc_Context * const context)
{
	struct io_libecc_Value value;
	
	value = io_libecc_Context.argument(context, 0);
	if (value.type != io_libecc_value_binaryType)
		value = io_libecc_Value.toBinary(context, value);
	
	return io_libecc_Value.binary(exp(value.data.binary));
}

static
struct io_libecc_Value mathFloor (struct io_libecc_Context * const context)
{
	struct io_libecc_Value value;
	
	value = io_libecc_Context.argument(context, 0);
	if (value.type != io_libecc_value_binaryType)
		value = io_libecc_Value.toBinary(context, value);
	
	return io_libecc_Value.binary(floor(value.data.binary));
}

static
struct io_libecc_Value mathLog (struct io_libecc_Context * const context)
{
	struct io_libecc_Value value;
	
	value = io_libecc_Context.argument(context, 0);
	if (value.type != io_libecc_value_binaryType)
		value = io_libecc_Value.toBinary(context, value);
	
	return io_libecc_Value.binary(log(value.data.binary));
}

static
struct io_libecc_Value mathMax (struct io_libecc_Context * const context)
{
	double result = -INFINITY, value;
	int index, count;
	
	count = io_libecc_Context.argumentCount(context);
	if (!count)
		return io_libecc_Value.binary(-INFINITY);
	
	for (index = 0; index < count; ++index)
	{
		value = io_libecc_Value.toBinary(context, io_libecc_Context.argument(context, index)).data.binary;
		if (isnan(value))
			return io_libecc_Value.binary(NAN);
		
		if (result < value)
			result = value;
	}
	
	return io_libecc_Value.binary(result);
}

static
struct io_libecc_Value mathMin (struct io_libecc_Context * const context)
{
	double result = INFINITY, value;
	int index, count;
	
	count = io_libecc_Context.argumentCount(context);
	if (!count)
		return io_libecc_Value.binary(INFINITY);
	
	for (index = 0; index < count; ++index)
	{
		value = io_libecc_Value.toBinary(context, io_libecc_Context.argument(context, index)).data.binary;
		if (isnan(value))
			return io_libecc_Value.binary(NAN);
		
		if (result > value)
			result = value;
	}
	
	return io_libecc_Value.binary(result);
}

static
struct io_libecc_Value mathPow (struct io_libecc_Context * const context)
{
	struct io_libecc_Value x, y;
	
	x = io_libecc_Value.toBinary(context, io_libecc_Context.argument(context, 0));
	y = io_libecc_Value.toBinary(context, io_libecc_Context.argument(context, 1));
	
	if (fabs(x.data.binary) == 1 && !isfinite(y.data.binary))
		return io_libecc_Value.binary(NAN);
	
	return io_libecc_Value.binary(pow(x.data.binary, y.data.binary));
}

static
struct io_libecc_Value mathRandom (struct io_libecc_Context * const context)
{
	return io_libecc_Value.binary((double)rand() / (double)RAND_MAX);
}

static
struct io_libecc_Value mathRound (struct io_libecc_Context * const context)
{
	struct io_libecc_Value value;
	
	value = io_libecc_Context.argument(context, 0);
	if (value.type != io_libecc_value_binaryType)
		value = io_libecc_Value.toBinary(context, value);
	
	if (value.data.binary < 0)
		return io_libecc_Value.binary(1.0 - ceil(0.5 - value.data.binary));
	else
		return io_libecc_Value.binary(floor(0.5 + value.data.binary));
}

static
struct io_libecc_Value mathSin (struct io_libecc_Context * const context)
{
	struct io_libecc_Value value;
	
	value = io_libecc_Context.argument(context, 0);
	if (value.type != io_libecc_value_binaryType)
		value = io_libecc_Value.toBinary(context, value);
	
	return io_libecc_Value.binary(sin(value.data.binary));
}

static
struct io_libecc_Value mathSqrt (struct io_libecc_Context * const context)
{
	struct io_libecc_Value value;
	
	value = io_libecc_Context.argument(context, 0);
	if (value.type != io_libecc_value_binaryType)
		value = io_libecc_Value.toBinary(context, value);
	
	return io_libecc_Value.binary(sqrt(value.data.binary));
}

static
struct io_libecc_Value mathTan (struct io_libecc_Context * const context)
{
	struct io_libecc_Value value;
	
	value = io_libecc_Context.argument(context, 0);
	if (value.type != io_libecc_value_binaryType)
		value = io_libecc_Value.toBinary(context, value);
	
	return io_libecc_Value.binary(tan(value.data.binary));
}

// MARK: - Public

struct io_libecc_Object * io_libecc_math_object = NULL;

void setup ()
{
	const enum io_libecc_value_Flags r = io_libecc_value_readonly;
	const enum io_libecc_value_Flags h = io_libecc_value_hidden;
	const enum io_libecc_value_Flags s = io_libecc_value_sealed;
	
	io_libecc_math_object = io_libecc_Object.createTyped(&io_libecc_math_type);
	
	io_libecc_Object.addMember(io_libecc_math_object, io_libecc_Key.makeWithCString("E"), io_libecc_Value.binary(2.71828182845904523536), r|h|s);
	io_libecc_Object.addMember(io_libecc_math_object, io_libecc_Key.makeWithCString("LN10"), io_libecc_Value.binary(2.30258509299404568402), r|h|s);
	io_libecc_Object.addMember(io_libecc_math_object, io_libecc_Key.makeWithCString("LN2"), io_libecc_Value.binary(0.693147180559945309417), r|h|s);
	io_libecc_Object.addMember(io_libecc_math_object, io_libecc_Key.makeWithCString("LOG2E"), io_libecc_Value.binary(1.44269504088896340736), r|h|s);
	io_libecc_Object.addMember(io_libecc_math_object, io_libecc_Key.makeWithCString("LOG10E"), io_libecc_Value.binary(0.434294481903251827651), r|h|s);
	io_libecc_Object.addMember(io_libecc_math_object, io_libecc_Key.makeWithCString("PI"), io_libecc_Value.binary(3.14159265358979323846), r|h|s);
	io_libecc_Object.addMember(io_libecc_math_object, io_libecc_Key.makeWithCString("SQRT1_2"), io_libecc_Value.binary(0.707106781186547524401), r|h|s);
	io_libecc_Object.addMember(io_libecc_math_object, io_libecc_Key.makeWithCString("SQRT2"), io_libecc_Value.binary(1.41421356237309504880), r|h|s);
	
	io_libecc_Function.addToObject(io_libecc_math_object, "abs", mathAbs, 1, h);
	io_libecc_Function.addToObject(io_libecc_math_object, "acos", mathACos, 1, h);
	io_libecc_Function.addToObject(io_libecc_math_object, "asin", mathASin, 1, h);
	io_libecc_Function.addToObject(io_libecc_math_object, "atan", mathATan, 1, h);
	io_libecc_Function.addToObject(io_libecc_math_object, "atan2", mathATan2, 2, h);
	io_libecc_Function.addToObject(io_libecc_math_object, "ceil", mathCeil, 1, h);
	io_libecc_Function.addToObject(io_libecc_math_object, "cos", mathCos, 1, h);
	io_libecc_Function.addToObject(io_libecc_math_object, "exp", mathExp, 1, h);
	io_libecc_Function.addToObject(io_libecc_math_object, "floor", mathFloor, 1, h);
	io_libecc_Function.addToObject(io_libecc_math_object, "log", mathLog, 1, h);
	io_libecc_Function.addToObject(io_libecc_math_object, "max", mathMax, -2, h);
	io_libecc_Function.addToObject(io_libecc_math_object, "min", mathMin, -2, h);
	io_libecc_Function.addToObject(io_libecc_math_object, "pow", mathPow, 2, h);
	io_libecc_Function.addToObject(io_libecc_math_object, "random", mathRandom, 0, h);
	io_libecc_Function.addToObject(io_libecc_math_object, "round", mathRound, 1, h);
	io_libecc_Function.addToObject(io_libecc_math_object, "sin", mathSin, 1, h);
	io_libecc_Function.addToObject(io_libecc_math_object, "sqrt", mathSqrt, 1, h);
	io_libecc_Function.addToObject(io_libecc_math_object, "tan", mathTan, 1, h);
}

void teardown (void)
{
	io_libecc_math_object = NULL;
}
