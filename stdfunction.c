
/*
//  function.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
*/

#include "ecc.h"

void ecc_function_typecapture(eccobject_t* object);
void ecc_function_typemark(eccobject_t* object);

static va_list empty_ap;

eccobject_t* ECC_Prototype_Function = NULL;
eccobjfunction_t* ECC_CtorFunc_Function = NULL;

const eccobjinterntype_t ECC_Type_Function = {
    .text = &ECC_String_FunctionType,
    .fnmark = ecc_function_typemark,
    .fncapture = ecc_function_typecapture,
    /* XXX: don't finalize */
};

void ecc_function_typecapture(eccobject_t* object)
{
    eccobjfunction_t* self = (eccobjfunction_t*)object;

    if(self->refobject)
        ++self->refobject->refcount;

    if(self->pair)
        ++self->pair->object.refcount;
}

void ecc_function_typemark(eccobject_t* object)
{
    eccobjfunction_t* self = (eccobjfunction_t*)object;

    ecc_mempool_markobject(&self->funcenv);

    if(self->refobject)
        ecc_mempool_markobject(self->refobject);

    if(self->pair)
        ecc_mempool_markobject(&self->pair->object);
}

eccvalue_t ecc_function_tochars(ecccontext_t* context, eccvalue_t value)
{
    eccobjfunction_t* self;
    eccappbuf_t chars;
    (void)context;
    assert(value.type == ECC_VALTYPE_FUNCTION);
    assert(value.data.function);

    self = value.data.function;
    ecc_strbuf_beginappend(&chars);

    ecc_strbuf_append(&chars, "function %s", self->name ? self->name : "anonymous");

    if(self->text.bytes == ECC_String_NativeCode.bytes)
        ecc_strbuf_append(&chars, "() [native code]");
    else
        ecc_strbuf_append(&chars, "%.*s", self->text.length, self->text.bytes);

    return ecc_strbuf_endappend(&chars);
}

eccvalue_t ecc_objfnfunction_tostring(ecccontext_t* context)
{
    ecc_context_assertthistype(context, ECC_VALTYPE_FUNCTION);

    return ecc_function_tochars(context, context->thisvalue);
}

eccvalue_t ecc_objfnfunction_apply(ecccontext_t* context)
{
    eccvalue_t thisval, arguments;

    ecc_context_assertthistype(context, ECC_VALTYPE_FUNCTION);

    context->isstrictmode = context->parent->isstrictmode;

    thisval = ecc_context_argument(context, 0);
    if(thisval.type != ECC_VALTYPE_UNDEFINED && thisval.type != ECC_VALTYPE_NULL)
        thisval = ecc_value_toobject(context, thisval);

    arguments = ecc_context_argument(context, 1);

    if(arguments.type == ECC_VALTYPE_UNDEFINED || arguments.type == ECC_VALTYPE_NULL)
        return ecc_oper_callfunctionva(context, ECC_CTXOFFSET_APPLY, context->thisvalue.data.function, thisval, 0, empty_ap);
    else
    {
        if(!ecc_value_isobject(arguments))
            ecc_context_typeerror(context, ecc_strbuf_create("arguments is not an object"));

        return ecc_oper_callfunctionarguments(context, ECC_CTXOFFSET_APPLY, context->thisvalue.data.function, thisval, arguments.data.object);
    }
}

eccvalue_t ecc_objfnfunction_call(ecccontext_t* context)
{
    eccobject_t arguments;

    ecc_context_assertthistype(context, ECC_VALTYPE_FUNCTION);

    context->isstrictmode = context->parent->isstrictmode;

    arguments = *context->execenv->hmapmapitems[2].hmapmapvalue.data.object;

    if(arguments.hmapitemcount)
    {
        eccvalue_t thisval = ecc_context_argument(context, 0);
        if(thisval.type != ECC_VALTYPE_UNDEFINED && thisval.type != ECC_VALTYPE_NULL)
            thisval = ecc_value_toobject(context, thisval);

        --arguments.hmapitemcapacity;
        --arguments.hmapitemcount;
        ++arguments.hmapitemitems;
        if(!arguments.hmapitemcount)
        {
            arguments.hmapitemitems = NULL;
            arguments.hmapitemcapacity = 0;
        }

        return ecc_oper_callfunctionarguments(context, ECC_CTXOFFSET_CALL, context->thisvalue.data.function, thisval, &arguments);
    }
    else
        return ecc_oper_callfunctionva(context, ECC_CTXOFFSET_CALL, context->thisvalue.data.function, ECCValConstUndefined, 0, empty_ap);
}

