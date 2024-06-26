//
//  arguments.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"

// MARK: - Private

eccobject_t* io_libecc_arguments_prototype;

const eccobjinterntype_t ECCObjTypeArguments = {
    .text = &ECC_ConstString_ArgumentsType,
};

// MARK: - Static Members

static void setup(void);
static void teardown(void);
static eccobject_t* createSized(uint32_t size);
static eccobject_t* createWithCList(int count, const char* list[]);
const struct eccpseudonsarguments_t ECCNSArguments = {
    setup,
    teardown,
    createSized,
    createWithCList,
    {}
};

static eccvalue_t getLength(eccstate_t* context)
{
    return ECCNSValue.binary(context->thisvalue.data.object->elementCount);
}

static eccvalue_t setLength(eccstate_t* context)
{
    double length;

    length = ECCNSValue.toBinary(context, ECCNSContext.argument(context, 0)).data.binary;
    if(!isfinite(length) || length < 0 || length > UINT32_MAX || length != (uint32_t)length)
        ECCNSContext.rangeError(context, io_libecc_Chars.create("invalid array length"));

    if(ECCNSObject.resizeElement(context->thisvalue.data.object, length) && context->strictMode)
    {
        ECCNSContext.typeError(context, io_libecc_Chars.create("'%u' is non-configurable", context->thisvalue.data.object->elementCount));
    }

    return ECCValConstUndefined;
}

static eccvalue_t getCallee(eccstate_t* context)
{
    ECCNSContext.rewindStatement(context->parent);
    ECCNSContext.typeError(context, io_libecc_Chars.create("'callee' cannot be accessed in this context"));

    return ECCValConstUndefined;
}

static eccvalue_t setCallee(eccstate_t* context)
{
    ECCNSContext.rewindStatement(context->parent);
    ECCNSContext.typeError(context, io_libecc_Chars.create("'callee' cannot be accessed in this context"));

    return ECCValConstUndefined;
}

// MARK: - Methods

void setup(void)
{
    const eccvalflag_t h = ECC_VALFLAG_HIDDEN;
    const eccvalflag_t s = ECC_VALFLAG_SEALED;

    io_libecc_arguments_prototype = ECCNSObject.createTyped(&ECCObjTypeArguments);

    ECCNSObject.addMember(io_libecc_arguments_prototype, io_libecc_key_length, io_libecc_Function.accessor(getLength, setLength), h | s | ECC_VALFLAG_ASOWN | ECC_VALFLAG_ASDATA);
    ECCNSObject.addMember(io_libecc_arguments_prototype, io_libecc_key_callee, io_libecc_Function.accessor(getCallee, setCallee), h | s | ECC_VALFLAG_ASOWN);
}

void teardown(void)
{
    io_libecc_arguments_prototype = NULL;
}

eccobject_t* createSized(uint32_t size)
{
    eccobject_t* self = ECCNSObject.create(io_libecc_arguments_prototype);

    ECCNSObject.resizeElement(self, size);

    return self;
}

eccobject_t* createWithCList(int count, const char* list[])
{
    eccobject_t* self = createSized(count);

    ECCNSObject.populateElementWithCList(self, count, list);

    return self;
}
