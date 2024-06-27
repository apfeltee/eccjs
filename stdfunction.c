//
//  function.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"

static va_list empty_ap;

eccobject_t* ECC_Prototype_Function = NULL;
eccobjfunction_t* ECC_CtorFunc_Function = NULL;

const eccobjinterntype_t ECC_Type_Function = {
    .text = &ECC_ConstString_FunctionType, .mark = eccfunction_mark, .capture = eccfunction_capture,
    /* XXX: don't finalize */
};

void eccfunction_capture(eccobject_t* object)
{
    eccobjfunction_t* self = (eccobjfunction_t*)object;

    if(self->refObject)
        ++self->refObject->referenceCount;

    if(self->pair)
        ++self->pair->object.referenceCount;
}

void eccfunction_mark(eccobject_t* object)
{
    eccobjfunction_t* self = (eccobjfunction_t*)object;

    ecc_mempool_markobject(&self->environment);

    if(self->refObject)
        ecc_mempool_markobject(self->refObject);

    if(self->pair)
        ecc_mempool_markobject(&self->pair->object);
}

// MARK: - Static Members

eccvalue_t eccfunction_toChars(eccstate_t* context, eccvalue_t value)
{
    eccobjfunction_t* self;
    eccappendbuffer_t chars;
    (void)context;
    assert(value.type == ECC_VALTYPE_FUNCTION);
    assert(value.data.function);

    self = value.data.function;
    ecc_charbuf_beginappend(&chars);

    ecc_charbuf_append(&chars, "function %s", self->name ? self->name : "anonymous");

    if(self->text.bytes == ECC_ConstString_NativeCode.bytes)
        ecc_charbuf_append(&chars, "() [native code]");
    else
        ecc_charbuf_append(&chars, "%.*s", self->text.length, self->text.bytes);

    return ecc_charbuf_endappend(&chars);
}

eccvalue_t objfunctionfn_toString(eccstate_t* context)
{
    ecc_context_assertthistype(context, ECC_VALTYPE_FUNCTION);

    return eccfunction_toChars(context, context->thisvalue);
}

eccvalue_t objfunctionfn_apply(eccstate_t* context)
{
    eccvalue_t thisval, arguments;

    ecc_context_assertthistype(context, ECC_VALTYPE_FUNCTION);

    context->strictMode = context->parent->strictMode;

    thisval = ecc_context_argument(context, 0);
    if(thisval.type != ECC_VALTYPE_UNDEFINED && thisval.type != ECC_VALTYPE_NULL)
        thisval = ecc_value_toobject(context, thisval);

    arguments = ecc_context_argument(context, 1);

    if(arguments.type == ECC_VALTYPE_UNDEFINED || arguments.type == ECC_VALTYPE_NULL)
        return ecc_oper_callfunctionva(context, ECC_CTXOFFSET_APPLY, context->thisvalue.data.function, thisval, 0, empty_ap);
    else
    {
        if(!ecc_value_isobject(arguments))
            ecc_context_typeerror(context, ecc_charbuf_create("arguments is not an object"));

        return ecc_oper_callfunctionarguments(context, ECC_CTXOFFSET_APPLY, context->thisvalue.data.function, thisval, arguments.data.object);
    }
}

eccvalue_t objfunctionfn_call(eccstate_t* context)
{
    eccobject_t arguments;

    ecc_context_assertthistype(context, ECC_VALTYPE_FUNCTION);

    context->strictMode = context->parent->strictMode;

    arguments = *context->environment->hashmap[2].value.data.object;

    if(arguments.elementCount)
    {
        eccvalue_t thisval = ecc_context_argument(context, 0);
        if(thisval.type != ECC_VALTYPE_UNDEFINED && thisval.type != ECC_VALTYPE_NULL)
            thisval = ecc_value_toobject(context, thisval);

        --arguments.elementCapacity;
        --arguments.elementCount;
        ++arguments.element;
        if(!arguments.elementCount)
        {
            arguments.element = NULL;
            arguments.elementCapacity = 0;
        }

        return ecc_oper_callfunctionarguments(context, ECC_CTXOFFSET_CALL, context->thisvalue.data.function, thisval, &arguments);
    }
    else
        return ecc_oper_callfunctionva(context, ECC_CTXOFFSET_CALL, context->thisvalue.data.function, ECCValConstUndefined, 0, empty_ap);
}

eccvalue_t objfunctionfn_bindCall(eccstate_t* context)
{
    eccobjfunction_t* function;
    eccobject_t* arguments;
    uint16_t count, length;

    ecc_context_assertthistype(context, ECC_VALTYPE_FUNCTION);

    context->strictMode = context->parent->strictMode;

    function = context->thisvalue.data.function;

    count = ecc_context_argumentcount(context);
    length = (function->environment.elementCount - 1) + count;
    arguments = ecc_array_createsized(length);

    memcpy(arguments->element, function->environment.element + 1, sizeof(*arguments->element) * (function->environment.elementCount - 1));
    memcpy(arguments->element + (function->environment.elementCount - 1), context->environment->hashmap[2].value.data.object->element,
           sizeof(*arguments->element) * (context->environment->hashmap[2].value.data.object->elementCount));

    return ecc_oper_callfunctionarguments(context, 0, context->thisvalue.data.function->pair, function->environment.element[0].value, arguments);
}

