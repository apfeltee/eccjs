
/*
//  math.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
*/

#include "ecc.h"
#include "compat.h"

static eccvalue_t ecc_objfnmath_mathabs(ecccontext_t *context);
static eccvalue_t ecc_objfnmath_mathacos(ecccontext_t *context);
static eccvalue_t ecc_objfnmath_mathasin(ecccontext_t *context);
static eccvalue_t ecc_objfnmath_mathatan(ecccontext_t *context);
static eccvalue_t ecc_objfnmath_mathatan2(ecccontext_t *context);
static eccvalue_t ecc_objfnmath_mathceil(ecccontext_t *context);
static eccvalue_t ecc_objfnmath_mathcos(ecccontext_t *context);
static eccvalue_t ecc_objfnmath_mathexp(ecccontext_t *context);
static eccvalue_t ecc_objfnmath_mathfloor(ecccontext_t *context);
static eccvalue_t ecc_objfnmath_mathlog(ecccontext_t *context);
static eccvalue_t ecc_objfnmath_mathmax(ecccontext_t *context);
static eccvalue_t ecc_objfnmath_mathmin(ecccontext_t *context);
static eccvalue_t ecc_objfnmath_mathpow(ecccontext_t *context);
static eccvalue_t ecc_objfnmath_mathrandom(ecccontext_t *context);
static eccvalue_t ecc_objfnmath_mathround(ecccontext_t *context);
static eccvalue_t ecc_objfnmath_mathsin(ecccontext_t *context);
static eccvalue_t ecc_objfnmath_mathsqrt(ecccontext_t *context);
static eccvalue_t ecc_objfnmath_mathtan(ecccontext_t *context);

const eccobjinterntype_t ECC_Type_Math = {
    .text = &ECC_String_MathType,
};

static eccvalue_t ecc_objfnmath_mathabs(ecccontext_t* context)
{
    eccvalue_t value;

    value = ecc_context_argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ecc_value_tobinary(context, value);

    return ecc_value_fromfloat(fabs(value.data.valnumfloat));
}

static eccvalue_t ecc_objfnmath_mathacos(ecccontext_t* context)
{
    eccvalue_t value;

    value = ecc_context_argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ecc_value_tobinary(context, value);

    return ecc_value_fromfloat(acos(value.data.valnumfloat));
}

static eccvalue_t ecc_objfnmath_mathasin(ecccontext_t* context)
{
    eccvalue_t value;

    value = ecc_context_argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ecc_value_tobinary(context, value);

    return ecc_value_fromfloat(asin(value.data.valnumfloat));
}

static eccvalue_t ecc_objfnmath_mathatan(ecccontext_t* context)
{
    eccvalue_t value;

    value = ecc_context_argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ecc_value_tobinary(context, value);

    return ecc_value_fromfloat(atan(value.data.valnumfloat));
}

static eccvalue_t ecc_objfnmath_mathatan2(ecccontext_t* context)
{
    eccvalue_t x, y;

    x = ecc_context_argument(context, 0);
    if(x.type != ECC_VALTYPE_BINARY)
        x = ecc_value_tobinary(context, x);

    y = ecc_context_argument(context, 1);
    if(y.type != ECC_VALTYPE_BINARY)
        y = ecc_value_tobinary(context, y);

    return ecc_value_fromfloat(atan2(x.data.valnumfloat, y.data.valnumfloat));
}

static eccvalue_t ecc_objfnmath_mathceil(ecccontext_t* context)
{
    eccvalue_t value;

    value = ecc_context_argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ecc_value_tobinary(context, value);

    return ecc_value_fromfloat(ceil(value.data.valnumfloat));
}

static eccvalue_t ecc_objfnmath_mathcos(ecccontext_t* context)
{
    eccvalue_t value;

    value = ecc_context_argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ecc_value_tobinary(context, value);

    return ecc_value_fromfloat(cos(value.data.valnumfloat));
}

static eccvalue_t ecc_objfnmath_mathexp(ecccontext_t* context)
{
    eccvalue_t value;

    value = ecc_context_argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ecc_value_tobinary(context, value);

    return ecc_value_fromfloat(exp(value.data.valnumfloat));
}

static eccvalue_t ecc_objfnmath_mathfloor(ecccontext_t* context)
{
    eccvalue_t value;

    value = ecc_context_argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ecc_value_tobinary(context, value);

    return ecc_value_fromfloat(floor(value.data.valnumfloat));
}

static eccvalue_t ecc_objfnmath_mathlog(ecccontext_t* context)
{
    eccvalue_t value;

    value = ecc_context_argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ecc_value_tobinary(context, value);

    return ecc_value_fromfloat(log(value.data.valnumfloat));
}

static eccvalue_t ecc_objfnmath_mathmax(ecccontext_t* context)
{
    double result = -ECC_CONST_INFINITY;
    double value;
    int index, count;

    count = ecc_context_argumentcount(context);
    if(!count)
        return ecc_value_fromfloat(-ECC_CONST_INFINITY);

    for(index = 0; index < count; ++index)
    {
        value = ecc_value_tobinary(context, ecc_context_argument(context, index)).data.valnumfloat;
        if(isnan(value))
            return ecc_value_fromfloat(ECC_CONST_NAN);

        if(result < value)
            result = value;
    }

    return ecc_value_fromfloat(result);
}

