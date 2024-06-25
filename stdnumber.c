//
//  number.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"

// MARK: - Private

eccobject_t* io_libecc_number_prototype = NULL;
eccobjscriptfunction_t* io_libecc_number_constructor = NULL;

const eccobjinterntype_t io_libecc_number_type = {
    .text = &ECC_ConstString_NumberType,
};

// MARK: - Static Members

static void setup(void);
static void teardown(void);
static eccobjnumber_t* create(double);
const struct eccpseudonsnumber_t io_libecc_Number = {
    setup,
    teardown,
    create,
    {}
};

static eccvalue_t toExponential(eccstate_t* context)
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
            ECCNSContext.rangeError(context, io_libecc_Chars.create("precision '%.0f' out of range", precision));

        if(isnan(precision))
            precision = 0;
    }

    if(isnan(binary))
        return ECCNSValue.text(&ECC_ConstString_Nan);
    else if(binary == INFINITY)
        return ECCNSValue.text(&ECC_ConstString_Infinity);
    else if(binary == -INFINITY)
        return ECCNSValue.text(&ECC_ConstString_NegativeInfinity);

    io_libecc_Chars.beginAppend(&chars);

    if(value.type != ECC_VALTYPE_UNDEFINED)
        io_libecc_Chars.append(&chars, "%.*e", (int32_t)precision, binary);
    else
        io_libecc_Chars.append(&chars, "%e", binary);

    io_libecc_Chars.normalizeBinary(&chars);
    return io_libecc_Chars.endAppend(&chars);
}

static eccvalue_t toFixed(eccstate_t* context)
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
            ECCNSContext.rangeError(context, io_libecc_Chars.create("precision '%.0f' out of range", precision));

        if(isnan(precision))
            precision = 0;
    }

    if(isnan(binary))
        return ECCNSValue.text(&ECC_ConstString_Nan);
    else if(binary == INFINITY)
        return ECCNSValue.text(&ECC_ConstString_Infinity);
    else if(binary == -INFINITY)
        return ECCNSValue.text(&ECC_ConstString_NegativeInfinity);

    io_libecc_Chars.beginAppend(&chars);

    if(binary <= -1e+21 || binary >= 1e+21)
        io_libecc_Chars.appendBinary(&chars, binary, 10);
    else
        io_libecc_Chars.append(&chars, "%.*f", (int32_t)precision, binary);

    return io_libecc_Chars.endAppend(&chars);
}

static eccvalue_t toPrecision(eccstate_t* context)
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
            ECCNSContext.rangeError(context, io_libecc_Chars.create("precision '%.0f' out of range", precision));

        if(isnan(precision))
            precision = 0;
    }

    if(isnan(binary))
        return ECCNSValue.text(&ECC_ConstString_Nan);
    else if(binary == INFINITY)
        return ECCNSValue.text(&ECC_ConstString_Infinity);
    else if(binary == -INFINITY)
        return ECCNSValue.text(&ECC_ConstString_NegativeInfinity);

    io_libecc_Chars.beginAppend(&chars);

    if(value.type != ECC_VALTYPE_UNDEFINED)
    {
        io_libecc_Chars.append(&chars, "%.*g", (int32_t)precision, binary);
        io_libecc_Chars.normalizeBinary(&chars);
    }
    else
        io_libecc_Chars.appendBinary(&chars, binary, 10);

    return io_libecc_Chars.endAppend(&chars);
}

static eccvalue_t toString(eccstate_t* context)
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
            ECCNSContext.rangeError(context, io_libecc_Chars.create("radix must be an integer at least 2 and no greater than 36"));

        if(radix != 10 && (binary < LONG_MIN || binary > LONG_MAX))
            ECCNSEnv.printWarning("%g.toString(%d) out of bounds; only long int are supported by radices other than 10", binary, radix);
    }

    return ECCNSValue.binaryToString(binary, radix);
}

static eccvalue_t valueOf(eccstate_t* context)
{
    ECCNSContext.assertThisType(context, ECC_VALTYPE_NUMBER);

    return ECCNSValue.binary(context->thisvalue.data.number->value);
}

static eccvalue_t constructor(eccstate_t* context)
{
    eccvalue_t value;

    value = ECCNSContext.argument(context, 0);
    if(value.type == ECC_VALTYPE_UNDEFINED)
        value = ECCNSValue.binary(value.check == 1 ? NAN : 0);
    else
        value = ECCNSValue.toBinary(context, value);

    if(context->construct)
        return ECCNSValue.number(io_libecc_Number.create(value.data.binary));
    else
        return value;
}

// MARK: - Methods

void setup()
{
    const eccvalflag_t r = ECC_VALFLAG_READONLY;
    const eccvalflag_t h = ECC_VALFLAG_HIDDEN;
    const eccvalflag_t s = ECC_VALFLAG_SEALED;

    io_libecc_Function.setupBuiltinObject(&io_libecc_number_constructor, constructor, 1, &io_libecc_number_prototype, ECCNSValue.number(create(0)), &io_libecc_number_type);

    io_libecc_Function.addMember(io_libecc_number_constructor, "MAX_VALUE", ECCNSValue.binary(DBL_MAX), r | h | s);
    io_libecc_Function.addMember(io_libecc_number_constructor, "MIN_VALUE", ECCNSValue.binary(DBL_MIN * DBL_EPSILON), r | h | s);
    io_libecc_Function.addMember(io_libecc_number_constructor, "NaN", ECCNSValue.binary(NAN), r | h | s);
    io_libecc_Function.addMember(io_libecc_number_constructor, "NEGATIVE_INFINITY", ECCNSValue.binary(-INFINITY), r | h | s);
    io_libecc_Function.addMember(io_libecc_number_constructor, "POSITIVE_INFINITY", ECCNSValue.binary(INFINITY), r | h | s);

    io_libecc_Function.addToObject(io_libecc_number_prototype, "toString", toString, 1, h);
    io_libecc_Function.addToObject(io_libecc_number_prototype, "toLocaleString", toString, 1, h);
    io_libecc_Function.addToObject(io_libecc_number_prototype, "valueOf", valueOf, 0, h);
    io_libecc_Function.addToObject(io_libecc_number_prototype, "toFixed", toFixed, 1, h);
    io_libecc_Function.addToObject(io_libecc_number_prototype, "toExponential", toExponential, 1, h);
    io_libecc_Function.addToObject(io_libecc_number_prototype, "toPrecision", toPrecision, 1, h);
}

void teardown(void)
{
    io_libecc_number_prototype = NULL;
    io_libecc_number_constructor = NULL;
}

eccobjnumber_t* create(double binary)
{
    eccobjnumber_t* self = malloc(sizeof(*self));
    *self = io_libecc_Number.identity;
    io_libecc_Pool.addObject(&self->object);
    ECCNSObject.initialize(&self->object, io_libecc_number_prototype);

    self->value = binary;

    return self;
}
