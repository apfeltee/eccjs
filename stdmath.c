//
//  math.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"

static eccvalue_t mathobjfn_mathAbs(eccstate_t *context);
static eccvalue_t mathobjfn_mathACos(eccstate_t *context);
static eccvalue_t mathobjfn_mathASin(eccstate_t *context);
static eccvalue_t mathobjfn_mathATan(eccstate_t *context);
static eccvalue_t mathobjfn_mathATan2(eccstate_t *context);
static eccvalue_t mathobjfn_mathCeil(eccstate_t *context);
static eccvalue_t mathobjfn_mathCos(eccstate_t *context);
static eccvalue_t mathobjfn_mathExp(eccstate_t *context);
static eccvalue_t mathobjfn_mathFloor(eccstate_t *context);
static eccvalue_t mathobjfn_mathLog(eccstate_t *context);
static eccvalue_t mathobjfn_mathMax(eccstate_t *context);
static eccvalue_t mathobjfn_mathMin(eccstate_t *context);
static eccvalue_t mathobjfn_mathPow(eccstate_t *context);
static eccvalue_t mathobjfn_mathRandom(eccstate_t *context);
static eccvalue_t mathobjfn_mathRound(eccstate_t *context);
static eccvalue_t mathobjfn_mathSin(eccstate_t *context);
static eccvalue_t mathobjfn_mathSqrt(eccstate_t *context);
static eccvalue_t mathobjfn_mathTan(eccstate_t *context);
static void nsmathfn_setup(void);
static void nsmathfn_teardown(void);


const eccobjinterntype_t ECC_Type_Math = {
    .text = &ECC_ConstString_MathType,
};

const struct eccpseudonsmath_t io_libecc_Math = {
    nsmathfn_setup,
    nsmathfn_teardown,
    {},
};

static eccvalue_t mathobjfn_mathAbs(eccstate_t* context)
{
    eccvalue_t value;

    value = ECCNSContext.argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ecc_value_tobinary(context, value);

    return ecc_value_binary(fabs(value.data.binary));
}

static eccvalue_t mathobjfn_mathACos(eccstate_t* context)
{
    eccvalue_t value;

    value = ECCNSContext.argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ecc_value_tobinary(context, value);

    return ecc_value_binary(acos(value.data.binary));
}

static eccvalue_t mathobjfn_mathASin(eccstate_t* context)
{
    eccvalue_t value;

    value = ECCNSContext.argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ecc_value_tobinary(context, value);

    return ecc_value_binary(asin(value.data.binary));
}

static eccvalue_t mathobjfn_mathATan(eccstate_t* context)
{
    eccvalue_t value;

    value = ECCNSContext.argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ecc_value_tobinary(context, value);

    return ecc_value_binary(atan(value.data.binary));
}

static eccvalue_t mathobjfn_mathATan2(eccstate_t* context)
{
    eccvalue_t x, y;

    x = ECCNSContext.argument(context, 0);
    if(x.type != ECC_VALTYPE_BINARY)
        x = ecc_value_tobinary(context, x);

    y = ECCNSContext.argument(context, 1);
    if(y.type != ECC_VALTYPE_BINARY)
        y = ecc_value_tobinary(context, y);

    return ecc_value_binary(atan2(x.data.binary, y.data.binary));
}

static eccvalue_t mathobjfn_mathCeil(eccstate_t* context)
{
    eccvalue_t value;

    value = ECCNSContext.argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ecc_value_tobinary(context, value);

    return ecc_value_binary(ceil(value.data.binary));
}

static eccvalue_t mathobjfn_mathCos(eccstate_t* context)
{
    eccvalue_t value;

    value = ECCNSContext.argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ecc_value_tobinary(context, value);

    return ecc_value_binary(cos(value.data.binary));
}

