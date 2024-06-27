//
//  function.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"

static void eccfunction_capture(eccobject_t *object);
static void eccfunction_mark(eccobject_t *object);
static eccvalue_t eccfunction_toChars(eccstate_t *context, eccvalue_t value);
static eccvalue_t objfunctionfn_toString(eccstate_t *context);
static eccvalue_t objfunctionfn_apply(eccstate_t *context);
static eccvalue_t objfunctionfn_call(eccstate_t *context);
static eccvalue_t objfunctionfn_bindCall(eccstate_t *context);
static eccvalue_t objfunctionfn_bind(eccstate_t *context);
static eccvalue_t objfunctionfn_prototypeConstructor(eccstate_t *context);
static eccvalue_t objfunctionfn_constructor(eccstate_t *context);
static void nsfunctionfn_setup(void);
static void nsfunctionfn_teardown(void);
static eccobjfunction_t *nsfunctionfn_create(eccobject_t *environment);
static eccobjfunction_t *nsfunctionfn_createSized(eccobject_t *environment, uint32_t size);
static eccobjfunction_t *nsfunctionfn_createWithNative(const eccnativefuncptr_t native, int parameterCount);
static eccobjfunction_t *nsfunctionfn_copy(eccobjfunction_t *original);
static void nsfunctionfn_destroy(eccobjfunction_t *self);
static void nsfunctionfn_addMember(eccobjfunction_t *self, const char *name, eccvalue_t value, int flags);
static eccobjfunction_t *nsfunctionfn_addMethod(eccobjfunction_t *self, const char *name, const eccnativefuncptr_t native, int parameterCount, int flags);
static void nsfunctionfn_addValue(eccobjfunction_t *self, const char *name, eccvalue_t value, int flags);
static eccobjfunction_t *nsfunctionfn_addFunction(eccobjfunction_t *self, const char *name, const eccnativefuncptr_t native, int parameterCount, int flags);
static eccobjfunction_t *nsfunctionfn_addToObject(eccobject_t *object, const char *name, const eccnativefuncptr_t native, int parameterCount, int flags);
static void nsfunctionfn_linkPrototype(eccobjfunction_t *self, eccvalue_t prototype, int flags);
static void nsfunctionfn_setupBuiltinObject(eccobjfunction_t **constructor, const eccnativefuncptr_t native, int parameterCount, eccobject_t **prototype, eccvalue_t prototypeValue, const eccobjinterntype_t *type);
static eccvalue_t nsfunctionfn_accessor(const eccnativefuncptr_t getter, const eccnativefuncptr_t setter);


const struct eccpseudonsfunction_t ECCNSFunction = {
    nsfunctionfn_setup,
    nsfunctionfn_teardown,
    nsfunctionfn_create,
    nsfunctionfn_createSized,
    nsfunctionfn_createWithNative,
    nsfunctionfn_copy,
    nsfunctionfn_destroy,
    nsfunctionfn_addMember,
    nsfunctionfn_addValue,
    nsfunctionfn_addMethod,
    nsfunctionfn_addFunction,
    nsfunctionfn_addToObject,
    nsfunctionfn_linkPrototype,
    nsfunctionfn_setupBuiltinObject,
    nsfunctionfn_accessor,
    {}
};

// MARK: - Private

static va_list empty_ap;

eccobject_t* ECC_Prototype_Function = NULL;
eccobjfunction_t* ECC_CtorFunc_Function = NULL;

const eccobjinterntype_t ECC_Type_Function = {
    .text = &ECC_ConstString_FunctionType, .mark = eccfunction_mark, .capture = eccfunction_capture,
    /* XXX: don't finalize */
};

static void eccfunction_capture(eccobject_t* object)
{
    eccobjfunction_t* self = (eccobjfunction_t*)object;

    if(self->refObject)
        ++self->refObject->referenceCount;

    if(self->pair)
        ++self->pair->object.referenceCount;
}

static void eccfunction_mark(eccobject_t* object)
{
    eccobjfunction_t* self = (eccobjfunction_t*)object;

    ECCNSMemoryPool.markObject(&self->environment);

    if(self->refObject)
        ECCNSMemoryPool.markObject(self->refObject);

    if(self->pair)
        ECCNSMemoryPool.markObject(&self->pair->object);
}

// MARK: - Static Members

