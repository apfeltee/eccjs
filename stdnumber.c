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
static void nsnumberfn_setup(void);
static void nsnumberfn_teardown(void);
static eccobjnumber_t *nsnumberfn_create(double binary);


eccobject_t* ECC_Prototype_Number = NULL;
eccobjfunction_t* ECC_CtorFunc_Number = NULL;

const eccobjinterntype_t ECC_Type_Number = {
    .text = &ECC_ConstString_NumberType,
};


const struct eccpseudonsnumber_t ECCNSNumber = {
    nsnumberfn_setup,
    nsnumberfn_teardown,
    nsnumberfn_create,
    {}
};

static eccvalue_t objnumberfn_toExponential(eccstate_t* context)
{
    eccappendbuffer_t chars;
    eccvalue_t value;
    double binary, precision = 0;

    ECCNSContext.assertThisMask(context, ECC_VALMASK_NUMBER);

    binary = ECCNSValue.toBinary(context, context->thisvalue).data.binary;
    value = ECCNSContext.argument(context, 0);
    if(value.type != ECC_VALTYPE_UNDEFINED)
    {
        precision = ECCNSValue.toBinary(context, value).data.binary;
        if(precision <= -1 || precision >= 21)
            ECCNSContext.rangeError(context, ECCNSChars.create("precision '%.0f' out of range", precision));

        if(isnan(precision))
            precision = 0;
    }

    if(isnan(binary))
        return ECCNSValue.text(&ECC_ConstString_Nan);
    else if(binary == INFINITY)
        return ECCNSValue.text(&ECC_ConstString_Infinity);
    else if(binary == -INFINITY)
        return ECCNSValue.text(&ECC_ConstString_NegativeInfinity);

    ECCNSChars.beginAppend(&chars);

    if(value.type != ECC_VALTYPE_UNDEFINED)
        ECCNSChars.append(&chars, "%.*e", (int32_t)precision, binary);
    else
        ECCNSChars.append(&chars, "%e", binary);

    ECCNSChars.normalizeBinary(&chars);
    return ECCNSChars.endAppend(&chars);
}

static eccvalue_t objnumberfn_toFixed(eccstate_t* context)
{
    eccappendbuffer_t chars;
    eccvalue_t value;
    double binary, precision = 0;

    ECCNSContext.assertThisMask(context, ECC_VALMASK_NUMBER);

    binary = ECCNSValue.toBinary(context, context->thisvalue).data.binary;
    value = ECCNSContext.argument(context, 0);
    if(value.type != ECC_VALTYPE_UNDEFINED)
    {
        precision = ECCNSValue.toBinary(context, value).data.binary;
        if(precision <= -1 || precision >= 21)
            ECCNSContext.rangeError(context, ECCNSChars.create("precision '%.0f' out of range", precision));

        if(isnan(precision))
            precision = 0;
    }

    if(isnan(binary))
        return ECCNSValue.text(&ECC_ConstString_Nan);
    else if(binary == INFINITY)
        return ECCNSValue.text(&ECC_ConstString_Infinity);
    else if(binary == -INFINITY)
        return ECCNSValue.text(&ECC_ConstString_NegativeInfinity);

    ECCNSChars.beginAppend(&chars);

    if(binary <= -1e+21 || binary >= 1e+21)
        ECCNSChars.appendBinary(&chars, binary, 10);
    else
        ECCNSChars.append(&chars, "%.*f", (int32_t)precision, binary);

    return ECCNSChars.endAppend(&chars);
}

static eccvalue_t objnumberfn_toPrecision(eccstate_t* context)
{
    eccappendbuffer_t chars;
    eccvalue_t value;
    double binary, precision = 0;

    ECCNSContext.assertThisMask(context, ECC_VALMASK_NUMBER);

    binary = ECCNSValue.toBinary(context, context->thisvalue).data.binary;
    value = ECCNSContext.argument(context, 0);
    if(value.type != ECC_VALTYPE_UNDEFINED)
    {
        precision = ECCNSValue.toBinary(context, value).data.binary;
        if(precision <= -1 || precision >= 101)
            ECCNSContext.rangeError(context, ECCNSChars.create("precision '%.0f' out of range", precision));

        if(isnan(precision))
            precision = 0;
    }

    if(isnan(binary))
        return ECCNSValue.text(&ECC_ConstString_Nan);
    else if(binary == INFINITY)
        return ECCNSValue.text(&ECC_ConstString_Infinity);
    else if(binary == -INFINITY)
        return ECCNSValue.text(&ECC_ConstString_NegativeInfinity);

    ECCNSChars.beginAppend(&chars);

    if(value.type != ECC_VALTYPE_UNDEFINED)
    {
        ECCNSChars.append(&chars, "%.*g", (int32_t)precision, binary);
        ECCNSChars.normalizeBinary(&chars);
    }
    else
        ECCNSChars.appendBinary(&chars, binary, 10);

    return ECCNSChars.endAppend(&chars);
}

