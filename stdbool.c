//
//  boolean.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"

// MARK: - Private

eccobject_t* io_libecc_boolean_prototype = NULL;
eccobjscriptfunction_t* io_libecc_boolean_constructor = NULL;

const eccobjinterntype_t io_libecc_boolean_type = {
    .text = &ECC_ConstString_BooleanType,
};

// MARK: - Static Members

static void setup(void);
static void teardown(void);
static eccobjbool_t* create(int);
const struct type_io_libecc_Boolean io_libecc_Boolean = {
    setup,
    teardown,
    create,
    {}
};

static eccvalue_t toString(eccstate_t* context)
{
    int truth;

    ECCNSContext.assertThisMask(context, ECC_VALMASK_BOOLEAN);

    truth = ECCNSValue.isObject(context->thisvalue) ? context->thisvalue.data.boolean->truth : ECCNSValue.isTrue(context->thisvalue);

    return ECCNSValue.text(truth ? &ECC_ConstString_True : &ECC_ConstString_False);
}

static eccvalue_t valueOf(eccstate_t* context)
{
    int truth;

    ECCNSContext.assertThisMask(context, ECC_VALMASK_BOOLEAN);

    truth = ECCNSValue.isObject(context->thisvalue) ? context->thisvalue.data.boolean->truth : ECCNSValue.isTrue(context->thisvalue);

    return ECCNSValue.truth(truth);
}

static eccvalue_t constructor(eccstate_t* context)
{
    char truth;

    truth = ECCNSValue.isTrue(ECCNSContext.argument(context, 0));
    if(context->construct)
        return ECCNSValue.boolean(io_libecc_Boolean.create(truth));
    else
        return ECCNSValue.truth(truth);
}

// MARK: - Methods

void setup()
{
    const eccvalflag_t h = ECC_VALFLAG_HIDDEN;

    io_libecc_Function.setupBuiltinObject(&io_libecc_boolean_constructor, constructor, 1, &io_libecc_boolean_prototype, ECCNSValue.boolean(create(0)), &io_libecc_boolean_type);

    io_libecc_Function.addToObject(io_libecc_boolean_prototype, "toString", toString, 0, h);
    io_libecc_Function.addToObject(io_libecc_boolean_prototype, "valueOf", valueOf, 0, h);
}

void teardown(void)
{
    io_libecc_boolean_prototype = NULL;
    io_libecc_boolean_constructor = NULL;
}

eccobjbool_t* create(int truth)
{
    eccobjbool_t* self = malloc(sizeof(*self));
    *self = io_libecc_Boolean.identity;
    io_libecc_Pool.addObject(&self->object);
    ECCNSObject.initialize(&self->object, io_libecc_boolean_prototype);

    self->truth = truth;

    return self;
}
