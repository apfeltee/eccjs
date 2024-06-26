//
//  boolean.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"

/* stdbool.c */
static eccvalue_t objboolfn_toString(eccstate_t *context);
static eccvalue_t objboolfn_valueOf(eccstate_t *context);
static eccvalue_t objboolfn_constructor(eccstate_t *context);
static void nsboolfn_setup(void);
static void nsboolfn_teardown(void);
static eccobjbool_t *nsboolfn_create(int truth);


eccobject_t* ECC_Prototype_Boolean = NULL;
eccobjscriptfunction_t* ECC_CtorFunc_Boolean = NULL;

const eccobjinterntype_t ECC_Type_Boolean = {
    .text = &ECC_ConstString_BooleanType,
};

const struct eccpseudonsboolean_t ECCNSBool = {
    nsboolfn_setup,
    nsboolfn_teardown,
    nsboolfn_create,
    {}
};

static eccvalue_t objboolfn_toString(eccstate_t* context)
{
    int truth;

    ECCNSContext.assertThisMask(context, ECC_VALMASK_BOOLEAN);

    truth = ECCNSValue.isObject(context->thisvalue) ? context->thisvalue.data.boolean->truth : ECCNSValue.isTrue(context->thisvalue);

    return ECCNSValue.text(truth ? &ECC_ConstString_True : &ECC_ConstString_False);
}

static eccvalue_t objboolfn_valueOf(eccstate_t* context)
{
    int truth;

    ECCNSContext.assertThisMask(context, ECC_VALMASK_BOOLEAN);

    truth = ECCNSValue.isObject(context->thisvalue) ? context->thisvalue.data.boolean->truth : ECCNSValue.isTrue(context->thisvalue);

    return ECCNSValue.truth(truth);
}

static eccvalue_t objboolfn_constructor(eccstate_t* context)
{
    char truth;

    truth = ECCNSValue.isTrue(ECCNSContext.argument(context, 0));
    if(context->construct)
        return ECCNSValue.boolean(ECCNSBool.create(truth));
    else
        return ECCNSValue.truth(truth);
}


static void nsboolfn_setup()
{
    const eccvalflag_t h = ECC_VALFLAG_HIDDEN;

    ECCNSFunction.setupBuiltinObject(&ECC_CtorFunc_Boolean, objboolfn_constructor, 1, &ECC_Prototype_Boolean, ECCNSValue.boolean(nsboolfn_create(0)), &ECC_Type_Boolean);

    ECCNSFunction.addToObject(ECC_Prototype_Boolean, "toString", objboolfn_toString, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_Boolean, "valueOf", objboolfn_valueOf, 0, h);
}

static void nsboolfn_teardown(void)
{
    ECC_Prototype_Boolean = NULL;
    ECC_CtorFunc_Boolean = NULL;
}

static eccobjbool_t* nsboolfn_create(int truth)
{
    eccobjbool_t* self = malloc(sizeof(*self));
    *self = ECCNSBool.identity;
    ECCNSMemoryPool.addObject(&self->object);
    ECCNSObject.initialize(&self->object, ECC_Prototype_Boolean);

    self->truth = truth;

    return self;
}
