//
//  number.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"

static eccvalue_t objnumberfn_toExponential(eccstate_t *context);
static eccvalue_t objnumberfn_toFixed(eccstate_t *context);
static eccvalue_t objnumberfn_toPrecision(eccstate_t *context);
static eccvalue_t objnumberfn_toString(eccstate_t *context);
static eccvalue_t objnumberfn_valueOf(eccstate_t *context);
static eccvalue_t objnumberfn_constructor(eccstate_t *context);



eccobject_t* ECC_Prototype_Number = NULL;
eccobjfunction_t* ECC_CtorFunc_Number = NULL;

const eccobjinterntype_t ECC_Type_Number = {
    .text = &ECC_ConstString_NumberType,
};

static eccvalue_t objnumberfn_toExponential(eccstate_t* context)
{
    eccappendbuffer_t chars;
    eccvalue_t value;
    double binary, precision = 0;

    ecc_context_assertthismask(context, ECC_VALMASK_NUMBER);

    binary = ecc_value_tobinary(context, context->thisvalue).data.binary;
    value = ecc_context_argument(context, 0);
    if(value.type != ECC_VALTYPE_UNDEFINED)
    {
        precision = ecc_value_tobinary(context, value).data.binary;
        if(precision <= -1 || precision >= 21)
            ecc_context_rangeerror(context, ecc_charbuf_create("precision '%.0f' out of range", precision));

        if(isnan(precision))
            precision = 0;
    }

    if(isnan(binary))
        return ecc_value_text(&ECC_ConstString_Nan);
    else if(binary == INFINITY)
        return ecc_value_text(&ECC_ConstString_Infinity);
    else if(binary == -INFINITY)
        return ecc_value_text(&ECC_ConstString_NegativeInfinity);

    ecc_charbuf_beginappend(&chars);

    if(value.type != ECC_VALTYPE_UNDEFINED)
        ecc_charbuf_append(&chars, "%.*e", (int32_t)precision, binary);
    else
        ecc_charbuf_append(&chars, "%e", binary);

    ecc_charbuf_normalizebinary(&chars);
    return ecc_charbuf_endappend(&chars);
}

static eccvalue_t objnumberfn_toFixed(eccstate_t* context)
{
    eccappendbuffer_t chars;
    eccvalue_t value;
    double binary, precision = 0;

    ecc_context_assertthismask(context, ECC_VALMASK_NUMBER);

    binary = ecc_value_tobinary(context, context->thisvalue).data.binary;
    value = ecc_context_argument(context, 0);
    if(value.type != ECC_VALTYPE_UNDEFINED)
    {
        precision = ecc_value_tobinary(context, value).data.binary;
        if(precision <= -1 || precision >= 21)
            ecc_context_rangeerror(context, ecc_charbuf_create("precision '%.0f' out of range", precision));

        if(isnan(precision))
            precision = 0;
    }

    if(isnan(binary))
        return ecc_value_text(&ECC_ConstString_Nan);
    else if(binary == INFINITY)
        return ecc_value_text(&ECC_ConstString_Infinity);
    else if(binary == -INFINITY)
        return ecc_value_text(&ECC_ConstString_NegativeInfinity);

    ecc_charbuf_beginappend(&chars);

    if(binary <= -1e+21 || binary >= 1e+21)
        ecc_charbuf_appendbinary(&chars, binary, 10);
    else
        ecc_charbuf_append(&chars, "%.*f", (int32_t)precision, binary);

    return ecc_charbuf_endappend(&chars);
}

static eccvalue_t objnumberfn_toPrecision(eccstate_t* context)
{
    eccappendbuffer_t chars;
    eccvalue_t value;
    double binary, precision = 0;

    ecc_context_assertthismask(context, ECC_VALMASK_NUMBER);

    binary = ecc_value_tobinary(context, context->thisvalue).data.binary;
    value = ecc_context_argument(context, 0);
    if(value.type != ECC_VALTYPE_UNDEFINED)
    {
        precision = ecc_value_tobinary(context, value).data.binary;
        if(precision <= -1 || precision >= 101)
        {
            ecc_context_rangeerror(context, ecc_charbuf_create("precision '%.0f' out of range", precision));
        }
        if(isnan(precision))
        {
            precision = 0;
        }
    }
    if(isnan(binary))
    {
        return ecc_value_text(&ECC_ConstString_Nan);
    }
    else if(binary == INFINITY)
    {
        return ecc_value_text(&ECC_ConstString_Infinity);
    }
    else if(binary == -INFINITY)
    {
        return ecc_value_text(&ECC_ConstString_NegativeInfinity);
    }
    ecc_charbuf_beginappend(&chars);

    if(value.type != ECC_VALTYPE_UNDEFINED)
    {
        ecc_charbuf_append(&chars, "%.*g", (int32_t)precision, binary);
        ecc_charbuf_normalizebinary(&chars);
    }
    else
        ecc_charbuf_appendbinary(&chars, binary, 10);

    return ecc_charbuf_endappend(&chars);
}

