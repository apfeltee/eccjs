//
//  math.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"

// MARK: - Private

const eccobjinterntype_t io_libecc_math_type = {
    .text = &ECC_ConstString_MathType,
};

// MARK: - Static Members
static void setup(void);
static void teardown(void);
const struct eccpseudonsmath_t io_libecc_Math = {
    setup,
    teardown,
    {},
};

static eccvalue_t mathAbs(eccstate_t* context)
{
    eccvalue_t value;

    value = ECCNSContext.argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ECCNSValue.toBinary(context, value);

    return ECCNSValue.binary(fabs(value.data.binary));
}

static eccvalue_t mathACos(eccstate_t* context)
{
    eccvalue_t value;

    value = ECCNSContext.argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ECCNSValue.toBinary(context, value);

    return ECCNSValue.binary(acos(value.data.binary));
}

static eccvalue_t mathASin(eccstate_t* context)
{
    eccvalue_t value;

    value = ECCNSContext.argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ECCNSValue.toBinary(context, value);

    return ECCNSValue.binary(asin(value.data.binary));
}

static eccvalue_t mathATan(eccstate_t* context)
{
    eccvalue_t value;

    value = ECCNSContext.argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ECCNSValue.toBinary(context, value);

    return ECCNSValue.binary(atan(value.data.binary));
}

static eccvalue_t mathATan2(eccstate_t* context)
{
    eccvalue_t x, y;

    x = ECCNSContext.argument(context, 0);
    if(x.type != ECC_VALTYPE_BINARY)
        x = ECCNSValue.toBinary(context, x);

    y = ECCNSContext.argument(context, 1);
    if(y.type != ECC_VALTYPE_BINARY)
        y = ECCNSValue.toBinary(context, y);

    return ECCNSValue.binary(atan2(x.data.binary, y.data.binary));
}

static eccvalue_t mathCeil(eccstate_t* context)
{
    eccvalue_t value;

    value = ECCNSContext.argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ECCNSValue.toBinary(context, value);

    return ECCNSValue.binary(ceil(value.data.binary));
}

static eccvalue_t mathCos(eccstate_t* context)
{
    eccvalue_t value;

    value = ECCNSContext.argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ECCNSValue.toBinary(context, value);

    return ECCNSValue.binary(cos(value.data.binary));
}

static eccvalue_t mathExp(eccstate_t* context)
{
    eccvalue_t value;

    value = ECCNSContext.argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ECCNSValue.toBinary(context, value);

    return ECCNSValue.binary(exp(value.data.binary));
}

static eccvalue_t mathFloor(eccstate_t* context)
{
    eccvalue_t value;

    value = ECCNSContext.argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ECCNSValue.toBinary(context, value);

    return ECCNSValue.binary(floor(value.data.binary));
}

static eccvalue_t mathLog(eccstate_t* context)
{
    eccvalue_t value;

    value = ECCNSContext.argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ECCNSValue.toBinary(context, value);

    return ECCNSValue.binary(log(value.data.binary));
}

static eccvalue_t mathMax(eccstate_t* context)
{
    double result = -INFINITY, value;
    int index, count;

    count = ECCNSContext.argumentCount(context);
    if(!count)
        return ECCNSValue.binary(-INFINITY);

    for(index = 0; index < count; ++index)
    {
        value = ECCNSValue.toBinary(context, ECCNSContext.argument(context, index)).data.binary;
        if(isnan(value))
            return ECCNSValue.binary(NAN);

        if(result < value)
            result = value;
    }

    return ECCNSValue.binary(result);
}

static eccvalue_t mathMin(eccstate_t* context)
{
    double result = INFINITY, value;
    int index, count;

    count = ECCNSContext.argumentCount(context);
    if(!count)
        return ECCNSValue.binary(INFINITY);

    for(index = 0; index < count; ++index)
    {
        value = ECCNSValue.toBinary(context, ECCNSContext.argument(context, index)).data.binary;
        if(isnan(value))
            return ECCNSValue.binary(NAN);

        if(result > value)
            result = value;
    }

    return ECCNSValue.binary(result);
}