eccvalue_t objfunctionfn_bind(eccstate_t* context)
{
    eccobjfunction_t* function;
    uint16_t index, count;
    int parameterCount = 0;

    ecc_context_assertthistype(context, ECC_VALTYPE_FUNCTION);

    count = ecc_context_argumentcount(context);
    parameterCount = context->thisvalue.data.function->parameterCount - (count > 1 ? count - 1 : 0);
    function = ecc_function_createwithnative(objfunctionfn_bindCall, parameterCount > 0 ? parameterCount : 0);

    ecc_object_resizeelement(&function->environment, count ? count : 1);
    if(count)
        for(index = 0; index < count; ++index)
            function->environment.element[index].value = ecc_context_argument(context, index);
    else
        function->environment.element[0].value = ECCValConstUndefined;

    function->pair = context->thisvalue.data.function;
    function->boundThis = ecc_value_function(function);
    function->flags |= ECC_SCRIPTFUNCFLAG_NEEDARGUMENTS | ECC_SCRIPTFUNCFLAG_USEBOUNDTHIS;

    return ecc_value_function(function);
}

eccvalue_t objfunctionfn_prototypeConstructor(eccstate_t* context)
{
    (void)context;
    return ECCValConstUndefined;
}

eccvalue_t objfunctionfn_constructor(eccstate_t* context)
{
    int argumentCount;

    argumentCount = ecc_context_argumentcount(context);

    {
        int_fast32_t index;
        eccvalue_t value;
        eccappendbuffer_t chars;
        eccioinput_t* input;
        eccstate_t subContext = {};
        subContext.parent = context;
        subContext.thisvalue = ecc_value_object(&context->ecc->global->environment);
        subContext.ecc = context->ecc;
        subContext.depth = context->depth + 1;
        subContext.environment = ecc_context_environmentroot(context->parent);

        ecc_charbuf_beginappend(&chars);
        ecc_charbuf_append(&chars, "(function (");
        if(argumentCount)
            for(index = 0; index < argumentCount; ++index)
            {
                if(index == argumentCount - 1)
                    ecc_charbuf_append(&chars, "){");

                value = ecc_value_tostring(context, ecc_context_argument(context, index));
                ecc_charbuf_append(&chars, "%.*s", ecc_value_stringlength(&value), ecc_value_stringbytes(&value));

                if(index < argumentCount - 2)
                    ecc_charbuf_append(&chars, ",");
            }
        else
            ecc_charbuf_append(&chars, "){");

        ecc_charbuf_append(&chars, "})");

        value = ecc_charbuf_endappend(&chars);
        input = ecc_ioinput_createfrombytes(ecc_value_stringbytes(&value), ecc_value_stringlength(&value), "(Function)");
        ecc_context_settextindex(context, ECC_CTXINDEXTYPE_NO);
        ecc_script_evalinputwithcontext(context->ecc, input, &subContext);
    }

    return context->ecc->result;
}

// MARK: - Methods

void ecc_function_setup()
{
    const eccvalflag_t h = ECC_VALFLAG_HIDDEN;

    ecc_function_setupbuiltinobject(&ECC_CtorFunc_Function, objfunctionfn_constructor, -1, &ECC_Prototype_Function,
                                          ecc_value_function(ecc_function_createwithnative(objfunctionfn_prototypeConstructor, 0)), &ECC_Type_Function);

    ECC_CtorFunc_Function->object.prototype = ECC_Prototype_Function;

    ecc_function_addto(ECC_Prototype_Function, "toString", objfunctionfn_toString, 0, h);
    ecc_function_addto(ECC_Prototype_Function, "apply", objfunctionfn_apply, 2, h);
    ecc_function_addto(ECC_Prototype_Function, "call", objfunctionfn_call, -1, h);
    ecc_function_addto(ECC_Prototype_Function, "bind", objfunctionfn_bind, -1, h);
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
    ecc_object_initializesized(&self->environment, environment, size);

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
        self->parameterCount = parameterCount;
    }

    self->environment.hashmapCount = self->environment.hashmapCapacity;
    self->oplist = ecc_oplist_create(native, ECCValConstUndefined, ECC_ConstString_NativeCode);
    self->text = ECC_ConstString_NativeCode;

    ecc_object_addmember(&self->object, ECC_ConstKey_length, ecc_value_integer(abs(parameterCount)), ECC_VALFLAG_READONLY | ECC_VALFLAG_HIDDEN | ECC_VALFLAG_SEALED);

    return self;
}

eccobjfunction_t* ecc_function_copy(eccobjfunction_t* original)
{
    eccobjfunction_t* self = (eccobjfunction_t*)malloc(sizeof(*self));
    size_t byteSize;

    assert(original);
    ecc_mempool_addobject(&self->object);

    *self = *original;

    byteSize = sizeof(*self->object.hashmap) * self->object.hashmapCapacity;
    self->object.hashmap = (ecchashmap_t*)malloc(byteSize);
    memcpy(self->object.hashmap, original->object.hashmap, byteSize);

    return self;
}

void ecc_function_destroy(eccobjfunction_t* self)
{
    assert(self);

    ecc_object_finalize(&self->object);
    ecc_object_finalize(&self->environment);

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

    ecc_object_addmember(&self->environment, ecc_keyidx_makewithcstring(name), value, flags);
}

eccobjfunction_t* ecc_function_addfunction(eccobjfunction_t* self, const char* name, const eccnativefuncptr_t native, int parameterCount, int flags)
{
    assert(self);

    return ecc_function_addto(&self->environment, name, native, parameterCount, flags);
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
