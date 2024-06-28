//
//  math.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"

static eccvalue_t mathobjfn_mathAbs(ecccontext_t *context);
static eccvalue_t mathobjfn_mathACos(ecccontext_t *context);
static eccvalue_t mathobjfn_mathASin(ecccontext_t *context);
static eccvalue_t mathobjfn_mathATan(ecccontext_t *context);
static eccvalue_t mathobjfn_mathATan2(ecccontext_t *context);
static eccvalue_t mathobjfn_mathCeil(ecccontext_t *context);
static eccvalue_t mathobjfn_mathCos(ecccontext_t *context);
static eccvalue_t mathobjfn_mathExp(ecccontext_t *context);
static eccvalue_t mathobjfn_mathFloor(ecccontext_t *context);
static eccvalue_t mathobjfn_mathLog(ecccontext_t *context);
static eccvalue_t mathobjfn_mathMax(ecccontext_t *context);
static eccvalue_t mathobjfn_mathMin(ecccontext_t *context);
static eccvalue_t mathobjfn_mathPow(ecccontext_t *context);
static eccvalue_t mathobjfn_mathRandom(ecccontext_t *context);
static eccvalue_t mathobjfn_mathRound(ecccontext_t *context);
static eccvalue_t mathobjfn_mathSin(ecccontext_t *context);
static eccvalue_t mathobjfn_mathSqrt(ecccontext_t *context);
static eccvalue_t mathobjfn_mathTan(ecccontext_t *context);

const eccobjinterntype_t ECC_Type_Math = {
    .text = &ECC_ConstString_MathType,
};

static eccvalue_t mathobjfn_mathAbs(ecccontext_t* context)
{
    eccvalue_t value;

    value = ecc_context_argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ecc_value_tobinary(context, value);

    return ecc_value_fromfloat(fabs(value.data.binary));
}

static eccvalue_t mathobjfn_mathACos(ecccontext_t* context)
{
    eccvalue_t value;

    value = ecc_context_argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ecc_value_tobinary(context, value);

    return ecc_value_fromfloat(acos(value.data.binary));
}

static eccvalue_t mathobjfn_mathASin(ecccontext_t* context)
{
    eccvalue_t value;

    value = ecc_context_argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ecc_value_tobinary(context, value);

    return ecc_value_fromfloat(asin(value.data.binary));
}

static eccvalue_t mathobjfn_mathATan(ecccontext_t* context)
{
    eccvalue_t value;

    value = ecc_context_argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ecc_value_tobinary(context, value);

    return ecc_value_fromfloat(atan(value.data.binary));
}

static eccvalue_t mathobjfn_mathATan2(ecccontext_t* context)
{
    eccvalue_t x, y;

    x = ecc_context_argument(context, 0);
    if(x.type != ECC_VALTYPE_BINARY)
        x = ecc_value_tobinary(context, x);

    y = ecc_context_argument(context, 1);
    if(y.type != ECC_VALTYPE_BINARY)
        y = ecc_value_tobinary(context, y);

    return ecc_value_fromfloat(atan2(x.data.binary, y.data.binary));
}

static eccvalue_t mathobjfn_mathCeil(ecccontext_t* context)
{
    eccvalue_t value;

    value = ecc_context_argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ecc_value_tobinary(context, value);

    return ecc_value_fromfloat(ceil(value.data.binary));
}

static eccvalue_t mathobjfn_mathCos(ecccontext_t* context)
{
    eccvalue_t value;

    value = ecc_context_argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ecc_value_tobinary(context, value);

    return ecc_value_fromfloat(cos(value.data.binary));
}

static eccvalue_t mathobjfn_mathExp(ecccontext_t* context)
{
    eccvalue_t value;

    value = ecc_context_argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ecc_value_tobinary(context, value);

    return ecc_value_fromfloat(exp(value.data.binary));
}

static eccvalue_t mathobjfn_mathFloor(ecccontext_t* context)
{
    eccvalue_t value;

    value = ecc_context_argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ecc_value_tobinary(context, value);

    return ecc_value_fromfloat(floor(value.data.binary));
}

static eccvalue_t mathobjfn_mathLog(ecccontext_t* context)
{
    eccvalue_t value;

    value = ecc_context_argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ecc_value_tobinary(context, value);

    return ecc_value_fromfloat(log(value.data.binary));
}

static eccvalue_t mathobjfn_mathMax(ecccontext_t* context)
{
    double result = -INFINITY, value;
    int index, count;

    count = ecc_context_argumentcount(context);
    if(!count)
        return ecc_value_fromfloat(-INFINITY);

    for(index = 0; index < count; ++index)
    {
        value = ecc_value_tobinary(context, ecc_context_argument(context, index)).data.binary;
        if(isnan(value))
            return ecc_value_fromfloat(NAN);

        if(result < value)
            result = value;
    }

    return ecc_value_fromfloat(result);
}

static eccvalue_t mathobjfn_mathMin(ecccontext_t* context)
{
    double result = INFINITY, value;
    int index, count;

    count = ecc_context_argumentcount(context);
    if(!count)
        return ecc_value_fromfloat(INFINITY);

    for(index = 0; index < count; ++index)
    {
        value = ecc_value_tobinary(context, ecc_context_argument(context, index)).data.binary;
        if(isnan(value))
            return ecc_value_fromfloat(NAN);

        if(result > value)
            result = value;
    }

    return ecc_value_fromfloat(result);
}

static eccvalue_t mathobjfn_mathPow(ecccontext_t* context)
{
    eccvalue_t x, y;

    x = ecc_value_tobinary(context, ecc_context_argument(context, 0));
    y = ecc_value_tobinary(context, ecc_context_argument(context, 1));

    if(fabs(x.data.binary) == 1 && !isfinite(y.data.binary))
        return ecc_value_fromfloat(NAN);

    return ecc_value_fromfloat(pow(x.data.binary, y.data.binary));
}