static eccvalue_t mathPow(eccstate_t* context)
{
    eccvalue_t x, y;

    x = ECCNSValue.toBinary(context, ECCNSContext.argument(context, 0));
    y = ECCNSValue.toBinary(context, ECCNSContext.argument(context, 1));

    if(fabs(x.data.binary) == 1 && !isfinite(y.data.binary))
        return ECCNSValue.binary(NAN);

    return ECCNSValue.binary(pow(x.data.binary, y.data.binary));
}

static eccvalue_t mathRandom(eccstate_t* context)
{
    (void)context;
    return ECCNSValue.binary((double)rand() / (double)RAND_MAX);
}

static eccvalue_t mathRound(eccstate_t* context)
{
    eccvalue_t value;

    value = ECCNSContext.argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ECCNSValue.toBinary(context, value);

    if(value.data.binary < 0)
        return ECCNSValue.binary(1.0 - ceil(0.5 - value.data.binary));
    else
        return ECCNSValue.binary(floor(0.5 + value.data.binary));
}

static eccvalue_t mathSin(eccstate_t* context)
{
    eccvalue_t value;

    value = ECCNSContext.argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ECCNSValue.toBinary(context, value);

    return ECCNSValue.binary(sin(value.data.binary));
}

static eccvalue_t mathSqrt(eccstate_t* context)
{
    eccvalue_t value;

    value = ECCNSContext.argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ECCNSValue.toBinary(context, value);

    return ECCNSValue.binary(sqrt(value.data.binary));
}

static eccvalue_t mathTan(eccstate_t* context)
{
    eccvalue_t value;

    value = ECCNSContext.argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ECCNSValue.toBinary(context, value);

    return ECCNSValue.binary(tan(value.data.binary));
}

// MARK: - Public

eccobject_t* io_libecc_math_object = NULL;

void setup()
{
    const eccvalflag_t r = ECC_VALFLAG_READONLY;
    const eccvalflag_t h = ECC_VALFLAG_HIDDEN;
    const eccvalflag_t s = ECC_VALFLAG_SEALED;

    io_libecc_math_object = ECCNSObject.createTyped(&io_libecc_math_type);

    ECCNSObject.addMember(io_libecc_math_object, io_libecc_Key.makeWithCString("E"), ECCNSValue.binary(2.71828182845904523536), r | h | s);
    ECCNSObject.addMember(io_libecc_math_object, io_libecc_Key.makeWithCString("LN10"), ECCNSValue.binary(2.30258509299404568402), r | h | s);
    ECCNSObject.addMember(io_libecc_math_object, io_libecc_Key.makeWithCString("LN2"), ECCNSValue.binary(0.693147180559945309417), r | h | s);
    ECCNSObject.addMember(io_libecc_math_object, io_libecc_Key.makeWithCString("LOG2E"), ECCNSValue.binary(1.44269504088896340736), r | h | s);
    ECCNSObject.addMember(io_libecc_math_object, io_libecc_Key.makeWithCString("LOG10E"), ECCNSValue.binary(0.434294481903251827651), r | h | s);
    ECCNSObject.addMember(io_libecc_math_object, io_libecc_Key.makeWithCString("PI"), ECCNSValue.binary(3.14159265358979323846), r | h | s);
    ECCNSObject.addMember(io_libecc_math_object, io_libecc_Key.makeWithCString("SQRT1_2"), ECCNSValue.binary(0.707106781186547524401), r | h | s);
    ECCNSObject.addMember(io_libecc_math_object, io_libecc_Key.makeWithCString("SQRT2"), ECCNSValue.binary(1.41421356237309504880), r | h | s);

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

void teardown(void)
{
    io_libecc_math_object = NULL;
}
