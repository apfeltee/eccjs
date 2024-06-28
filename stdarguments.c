
/*
//  arguments.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
*/

#include "ecc.h"
#include "compat.h"

eccobject_t* ECC_Prototype_Arguments;

const eccobjinterntype_t ECC_Type_Arguments = {
    .text = &ECC_String_ArgumentsType,
};


eccvalue_t argobjfn_getLength(ecccontext_t* context)
{
    return ecc_value_fromfloat(context->thisvalue.data.object->hmapitemcount);
}

eccvalue_t argobjfn_setLength(ecccontext_t* context)
{
    double length;

    length = ecc_value_tobinary(context, ecc_context_argument(context, 0)).data.valnumfloat;
    if(!isfinite(length) || length < 0 || length > UINT32_MAX || length != (uint32_t)length)
        ecc_context_rangeerror(context, ecc_strbuf_create("invalid array length"));

    if(ecc_object_resizeelement(context->thisvalue.data.object, length) && context->strictMode)
    {
        ecc_context_typeerror(context, ecc_strbuf_create("'%u' is non-configurable", context->thisvalue.data.object->hmapitemcount));
    }

    return ECCValConstUndefined;
}

eccvalue_t argobjfn_getCallee(ecccontext_t* context)
{
    ecc_context_rewindstatement(context->parent);
    ecc_context_typeerror(context, ecc_strbuf_create("'callee' cannot be accessed in this context"));

    return ECCValConstUndefined;
}

eccvalue_t argobjfn_setCallee(ecccontext_t* context)
{
    ecc_context_rewindstatement(context->parent);
    ecc_context_typeerror(context, ecc_strbuf_create("'callee' cannot be accessed in this context"));

    return ECCValConstUndefined;
}


void ecc_args_setup(void)
{
    const eccvalflag_t h = ECC_VALFLAG_HIDDEN;
    const eccvalflag_t s = ECC_VALFLAG_SEALED;

    ECC_Prototype_Arguments = ecc_object_createtyped(&ECC_Type_Arguments);

    ecc_object_addmember(ECC_Prototype_Arguments, ECC_ConstKey_length, ecc_function_accessor(argobjfn_getLength, argobjfn_setLength), h | s | ECC_VALFLAG_ASOWN | ECC_VALFLAG_ASDATA);
    ecc_object_addmember(ECC_Prototype_Arguments, ECC_ConstKey_callee, ecc_function_accessor(argobjfn_getCallee, argobjfn_setCallee), h | s | ECC_VALFLAG_ASOWN);
}

void ecc_args_teardown(void)
{
    ECC_Prototype_Arguments = NULL;
}

eccobject_t* ecc_args_createsized(uint32_t size)
{
    eccobject_t* self = ecc_object_create(ECC_Prototype_Arguments);

    ecc_object_resizeelement(self, size);

    return self;
}

eccobject_t* ecc_args_createwithclist(int count, const char* list[])
{
    eccobject_t* self = ecc_args_createsized(count);

    ecc_object_populateelementwithclist(self, count, list);

    return self;
}