static eccvalue_t mathobjfn_mathExp(eccstate_t* context)
{
    eccvalue_t value;

    value = ECCNSContext.argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ecc_value_tobinary(context, value);

    return ecc_value_binary(exp(value.data.binary));
}

static eccvalue_t mathobjfn_mathFloor(eccstate_t* context)
{
    eccvalue_t value;

    value = ECCNSContext.argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ecc_value_tobinary(context, value);

    return ecc_value_binary(floor(value.data.binary));
}

static eccvalue_t mathobjfn_mathLog(eccstate_t* context)
{
    eccvalue_t value;

    value = ECCNSContext.argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ecc_value_tobinary(context, value);

    return ecc_value_binary(log(value.data.binary));
}

static eccvalue_t mathobjfn_mathMax(eccstate_t* context)
{
    double result = -INFINITY, value;
    int index, count;

    count = ECCNSContext.argumentCount(context);
    if(!count)
        return ecc_value_binary(-INFINITY);

    for(index = 0; index < count; ++index)
    {
        value = ecc_value_tobinary(context, ECCNSContext.argument(context, index)).data.binary;
        if(isnan(value))
            return ecc_value_binary(NAN);

        if(result < value)
            result = value;
    }

    return ecc_value_binary(result);
}

static eccvalue_t mathobjfn_mathMin(eccstate_t* context)
{
    double result = INFINITY, value;
    int index, count;

    count = ECCNSContext.argumentCount(context);
    if(!count)
        return ecc_value_binary(INFINITY);

    for(index = 0; index < count; ++index)
    {
        value = ecc_value_tobinary(context, ECCNSContext.argument(context, index)).data.binary;
        if(isnan(value))
            return ecc_value_binary(NAN);

        if(result > value)
            result = value;
    }

    return ecc_value_binary(result);
}

static eccvalue_t mathobjfn_mathPow(eccstate_t* context)
{
    eccvalue_t x, y;

    x = ecc_value_tobinary(context, ECCNSContext.argument(context, 0));
    y = ecc_value_tobinary(context, ECCNSContext.argument(context, 1));

    if(fabs(x.data.binary) == 1 && !isfinite(y.data.binary))
        return ecc_value_binary(NAN);

    return ecc_value_binary(pow(x.data.binary, y.data.binary));
}

static eccvalue_t mathobjfn_mathRandom(eccstate_t* context)
{
    (void)context;
    return ecc_value_binary((double)rand() / (double)RAND_MAX);
}

static eccvalue_t mathobjfn_mathRound(eccstate_t* context)
{
    eccvalue_t value;

    value = ECCNSContext.argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ecc_value_tobinary(context, value);

    if(value.data.binary < 0)
        return ecc_value_binary(1.0 - ceil(0.5 - value.data.binary));
    else
        return ecc_value_binary(floor(0.5 + value.data.binary));
}

static eccvalue_t mathobjfn_mathSin(eccstate_t* context)
{
    eccvalue_t value;

    value = ECCNSContext.argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ecc_value_tobinary(context, value);

    return ecc_value_binary(sin(value.data.binary));
}

static eccvalue_t mathobjfn_mathSqrt(eccstate_t* context)
{
    eccvalue_t value;

    value = ECCNSContext.argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ecc_value_tobinary(context, value);

    return ecc_value_binary(sqrt(value.data.binary));
}

static eccvalue_t mathobjfn_mathTan(eccstate_t* context)
{
    eccvalue_t value;

    value = ECCNSContext.argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ecc_value_tobinary(context, value);

    return ecc_value_binary(tan(value.data.binary));
}

// MARK: - Public

eccobject_t* ECC_Prototype_MathObject = NULL;