static eccvalue_t eccfunction_toChars(eccstate_t* context, eccvalue_t value)
{
    eccobjfunction_t* self;
    eccappendbuffer_t chars;
    (void)context;
    assert(value.type == ECC_VALTYPE_FUNCTION);
    assert(value.data.function);

    self = value.data.function;
    ECCNSChars.beginAppend(&chars);

    ECCNSChars.append(&chars, "function %s", self->name ? self->name : "anonymous");

    if(self->text.bytes == ECC_ConstString_NativeCode.bytes)
        ECCNSChars.append(&chars, "() [native code]");
    else
        ECCNSChars.append(&chars, "%.*s", self->text.length, self->text.bytes);

    return ECCNSChars.endAppend(&chars);
}

static eccvalue_t objfunctionfn_toString(eccstate_t* context)
{
    ECCNSContext.assertThisType(context, ECC_VALTYPE_FUNCTION);

    return eccfunction_toChars(context, context->thisvalue);
}

static eccvalue_t objfunctionfn_apply(eccstate_t* context)
{
    eccvalue_t thisval, arguments;

    ECCNSContext.assertThisType(context, ECC_VALTYPE_FUNCTION);

    context->strictMode = context->parent->strictMode;

    thisval = ECCNSContext.argument(context, 0);
    if(thisval.type != ECC_VALTYPE_UNDEFINED && thisval.type != ECC_VALTYPE_NULL)
        thisval = ecc_value_toobject(context, thisval);

    arguments = ECCNSContext.argument(context, 1);

    if(arguments.type == ECC_VALTYPE_UNDEFINED || arguments.type == ECC_VALTYPE_NULL)
        return ECCNSOperand.callFunctionVA(context, ECC_CTXOFFSET_APPLY, context->thisvalue.data.function, thisval, 0, empty_ap);
    else
    {
        if(!ecc_value_isobject(arguments))
            ECCNSContext.typeError(context, ECCNSChars.create("arguments is not an object"));

        return ECCNSOperand.callFunctionArguments(context, ECC_CTXOFFSET_APPLY, context->thisvalue.data.function, thisval, arguments.data.object);
    }
}

static eccvalue_t objfunctionfn_call(eccstate_t* context)
{
    eccobject_t arguments;

    ECCNSContext.assertThisType(context, ECC_VALTYPE_FUNCTION);

    context->strictMode = context->parent->strictMode;

    arguments = *context->environment->hashmap[2].value.data.object;

    if(arguments.elementCount)
    {
        eccvalue_t thisval = ECCNSContext.argument(context, 0);
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

        return ECCNSOperand.callFunctionArguments(context, ECC_CTXOFFSET_CALL, context->thisvalue.data.function, thisval, &arguments);
    }
    else
        return ECCNSOperand.callFunctionVA(context, ECC_CTXOFFSET_CALL, context->thisvalue.data.function, ECCValConstUndefined, 0, empty_ap);
}

static eccvalue_t objfunctionfn_bindCall(eccstate_t* context)
{
    eccobjfunction_t* function;
    eccobject_t* arguments;
    uint16_t count, length;

    ECCNSContext.assertThisType(context, ECC_VALTYPE_FUNCTION);

    context->strictMode = context->parent->strictMode;

    function = context->thisvalue.data.function;

    count = ECCNSContext.argumentCount(context);
    length = (function->environment.elementCount - 1) + count;
    arguments = ECCNSArray.createSized(length);

    memcpy(arguments->element, function->environment.element + 1, sizeof(*arguments->element) * (function->environment.elementCount - 1));
    memcpy(arguments->element + (function->environment.elementCount - 1), context->environment->hashmap[2].value.data.object->element,
           sizeof(*arguments->element) * (context->environment->hashmap[2].value.data.object->elementCount));

    return ECCNSOperand.callFunctionArguments(context, 0, context->thisvalue.data.function->pair, function->environment.element[0].value, arguments);
}

static eccvalue_t objfunctionfn_bind(eccstate_t* context)
{
    eccobjfunction_t* function;
    uint16_t index, count;
    int parameterCount = 0;

    ECCNSContext.assertThisType(context, ECC_VALTYPE_FUNCTION);

    count = ECCNSContext.argumentCount(context);
    parameterCount = context->thisvalue.data.function->parameterCount - (count > 1 ? count - 1 : 0);
    function = nsfunctionfn_createWithNative(objfunctionfn_bindCall, parameterCount > 0 ? parameterCount : 0);

    ECCNSObject.resizeElement(&function->environment, count ? count : 1);
    if(count)
        for(index = 0; index < count; ++index)
            function->environment.element[index].value = ECCNSContext.argument(context, index);
    else
        function->environment.element[0].value = ECCValConstUndefined;

    function->pair = context->thisvalue.data.function;
    function->boundThis = ecc_value_function(function);
    function->flags |= ECC_SCRIPTFUNCFLAG_NEEDARGUMENTS | ECC_SCRIPTFUNCFLAG_USEBOUNDTHIS;

    return ecc_value_function(function);
}