static eccvalue_t ecc_objfnmath_mathmin(ecccontext_t* context)
{
    double result = ECC_CONST_INFINITY, value;
    int index, count;

    count = ecc_context_argumentcount(context);
    if(!count)
        return ecc_value_fromfloat(ECC_CONST_INFINITY);

    for(index = 0; index < count; ++index)
    {
        value = ecc_value_tobinary(context, ecc_context_argument(context, index)).data.valnumfloat;
        if(isnan(value))
            return ecc_value_fromfloat(ECC_CONST_NAN);

        if(result > value)
            result = value;
    }

    return ecc_value_fromfloat(result);
}

static eccvalue_t ecc_objfnmath_mathpow(ecccontext_t* context)
{
    eccvalue_t x, y;

    x = ecc_value_tobinary(context, ecc_context_argument(context, 0));
    y = ecc_value_tobinary(context, ecc_context_argument(context, 1));

    if(fabs(x.data.valnumfloat) == 1 && !isfinite(y.data.valnumfloat))
        return ecc_value_fromfloat(ECC_CONST_NAN);

    return ecc_value_fromfloat(pow(x.data.valnumfloat, y.data.valnumfloat));
}

static eccvalue_t ecc_objfnmath_mathrandom(ecccontext_t* context)
{
    (void)context;
    return ecc_value_fromfloat((double)rand() / (double)RAND_MAX);
}

static eccvalue_t ecc_objfnmath_mathround(ecccontext_t* context)
{
    eccvalue_t value;

    value = ecc_context_argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ecc_value_tobinary(context, value);

    if(value.data.valnumfloat < 0)
        return ecc_value_fromfloat(1.0 - ceil(0.5 - value.data.valnumfloat));
    else
        return ecc_value_fromfloat(floor(0.5 + value.data.valnumfloat));
}

static eccvalue_t ecc_objfnmath_mathsin(ecccontext_t* context)
{
    eccvalue_t value;

    value = ecc_context_argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ecc_value_tobinary(context, value);

    return ecc_value_fromfloat(sin(value.data.valnumfloat));
}

static eccvalue_t ecc_objfnmath_mathsqrt(ecccontext_t* context)
{
    eccvalue_t value;

    value = ecc_context_argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ecc_value_tobinary(context, value);

    return ecc_value_fromfloat(sqrt(value.data.valnumfloat));
}

static eccvalue_t ecc_objfnmath_mathtan(ecccontext_t* context)
{
    eccvalue_t value;

    value = ecc_context_argument(context, 0);
    if(value.type != ECC_VALTYPE_BINARY)
        value = ecc_value_tobinary(context, value);

    return ecc_value_fromfloat(tan(value.data.valnumfloat));
}

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

    ecc_function_addto(ECC_Prototype_MathObject, "abs", ecc_objfnmath_mathabs, 1, h);
    ecc_function_addto(ECC_Prototype_MathObject, "acos", ecc_objfnmath_mathacos, 1, h);
    ecc_function_addto(ECC_Prototype_MathObject, "asin", ecc_objfnmath_mathasin, 1, h);
    ecc_function_addto(ECC_Prototype_MathObject, "atan", ecc_objfnmath_mathatan, 1, h);
    ecc_function_addto(ECC_Prototype_MathObject, "atan2", ecc_objfnmath_mathatan2, 2, h);
    ecc_function_addto(ECC_Prototype_MathObject, "ceil", ecc_objfnmath_mathceil, 1, h);
    ecc_function_addto(ECC_Prototype_MathObject, "cos", ecc_objfnmath_mathcos, 1, h);
    ecc_function_addto(ECC_Prototype_MathObject, "exp", ecc_objfnmath_mathexp, 1, h);
    ecc_function_addto(ECC_Prototype_MathObject, "floor", ecc_objfnmath_mathfloor, 1, h);
    ecc_function_addto(ECC_Prototype_MathObject, "log", ecc_objfnmath_mathlog, 1, h);
    ecc_function_addto(ECC_Prototype_MathObject, "max", ecc_objfnmath_mathmax, -2, h);
    ecc_function_addto(ECC_Prototype_MathObject, "min", ecc_objfnmath_mathmin, -2, h);
    ecc_function_addto(ECC_Prototype_MathObject, "pow", ecc_objfnmath_mathpow, 2, h);
    ecc_function_addto(ECC_Prototype_MathObject, "random", ecc_objfnmath_mathrandom, 0, h);
    ecc_function_addto(ECC_Prototype_MathObject, "round", ecc_objfnmath_mathround, 1, h);
    ecc_function_addto(ECC_Prototype_MathObject, "sin", ecc_objfnmath_mathsin, 1, h);
    ecc_function_addto(ECC_Prototype_MathObject, "sqrt", ecc_objfnmath_mathsqrt, 1, h);
    ecc_function_addto(ECC_Prototype_MathObject, "tan", ecc_objfnmath_mathtan, 1, h);
}

void ecc_libmath_teardown(void)
{
    ECC_Prototype_MathObject = NULL;
}