static eccvalue_t objnumberfn_toString(eccstate_t* context)
{
    eccvalue_t value;
    int32_t radix = 10;
    double binary;
    ecc_context_assertthismask(context, ECC_VALMASK_NUMBER);
    binary = ecc_value_tobinary(context, context->thisvalue).data.binary;
    value = ecc_context_argument(context, 0);
    if(value.type != ECC_VALTYPE_UNDEFINED)
    {
        radix = ecc_value_tointeger(context, value).data.integer;
        if(radix < 2 || radix > 36)
        {
            ecc_context_rangeerror(context, ecc_charbuf_create("radix must be an integer at least 2 and no greater than 36"));
        }
        if(radix != 10 && ((binary < (double)LONG_MIN) || (binary > (double)LONG_MAX)))
        {
            ecc_env_printwarning("%g.toString(%d) out of bounds; only long int are supported by radices other than 10", binary, radix);
        }
    }
    return ecc_value_binarytostring(binary, radix);
}

static eccvalue_t objnumberfn_valueOf(eccstate_t* context)
{
    ecc_context_assertthistype(context, ECC_VALTYPE_NUMBER);

    return ecc_value_binary(context->thisvalue.data.number->value);
}

static eccvalue_t objnumberfn_constructor(eccstate_t* context)
{
    eccvalue_t value;

    value = ecc_context_argument(context, 0);
    if(value.type == ECC_VALTYPE_UNDEFINED)
        value = ecc_value_binary(value.check == 1 ? NAN : 0);
    else
        value = ecc_value_tobinary(context, value);

    if(context->construct)
        return ecc_value_number(ecc_number_create(value.data.binary));
    else
        return value;
}

// MARK: - Methods

void ecc_number_setup()
{
    const eccvalflag_t r = ECC_VALFLAG_READONLY;
    const eccvalflag_t h = ECC_VALFLAG_HIDDEN;
    const eccvalflag_t s = ECC_VALFLAG_SEALED;

    ecc_function_setupbuiltinobject(&ECC_CtorFunc_Number, objnumberfn_constructor, 1, &ECC_Prototype_Number, ecc_value_number(ecc_number_create(0)), &ECC_Type_Number);

    ecc_function_addmember(ECC_CtorFunc_Number, "MAX_VALUE", ecc_value_binary(DBL_MAX), r | h | s);
    ecc_function_addmember(ECC_CtorFunc_Number, "MIN_VALUE", ecc_value_binary(DBL_MIN * DBL_EPSILON), r | h | s);
    ecc_function_addmember(ECC_CtorFunc_Number, "NaN", ecc_value_binary(NAN), r | h | s);
    ecc_function_addmember(ECC_CtorFunc_Number, "NEGATIVE_INFINITY", ecc_value_binary(-INFINITY), r | h | s);
    ecc_function_addmember(ECC_CtorFunc_Number, "POSITIVE_INFINITY", ecc_value_binary(INFINITY), r | h | s);

    ecc_function_addto(ECC_Prototype_Number, "toString", objnumberfn_toString, 1, h);
    ecc_function_addto(ECC_Prototype_Number, "toLocaleString", objnumberfn_toString, 1, h);
    ecc_function_addto(ECC_Prototype_Number, "valueOf", objnumberfn_valueOf, 0, h);
    ecc_function_addto(ECC_Prototype_Number, "toFixed", objnumberfn_toFixed, 1, h);
    ecc_function_addto(ECC_Prototype_Number, "toExponential", objnumberfn_toExponential, 1, h);
    ecc_function_addto(ECC_Prototype_Number, "toPrecision", objnumberfn_toPrecision, 1, h);
}

void ecc_number_teardown(void)
{
    ECC_Prototype_Number = NULL;
    ECC_CtorFunc_Number = NULL;
}

eccobjnumber_t* ecc_number_create(double binary)
{
    eccobjnumber_t* self;
    self = (eccobjnumber_t*)malloc(sizeof(*self));
    memset(self, 0, sizeof(eccobjnumber_t));
    ecc_mempool_addobject(&self->object);
    ecc_object_initialize(&self->object, ECC_Prototype_Number);
    self->value = binary;
    return self;
}