static eccvalue_t objfunctionfn_prototypeConstructor(eccstate_t* context)
{
    (void)context;
    return ECCValConstUndefined;
}

static eccvalue_t objfunctionfn_constructor(eccstate_t* context)
{
    int argumentCount;

    argumentCount = ECCNSContext.argumentCount(context);

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
        subContext.environment = ECCNSContext.environmentRoot(context->parent);

        ECCNSChars.beginAppend(&chars);
        ECCNSChars.append(&chars, "(function (");
        if(argumentCount)
            for(index = 0; index < argumentCount; ++index)
            {
                if(index == argumentCount - 1)
                    ECCNSChars.append(&chars, "){");

                value = ecc_value_tostring(context, ECCNSContext.argument(context, index));
                ECCNSChars.append(&chars, "%.*s", ecc_value_stringlength(&value), ecc_value_stringbytes(&value));

                if(index < argumentCount - 2)
                    ECCNSChars.append(&chars, ",");
            }
        else
            ECCNSChars.append(&chars, "){");

        ECCNSChars.append(&chars, "})");

        value = ECCNSChars.endAppend(&chars);
        input = ECCNSInput.createFromBytes(ecc_value_stringbytes(&value), ecc_value_stringlength(&value), "(Function)");
        ECCNSContext.setTextIndex(context, ECC_CTXINDEXTYPE_NO);
        ecc_script_evalinputwithcontext(context->ecc, input, &subContext);
    }

    return context->ecc->result;
}

// MARK: - Methods

static void nsfunctionfn_setup()
{
    const eccvalflag_t h = ECC_VALFLAG_HIDDEN;

    ECCNSFunction.setupBuiltinObject(&ECC_CtorFunc_Function, objfunctionfn_constructor, -1, &ECC_Prototype_Function,
                                          ecc_value_function(nsfunctionfn_createWithNative(objfunctionfn_prototypeConstructor, 0)), &ECC_Type_Function);

    ECC_CtorFunc_Function->object.prototype = ECC_Prototype_Function;

    ECCNSFunction.addToObject(ECC_Prototype_Function, "toString", objfunctionfn_toString, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_Function, "apply", objfunctionfn_apply, 2, h);
    ECCNSFunction.addToObject(ECC_Prototype_Function, "call", objfunctionfn_call, -1, h);
    ECCNSFunction.addToObject(ECC_Prototype_Function, "bind", objfunctionfn_bind, -1, h);
}

static void nsfunctionfn_teardown(void)
{
    ECC_Prototype_Function = NULL;
    ECC_CtorFunc_Function = NULL;
}

static eccobjfunction_t* nsfunctionfn_create(eccobject_t* environment)
{
    return nsfunctionfn_createSized(environment, 8);
}

static eccobjfunction_t* nsfunctionfn_createSized(eccobject_t* environment, uint32_t size)
{
    eccobjfunction_t* self = (eccobjfunction_t*)malloc(sizeof(*self));
    ECCNSMemoryPool.addFunction(self);

    *self = ECCNSFunction.identity;

    ECCNSObject.initialize(&self->object, ECC_Prototype_Function);
    ECCNSObject.initializeSized(&self->environment, environment, size);

    return self;
}

static eccobjfunction_t* nsfunctionfn_createWithNative(const eccnativefuncptr_t native, int parameterCount)
{
    eccobjfunction_t* self = NULL;

    if(parameterCount < 0)
    {
        self = nsfunctionfn_createSized(NULL, 3);
        self->flags |= ECC_SCRIPTFUNCFLAG_NEEDARGUMENTS;
    }
    else
    {
        self = nsfunctionfn_createSized(NULL, 3 + parameterCount);
        self->parameterCount = parameterCount;
    }

    self->environment.hashmapCount = self->environment.hashmapCapacity;
    self->oplist = ECCNSOpList.create(native, ECCValConstUndefined, ECC_ConstString_NativeCode);
    self->text = ECC_ConstString_NativeCode;

    ECCNSObject.addMember(&self->object, ECC_ConstKey_length, ecc_value_integer(abs(parameterCount)), ECC_VALFLAG_READONLY | ECC_VALFLAG_HIDDEN | ECC_VALFLAG_SEALED);

    return self;
}