eccvalue_t ecc_objfnfunction_bindcall(ecccontext_t* context)
{
    eccobjfunction_t* function;
    eccobject_t* arguments;
    uint32_t count, length;

    ecc_context_assertthistype(context, ECC_VALTYPE_FUNCTION);

    context->isstrictmode = context->parent->isstrictmode;

    function = context->thisvalue.data.function;

    count = ecc_context_argumentcount(context);
    length = (function->funcenv.hmapitemcount - 1) + count;
    arguments = ecc_array_createsized(length);

    memcpy(arguments->hmapitemitems, function->funcenv.hmapitemitems + 1, sizeof(*arguments->hmapitemitems) * (function->funcenv.hmapitemcount - 1));
    memcpy(arguments->hmapitemitems + (function->funcenv.hmapitemcount - 1), context->execenv->hmapmapitems[2].hmapmapvalue.data.object->hmapitemitems,
           sizeof(*arguments->hmapitemitems) * (context->execenv->hmapmapitems[2].hmapmapvalue.data.object->hmapitemcount));

    return ecc_oper_callfunctionarguments(context, 0, context->thisvalue.data.function->pair, function->funcenv.hmapitemitems[0].hmapitemvalue, arguments);
}

eccvalue_t ecc_objfnfunction_bind(ecccontext_t* context)
{
    eccobjfunction_t* function;
    uint32_t index, count;
    int parameterCount = 0;

    ecc_context_assertthistype(context, ECC_VALTYPE_FUNCTION);

    count = ecc_context_argumentcount(context);
    parameterCount = context->thisvalue.data.function->argparamcount - (count > 1 ? count - 1 : 0);
    function = ecc_function_createwithnative(ecc_objfnfunction_bindcall, parameterCount > 0 ? parameterCount : 0);

    ecc_object_resizeelement(&function->funcenv, count ? count : 1);
    if(count)
        for(index = 0; index < count; ++index)
            function->funcenv.hmapitemitems[index].hmapitemvalue = ecc_context_argument(context, index);
    else
        function->funcenv.hmapitemitems[0].hmapitemvalue = ECCValConstUndefined;

    function->pair = context->thisvalue.data.function;
    function->boundthisvalue = ecc_value_function(function);
    function->flags |= ECC_SCRIPTFUNCFLAG_NEEDARGUMENTS | ECC_SCRIPTFUNCFLAG_USEBOUNDTHIS;

    return ecc_value_function(function);
}

eccvalue_t ecc_objfnfunction_prototypeconstructor(ecccontext_t* context)
{
    (void)context;
    return ECCValConstUndefined;
}