static void nsmathfn_setup()
{
    const eccvalflag_t r = ECC_VALFLAG_READONLY;
    const eccvalflag_t h = ECC_VALFLAG_HIDDEN;
    const eccvalflag_t s = ECC_VALFLAG_SEALED;

    ECC_Prototype_MathObject = ECCNSObject.createTyped(&ECC_Type_Math);

    ECCNSObject.addMember(ECC_Prototype_MathObject, ECCNSKey.makeWithCString("E"), ecc_value_binary(2.71828182845904523536), r | h | s);
    ECCNSObject.addMember(ECC_Prototype_MathObject, ECCNSKey.makeWithCString("LN10"), ecc_value_binary(2.30258509299404568402), r | h | s);
    ECCNSObject.addMember(ECC_Prototype_MathObject, ECCNSKey.makeWithCString("LN2"), ecc_value_binary(0.693147180559945309417), r | h | s);
    ECCNSObject.addMember(ECC_Prototype_MathObject, ECCNSKey.makeWithCString("LOG2E"), ecc_value_binary(1.44269504088896340736), r | h | s);
    ECCNSObject.addMember(ECC_Prototype_MathObject, ECCNSKey.makeWithCString("LOG10E"), ecc_value_binary(0.434294481903251827651), r | h | s);
    ECCNSObject.addMember(ECC_Prototype_MathObject, ECCNSKey.makeWithCString("PI"), ecc_value_binary(3.14159265358979323846), r | h | s);
    ECCNSObject.addMember(ECC_Prototype_MathObject, ECCNSKey.makeWithCString("SQRT1_2"), ecc_value_binary(0.707106781186547524401), r | h | s);
    ECCNSObject.addMember(ECC_Prototype_MathObject, ECCNSKey.makeWithCString("SQRT2"), ecc_value_binary(1.41421356237309504880), r | h | s);

    ECCNSFunction.addToObject(ECC_Prototype_MathObject, "abs", mathobjfn_mathAbs, 1, h);
    ECCNSFunction.addToObject(ECC_Prototype_MathObject, "acos", mathobjfn_mathACos, 1, h);
    ECCNSFunction.addToObject(ECC_Prototype_MathObject, "asin", mathobjfn_mathASin, 1, h);
    ECCNSFunction.addToObject(ECC_Prototype_MathObject, "atan", mathobjfn_mathATan, 1, h);
    ECCNSFunction.addToObject(ECC_Prototype_MathObject, "atan2", mathobjfn_mathATan2, 2, h);
    ECCNSFunction.addToObject(ECC_Prototype_MathObject, "ceil", mathobjfn_mathCeil, 1, h);
    ECCNSFunction.addToObject(ECC_Prototype_MathObject, "cos", mathobjfn_mathCos, 1, h);
    ECCNSFunction.addToObject(ECC_Prototype_MathObject, "exp", mathobjfn_mathExp, 1, h);
    ECCNSFunction.addToObject(ECC_Prototype_MathObject, "floor", mathobjfn_mathFloor, 1, h);
    ECCNSFunction.addToObject(ECC_Prototype_MathObject, "log", mathobjfn_mathLog, 1, h);
    ECCNSFunction.addToObject(ECC_Prototype_MathObject, "max", mathobjfn_mathMax, -2, h);
    ECCNSFunction.addToObject(ECC_Prototype_MathObject, "min", mathobjfn_mathMin, -2, h);
    ECCNSFunction.addToObject(ECC_Prototype_MathObject, "pow", mathobjfn_mathPow, 2, h);
    ECCNSFunction.addToObject(ECC_Prototype_MathObject, "random", mathobjfn_mathRandom, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_MathObject, "round", mathobjfn_mathRound, 1, h);
    ECCNSFunction.addToObject(ECC_Prototype_MathObject, "sin", mathobjfn_mathSin, 1, h);
    ECCNSFunction.addToObject(ECC_Prototype_MathObject, "sqrt", mathobjfn_mathSqrt, 1, h);
    ECCNSFunction.addToObject(ECC_Prototype_MathObject, "tan", mathobjfn_mathTan, 1, h);
}

static void nsmathfn_teardown(void)
{
    ECC_Prototype_MathObject = NULL;
}
