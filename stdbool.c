
/*
//  boolean.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
*/

#include "ecc.h"

static eccvalue_t objboolfn_toString(ecccontext_t *context);
static eccvalue_t objboolfn_valueOf(ecccontext_t *context);
static eccvalue_t objboolfn_constructor(ecccontext_t *context);

eccobject_t* ECC_Prototype_Boolean = NULL;
eccobjfunction_t* ECC_CtorFunc_Boolean = NULL;

const eccobjinterntype_t ECC_Type_Boolean = {
    .text = &ECC_String_BooleanType,
};


static eccvalue_t objboolfn_toString(ecccontext_t* context)
{
    int truth;
    ecc_context_assertthismask(context, ECC_VALMASK_BOOLEAN);
    truth = ecc_value_isobject(context->thisvalue) ? context->thisvalue.data.boolean->truth : ecc_value_istrue(context->thisvalue);
    return ecc_value_fromtext(truth ? &ECC_String_True : &ECC_String_False);
}

static eccvalue_t objboolfn_valueOf(ecccontext_t* context)
{
    int truth;
    ecc_context_assertthismask(context, ECC_VALMASK_BOOLEAN);
    truth = ecc_value_isobject(context->thisvalue) ? context->thisvalue.data.boolean->truth : ecc_value_istrue(context->thisvalue);
    return ecc_value_truth(truth);
}

static eccvalue_t objboolfn_constructor(ecccontext_t* context)
{
    char truth;
    truth = ecc_value_istrue(ecc_context_argument(context, 0));
    if(context->construct)
        return ecc_value_boolean(ecc_bool_create(truth));
    return ecc_value_truth(truth);
}

void ecc_bool_setup()
{
    const eccvalflag_t h = ECC_VALFLAG_HIDDEN;
    ecc_function_setupbuiltinobject(&ECC_CtorFunc_Boolean, objboolfn_constructor, 1, &ECC_Prototype_Boolean, ecc_value_boolean(ecc_bool_create(0)), &ECC_Type_Boolean);
    ecc_function_addto(ECC_Prototype_Boolean, "toString", objboolfn_toString, 0, h);
    ecc_function_addto(ECC_Prototype_Boolean, "valueOf", objboolfn_valueOf, 0, h);
}

void ecc_bool_teardown(void)
{
    ECC_Prototype_Boolean = NULL;
    ECC_CtorFunc_Boolean = NULL;
}

eccobjbool_t* ecc_bool_create(int truth)
{
    eccobjbool_t* self = (eccobjbool_t*)malloc(sizeof(*self));
    memset(self, 0, sizeof(eccobjbool_t));
    ecc_mempool_addobject(&self->object);
    ecc_object_initialize(&self->object, ECC_Prototype_Boolean);
    self->truth = truth;
    return self;
}