static eccvalue_t mathobjfn_mathRandom(ecccontext_t* context)
{
    (void)context;
    return ecc_value_fromfloat((double)rand() / (double)RAND_MAX);
}

static eccvalue_t mathobjfn_mathRound(ecccontext_t* context)
{
    eccvalue_t value;

    value = ecc_context_argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ecc_value_tobinary(context, value);

    if(value.data.binary < 0)
        return ecc_value_fromfloat(1.0 - ceil(0.5 - value.data.binary));
    else
        return ecc_value_fromfloat(floor(0.5 + value.data.binary));
}

static eccvalue_t mathobjfn_mathSin(ecccontext_t* context)
{
    eccvalue_t value;

    value = ecc_context_argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ecc_value_tobinary(context, value);

    return ecc_value_fromfloat(sin(value.data.binary));
}

static eccvalue_t mathobjfn_mathSqrt(ecccontext_t* context)
{
    eccvalue_t value;

    value = ecc_context_argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ecc_value_tobinary(context, value);

    return ecc_value_fromfloat(sqrt(value.data.binary));
}

static eccvalue_t mathobjfn_mathTan(ecccontext_t* context)
{
    eccvalue_t value;

    value = ecc_context_argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ecc_value_tobinary(context, value);

    return ecc_value_fromfloat(tan(value.data.binary));
}

// MARK: - Public

eccobject_t* ECC_Prototype_MathObject = NULL;

void ecc_libmath_setup()
{
    const eccvalflag_t r = ECC_VALFLAG_READONLY;
    const eccvalflag_t h = ECC_VALFLAG_HIDDEN;
    const eccvalflag_t s = ECC_VALFLAG_SEALED;

    ECC_Prototype_MathObject = ecc_object_createtyped(&ECC_Type_Math);

    ecc_object_addmember(ECC_Prototype_MathObject, ecc_keyidx_makewithcstring("E"), ecc_value_fromfloat(2.71828182845904523536), r | h | s);
    ecc_object_addmember(ECC_Prototype_MathObject, ecc_keyidx_makewithcstring("LN10"), ecc_value_fromfloat(2.30258509299404568402), r | h | s);
    ecc_object_addmember(ECC_Prototype_MathObject, ecc_keyidx_makewithcstring("LN2"), ecc_value_fromfloat(0.693147180559945309417), r | h | s);
    ecc_object_addmember(ECC_Prototype_MathObject, ecc_keyidx_makewithcstring("LOG2E"), ecc_value_fromfloat(1.44269504088896340736), r | h | s);
    ecc_object_addmember(ECC_Prototype_MathObject, ecc_keyidx_makewithcstring("LOG10E"), ecc_value_fromfloat(0.434294481903251827651), r | h | s);
    ecc_object_addmember(ECC_Prototype_MathObject, ecc_keyidx_makewithcstring("PI"), ecc_value_fromfloat(3.14159265358979323846), r | h | s);
    ecc_object_addmember(ECC_Prototype_MathObject, ecc_keyidx_makewithcstring("SQRT1_2"), ecc_value_fromfloat(0.707106781186547524401), r | h | s);
    ecc_object_addmember(ECC_Prototype_MathObject, ecc_keyidx_makewithcstring("SQRT2"), ecc_value_fromfloat(1.41421356237309504880), r | h | s);

    ecc_function_addto(ECC_Prototype_MathObject, "abs", mathobjfn_mathAbs, 1, h);
    ecc_function_addto(ECC_Prototype_MathObject, "acos", mathobjfn_mathACos, 1, h);
    ecc_function_addto(ECC_Prototype_MathObject, "asin", mathobjfn_mathASin, 1, h);
    ecc_function_addto(ECC_Prototype_MathObject, "atan", mathobjfn_mathATan, 1, h);
    ecc_function_addto(ECC_Prototype_MathObject, "atan2", mathobjfn_mathATan2, 2, h);
    ecc_function_addto(ECC_Prototype_MathObject, "ceil", mathobjfn_mathCeil, 1, h);
    ecc_function_addto(ECC_Prototype_MathObject, "cos", mathobjfn_mathCos, 1, h);
    ecc_function_addto(ECC_Prototype_MathObject, "exp", mathobjfn_mathExp, 1, h);
    ecc_function_addto(ECC_Prototype_MathObject, "floor", mathobjfn_mathFloor, 1, h);
    ecc_function_addto(ECC_Prototype_MathObject, "log", mathobjfn_mathLog, 1, h);
    ecc_function_addto(ECC_Prototype_MathObject, "max", mathobjfn_mathMax, -2, h);
    ecc_function_addto(ECC_Prototype_MathObject, "min", mathobjfn_mathMin, -2, h);
    ecc_function_addto(ECC_Prototype_MathObject, "pow", mathobjfn_mathPow, 2, h);
    ecc_function_addto(ECC_Prototype_MathObject, "random", mathobjfn_mathRandom, 0, h);
    ecc_function_addto(ECC_Prototype_MathObject, "round", mathobjfn_mathRound, 1, h);
    ecc_function_addto(ECC_Prototype_MathObject, "sin", mathobjfn_mathSin, 1, h);
    ecc_function_addto(ECC_Prototype_MathObject, "sqrt", mathobjfn_mathSqrt, 1, h);
    ecc_function_addto(ECC_Prototype_MathObject, "tan", mathobjfn_mathTan, 1, h);
}

void ecc_libmath_teardown(void)
{
    ECC_Prototype_MathObject = NULL;
}