eccvalue_t ecc_objfnfunction_constructor(ecccontext_t* context)
{
    int argumentCount;

    argumentCount = ecc_context_argumentcount(context);

    {
        int_fast32_t index;
        eccvalue_t value;
        eccappbuf_t chars;
        eccioinput_t* input;
        ecccontext_t subContext = {};
        subContext.parent = context;
        subContext.thisvalue = ecc_value_object(&context->ecc->globalfunc->funcenv);
        subContext.ecc = context->ecc;
        subContext.depth = context->depth + 1;
        subContext.execenv = ecc_context_environmentroot(context->parent);

        ecc_strbuf_beginappend(&chars);
        ecc_strbuf_append(&chars, "(function (");
        if(argumentCount)
            for(index = 0; index < argumentCount; ++index)
            {
                if(index == argumentCount - 1)
                    ecc_strbuf_append(&chars, "){");

                value = ecc_value_tostring(context, ecc_context_argument(context, index));
                ecc_strbuf_append(&chars, "%.*s", ecc_value_stringlength(&value), ecc_value_stringbytes(&value));

                if(index < argumentCount - 2)
                    ecc_strbuf_append(&chars, ",");
            }
        else
            ecc_strbuf_append(&chars, "){");

        ecc_strbuf_append(&chars, "})");

        value = ecc_strbuf_endappend(&chars);
        input = ecc_ioinput_createfrombytes(ecc_value_stringbytes(&value), ecc_value_stringlength(&value), "(Function)");
        ecc_context_settextindex(context, ECC_CTXINDEXTYPE_NO);
        ecc_script_evalinputwithcontext(context->ecc, input, &subContext);
    }

    return context->ecc->result;
}

void ecc_function_setup()
{
    const eccvalflag_t h = ECC_VALFLAG_HIDDEN;

    ecc_function_setupbuiltinobject(&ECC_CtorFunc_Function, ecc_objfnfunction_constructor, -1, &ECC_Prototype_Function,
                                          ecc_value_function(ecc_function_createwithnative(ecc_objfnfunction_prototypeconstructor, 0)), &ECC_Type_Function);

    ECC_CtorFunc_Function->object.prototype = ECC_Prototype_Function;

    ecc_function_addto(ECC_Prototype_Function, "toString", ecc_objfnfunction_tostring, 0, h);
    ecc_function_addto(ECC_Prototype_Function, "apply", ecc_objfnfunction_apply, 2, h);
    ecc_function_addto(ECC_Prototype_Function, "call", ecc_objfnfunction_call, -1, h);
    ecc_function_addto(ECC_Prototype_Function, "bind", ecc_objfnfunction_bind, -1, h);
}

void ecc_function_teardown(void)
{
    ECC_Prototype_Function = NULL;
    ECC_CtorFunc_Function = NULL;
}

eccobjfunction_t* ecc_function_create(eccobject_t* environment)
{
    return ecc_function_createsized(environment, 8);
}

eccobjfunction_t* ecc_function_createsized(eccobject_t* environment, uint32_t size)
{
    eccobjfunction_t* self = (eccobjfunction_t*)malloc(sizeof(*self));
    ecc_mempool_addfunction(self);
    memset(self, 0, sizeof(eccobjfunction_t));
    ecc_object_initialize(&self->object, ECC_Prototype_Function);
    ecc_object_initializesized(&self->funcenv, environment, size);

    return self;
}

eccobjfunction_t* ecc_function_createwithnative(const eccnativefuncptr_t native, int parameterCount)
{
    eccobjfunction_t* self = NULL;

    if(parameterCount < 0)
    {
        self = ecc_function_createsized(NULL, 3);
        self->flags |= ECC_SCRIPTFUNCFLAG_NEEDARGUMENTS;
    }
    else
    {
        self = ecc_function_createsized(NULL, 3 + parameterCount);
        self->argparamcount = parameterCount;
    }

    self->funcenv.hmapmapcount = self->funcenv.hmapmapcapacity;
    self->oplist = ecc_oplist_create(native, ECCValConstUndefined, ECC_String_NativeCode);
    self->text = ECC_String_NativeCode;

    ecc_object_addmember(&self->object, ECC_ConstKey_length, ecc_value_fromint(abs(parameterCount)), ECC_VALFLAG_READONLY | ECC_VALFLAG_HIDDEN | ECC_VALFLAG_SEALED);

    return self;
}

eccobjfunction_t* ecc_function_copy(eccobjfunction_t* original)
{
    eccobjfunction_t* self = (eccobjfunction_t*)malloc(sizeof(*self));
    size_t byteSize;

    assert(original);
    ecc_mempool_addobject(&self->object);

    *self = *original;

    byteSize = sizeof(*self->object.hmapmapitems) * self->object.hmapmapcapacity;
    self->object.hmapmapitems = (ecchashmap_t*)malloc(byteSize);
    memcpy(self->object.hmapmapitems, original->object.hmapmapitems, byteSize);

    return self;
}

