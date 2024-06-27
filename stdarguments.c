//
//  arguments.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"

static void nsargumentsfn_setup(void);
static void nsargumentsfn_teardown(void);
static eccobject_t* nsargumentsfn_createSized(uint32_t size);
static eccobject_t* nsargumentsfn_createWithCList(int count, const char* list[]);


eccobject_t* ECC_Prototype_Arguments;

const eccobjinterntype_t ECC_Type_Arguments = {
    .text = &ECC_ConstString_ArgumentsType,
};

const struct eccpseudonsarguments_t ECCNSArguments = {
    nsargumentsfn_setup,
    nsargumentsfn_teardown,
    nsargumentsfn_createSized,
    nsargumentsfn_createWithCList,
    {}
};

static eccvalue_t argobjfn_getLength(eccstate_t* context)
{
    return ECCNSValue.binary(context->thisvalue.data.object->elementCount);
}

static eccvalue_t argobjfn_setLength(eccstate_t* context)
{
    double length;

    length = ECCNSValue.toBinary(context, ECCNSContext.argument(context, 0)).data.binary;
    if(!isfinite(length) || length < 0 || length > UINT32_MAX || length != (uint32_t)length)
        ECCNSContext.rangeError(context, ECCNSChars.create("invalid array length"));

    if(ECCNSObject.resizeElement(context->thisvalue.data.object, length) && context->strictMode)
    {
        ECCNSContext.typeError(context, ECCNSChars.create("'%u' is non-configurable", context->thisvalue.data.object->elementCount));
    }

    return ECCValConstUndefined;
}

static eccvalue_t argobjfn_getCallee(eccstate_t* context)
{
    ECCNSContext.rewindStatement(context->parent);
    ECCNSContext.typeError(context, ECCNSChars.create("'callee' cannot be accessed in this context"));

    return ECCValConstUndefined;
}

static eccvalue_t argobjfn_setCallee(eccstate_t* context)
{
    ECCNSContext.rewindStatement(context->parent);
    ECCNSContext.typeError(context, ECCNSChars.create("'callee' cannot be accessed in this context"));

    return ECCValConstUndefined;
}


static void nsargumentsfn_setup(void)
{
    const eccvalflag_t h = ECC_VALFLAG_HIDDEN;
    const eccvalflag_t s = ECC_VALFLAG_SEALED;

    ECC_Prototype_Arguments = ECCNSObject.createTyped(&ECC_Type_Arguments);

    ECCNSObject.addMember(ECC_Prototype_Arguments, ECC_ConstKey_length, ECCNSFunction.accessor(argobjfn_getLength, argobjfn_setLength), h | s | ECC_VALFLAG_ASOWN | ECC_VALFLAG_ASDATA);
    ECCNSObject.addMember(ECC_Prototype_Arguments, ECC_ConstKey_callee, ECCNSFunction.accessor(argobjfn_getCallee, argobjfn_setCallee), h | s | ECC_VALFLAG_ASOWN);
}

static void nsargumentsfn_teardown(void)
{
    ECC_Prototype_Arguments = NULL;
}

static eccobject_t* nsargumentsfn_createSized(uint32_t size)
{
    eccobject_t* self = ECCNSObject.create(ECC_Prototype_Arguments);

    ECCNSObject.resizeElement(self, size);

    return self;
}

static eccobject_t* nsargumentsfn_createWithCList(int count, const char* list[])
{
    eccobject_t* self = nsargumentsfn_createSized(count);

    ECCNSObject.populateElementWithCList(self, count, list);

    return self;
}