static eccvalue_t objnumberfn_toString(eccstate_t* context)
{
    eccvalue_t value;
    int32_t radix = 10;
    double binary;

    ECCNSContext.assertThisMask(context, ECC_VALMASK_NUMBER);

    binary = ECCNSValue.toBinary(context, context->thisvalue).data.binary;
    value = ECCNSContext.argument(context, 0);
    if(value.type != ECC_VALTYPE_UNDEFINED)
    {
        radix = ECCNSValue.toInteger(context, value).data.integer;
        if(radix < 2 || radix > 36)
            ECCNSContext.rangeError(context, ECCNSChars.create("radix must be an integer at least 2 and no greater than 36"));

        if(radix != 10 && (binary < LONG_MIN || binary > LONG_MAX))
            ECCNSEnv.printWarning("%g.toString(%d) out of bounds; only long int are supported by radices other than 10", binary, radix);
    }

    return ECCNSValue.binaryToString(binary, radix);
}

static eccvalue_t objnumberfn_valueOf(eccstate_t* context)
{
    ECCNSContext.assertThisType(context, ECC_VALTYPE_NUMBER);

    return ECCNSValue.binary(context->thisvalue.data.number->value);
}

static eccvalue_t objnumberfn_constructor(eccstate_t* context)
{
    eccvalue_t value;

    value = ECCNSContext.argument(context, 0);
    if(value.type == ECC_VALTYPE_UNDEFINED)
        value = ECCNSValue.binary(value.check == 1 ? NAN : 0);
    else
        value = ECCNSValue.toBinary(context, value);

    if(context->construct)
        return ECCNSValue.number(ECCNSNumber.create(value.data.binary));
    else
        return value;
}

// MARK: - Methods

static void nsnumberfn_setup()
{
    const eccvalflag_t r = ECC_VALFLAG_READONLY;
    const eccvalflag_t h = ECC_VALFLAG_HIDDEN;
    const eccvalflag_t s = ECC_VALFLAG_SEALED;

    ECCNSFunction.setupBuiltinObject(&ECC_CtorFunc_Number, objnumberfn_constructor, 1, &ECC_Prototype_Number, ECCNSValue.number(nsnumberfn_create(0)), &ECC_Type_Number);

    ECCNSFunction.addMember(ECC_CtorFunc_Number, "MAX_VALUE", ECCNSValue.binary(DBL_MAX), r | h | s);
    ECCNSFunction.addMember(ECC_CtorFunc_Number, "MIN_VALUE", ECCNSValue.binary(DBL_MIN * DBL_EPSILON), r | h | s);
    ECCNSFunction.addMember(ECC_CtorFunc_Number, "NaN", ECCNSValue.binary(NAN), r | h | s);
    ECCNSFunction.addMember(ECC_CtorFunc_Number, "NEGATIVE_INFINITY", ECCNSValue.binary(-INFINITY), r | h | s);
    ECCNSFunction.addMember(ECC_CtorFunc_Number, "POSITIVE_INFINITY", ECCNSValue.binary(INFINITY), r | h | s);

    ECCNSFunction.addToObject(ECC_Prototype_Number, "toString", objnumberfn_toString, 1, h);
    ECCNSFunction.addToObject(ECC_Prototype_Number, "toLocaleString", objnumberfn_toString, 1, h);
    ECCNSFunction.addToObject(ECC_Prototype_Number, "valueOf", objnumberfn_valueOf, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_Number, "toFixed", objnumberfn_toFixed, 1, h);
    ECCNSFunction.addToObject(ECC_Prototype_Number, "toExponential", objnumberfn_toExponential, 1, h);
    ECCNSFunction.addToObject(ECC_Prototype_Number, "toPrecision", objnumberfn_toPrecision, 1, h);
}

static void nsnumberfn_teardown(void)
{
    ECC_Prototype_Number = NULL;
    ECC_CtorFunc_Number = NULL;
}

static eccobjnumber_t* nsnumberfn_create(double binary)
{
    eccobjnumber_t* self = malloc(sizeof(*self));
    *self = ECCNSNumber.identity;
    ECCNSMemoryPool.addObject(&self->object);
    ECCNSObject.initialize(&self->object, ECC_Prototype_Number);

    self->value = binary;

    return self;
}