void ecc_function_destroy(eccobjfunction_t* self)
{
    assert(self);

    ecc_object_finalize(&self->object);
    ecc_object_finalize(&self->funcenv);

    if(self->oplist)
        ecc_oplist_destroy(self->oplist), self->oplist = NULL;

    free(self), self = NULL;
}

void ecc_function_addmember(eccobjfunction_t* self, const char* name, eccvalue_t value, int flags)
{
    assert(self);

    if(value.type == ECC_VALTYPE_FUNCTION)
        value.data.function->name = name;

    ecc_object_addmember(&self->object, ecc_keyidx_makewithcstring(name), value, flags);
}

eccobjfunction_t* ecc_function_addmethod(eccobjfunction_t* self, const char* name, const eccnativefuncptr_t native, int parameterCount, int flags)
{
    assert(self);

    return ecc_function_addto(&self->object, name, native, parameterCount, flags);
}

void ecc_function_addvalue(eccobjfunction_t* self, const char* name, eccvalue_t value, int flags)
{
    assert(self);

    if(value.type == ECC_VALTYPE_FUNCTION)
        value.data.function->name = name;

    ecc_object_addmember(&self->funcenv, ecc_keyidx_makewithcstring(name), value, flags);
}

eccobjfunction_t* ecc_function_addfunction(eccobjfunction_t* self, const char* name, const eccnativefuncptr_t native, int parameterCount, int flags)
{
    assert(self);

    return ecc_function_addto(&self->funcenv, name, native, parameterCount, flags);
}

eccobjfunction_t* ecc_function_addto(eccobject_t* object, const char* name, const eccnativefuncptr_t native, int parameterCount, int flags)
{
    eccobjfunction_t* function;

    assert(object);

    function = ecc_function_createwithnative(native, parameterCount);
    function->name = name;

    ecc_object_addmember(object, ecc_keyidx_makewithcstring(name), ecc_value_function(function), flags);

    return function;
}

void ecc_function_linkprototype(eccobjfunction_t* self, eccvalue_t prototype, int flags)
{
    assert(self);

    ecc_object_addmember(prototype.data.object, ECC_ConstKey_constructor, ecc_value_function(self), ECC_VALFLAG_HIDDEN);
    ecc_object_addmember(&self->object, ECC_ConstKey_prototype, prototype, flags);
}

void ecc_function_setupbuiltinobject(eccobjfunction_t** constructor,
                        const eccnativefuncptr_t native,
                        int parameterCount,
                        eccobject_t** prototype,
                        eccvalue_t prototypeValue,
                        const eccobjinterntype_t* type)
{
    eccobjfunction_t* function = ecc_function_createwithnative(native, parameterCount);

    if(prototype)
    {
        eccobject_t* object = prototypeValue.data.object;
        object->type = type;

        if(!object->prototype)
            object->prototype = ECC_Prototype_Object;

        *prototype = object;
    }

    ecc_function_linkprototype(function, prototypeValue, ECC_VALFLAG_READONLY | ECC_VALFLAG_HIDDEN | ECC_VALFLAG_SEALED);
    *constructor = function;
}

eccvalue_t ecc_function_accessor(const eccnativefuncptr_t getter, const eccnativefuncptr_t setter)
{
    eccvalue_t value;
    eccobjfunction_t *getterFunction = NULL, *setterFunction = NULL;
    if(setter)
        setterFunction = ecc_function_createwithnative(setter, 1);

    if(getter)
    {
        getterFunction = ecc_function_createwithnative(getter, 0);
        getterFunction->pair = setterFunction;
        value = ecc_value_function(getterFunction);
        value.flags |= ECC_VALFLAG_GETTER;
    }
    else if(setter)
    {
        value = ecc_value_function(setterFunction);
        value.flags |= ECC_VALFLAG_SETTER;
    }
    else
        value = ECCValConstUndefined;

    return value;
}