static eccobjfunction_t* nsfunctionfn_copy(eccobjfunction_t* original)
{
    eccobjfunction_t* self = (eccobjfunction_t*)malloc(sizeof(*self));
    size_t byteSize;

    assert(original);
    ECCNSMemoryPool.addObject(&self->object);

    *self = *original;

    byteSize = sizeof(*self->object.hashmap) * self->object.hashmapCapacity;
    self->object.hashmap = (ecchashmap_t*)malloc(byteSize);
    memcpy(self->object.hashmap, original->object.hashmap, byteSize);

    return self;
}

static void nsfunctionfn_destroy(eccobjfunction_t* self)
{
    assert(self);

    ECCNSObject.finalize(&self->object);
    ECCNSObject.finalize(&self->environment);

    if(self->oplist)
        ECCNSOpList.destroy(self->oplist), self->oplist = NULL;

    free(self), self = NULL;
}

static void nsfunctionfn_addMember(eccobjfunction_t* self, const char* name, eccvalue_t value, int flags)
{
    assert(self);

    if(value.type == ECC_VALTYPE_FUNCTION)
        value.data.function->name = name;

    ECCNSObject.addMember(&self->object, ECCNSKey.makeWithCString(name), value, flags);
}

static eccobjfunction_t* nsfunctionfn_addMethod(eccobjfunction_t* self, const char* name, const eccnativefuncptr_t native, int parameterCount, int flags)
{
    assert(self);

    return nsfunctionfn_addToObject(&self->object, name, native, parameterCount, flags);
}

static void nsfunctionfn_addValue(eccobjfunction_t* self, const char* name, eccvalue_t value, int flags)
{
    assert(self);

    if(value.type == ECC_VALTYPE_FUNCTION)
        value.data.function->name = name;

    ECCNSObject.addMember(&self->environment, ECCNSKey.makeWithCString(name), value, flags);
}

static eccobjfunction_t* nsfunctionfn_addFunction(eccobjfunction_t* self, const char* name, const eccnativefuncptr_t native, int parameterCount, int flags)
{
    assert(self);

    return nsfunctionfn_addToObject(&self->environment, name, native, parameterCount, flags);
}

static eccobjfunction_t* nsfunctionfn_addToObject(eccobject_t* object, const char* name, const eccnativefuncptr_t native, int parameterCount, int flags)
{
    eccobjfunction_t* function;

    assert(object);

    function = nsfunctionfn_createWithNative(native, parameterCount);
    function->name = name;

    ECCNSObject.addMember(object, ECCNSKey.makeWithCString(name), ecc_value_function(function), flags);

    return function;
}

static void nsfunctionfn_linkPrototype(eccobjfunction_t* self, eccvalue_t prototype, int flags)
{
    assert(self);

    ECCNSObject.addMember(prototype.data.object, ECC_ConstKey_constructor, ecc_value_function(self), ECC_VALFLAG_HIDDEN);
    ECCNSObject.addMember(&self->object, ECC_ConstKey_prototype, prototype, flags);
}

void nsfunctionfn_setupBuiltinObject(eccobjfunction_t** constructor,
                        const eccnativefuncptr_t native,
                        int parameterCount,
                        eccobject_t** prototype,
                        eccvalue_t prototypeValue,
                        const eccobjinterntype_t* type)
{
    eccobjfunction_t* function = nsfunctionfn_createWithNative(native, parameterCount);

    if(prototype)
    {
        eccobject_t* object = prototypeValue.data.object;
        object->type = type;

        if(!object->prototype)
            object->prototype = ECC_Prototype_Object;

        *prototype = object;
    }

    nsfunctionfn_linkPrototype(function, prototypeValue, ECC_VALFLAG_READONLY | ECC_VALFLAG_HIDDEN | ECC_VALFLAG_SEALED);
    *constructor = function;
}

static eccvalue_t nsfunctionfn_accessor(const eccnativefuncptr_t getter, const eccnativefuncptr_t setter)
{
    eccvalue_t value;
    eccobjfunction_t *getterFunction = NULL, *setterFunction = NULL;
    if(setter)
        setterFunction = ECCNSFunction.createWithNative(setter, 1);

    if(getter)
    {
        getterFunction = ECCNSFunction.createWithNative(getter, 0);
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
