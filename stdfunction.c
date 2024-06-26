//
//  function.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"

static void capture(eccobject_t *object);
static void mark(eccobject_t *object);
static eccvalue_t toChars(eccstate_t *context, eccvalue_t value);
static eccvalue_t objfunctionfn_toString(eccstate_t *context);
static eccvalue_t objfunctionfn_apply(eccstate_t *context);
static eccvalue_t objfunctionfn_call(eccstate_t *context);
static eccvalue_t objfunctionfn_bindCall(eccstate_t *context);
static eccvalue_t objfunctionfn_bind(eccstate_t *context);
static eccvalue_t objfunctionfn_prototypeConstructor(eccstate_t *context);
static eccvalue_t objfunctionfn_constructor(eccstate_t *context);
static void setup(void);
static void teardown(void);
static eccobjscriptfunction_t *create(eccobject_t *environment);
static eccobjscriptfunction_t *createSized(eccobject_t *environment, uint32_t size);
static eccobjscriptfunction_t *createWithNative(const eccnativefuncptr_t native, int parameterCount);
static eccobjscriptfunction_t *copy(eccobjscriptfunction_t *original);
static void destroy(eccobjscriptfunction_t *self);
static void addMember(eccobjscriptfunction_t *self, const char *name, eccvalue_t value, eccvalflag_t flags);
static eccobjscriptfunction_t *addMethod(eccobjscriptfunction_t *self, const char *name, const eccnativefuncptr_t native, int parameterCount, eccvalflag_t flags);
static void addValue(eccobjscriptfunction_t *self, const char *name, eccvalue_t value, eccvalflag_t flags);
static eccobjscriptfunction_t *addFunction(eccobjscriptfunction_t *self, const char *name, const eccnativefuncptr_t native, int parameterCount, eccvalflag_t flags);
static eccobjscriptfunction_t *addToObject(eccobject_t *object, const char *name, const eccnativefuncptr_t native, int parameterCount, eccvalflag_t flags);
static void linkPrototype(eccobjscriptfunction_t *self, eccvalue_t prototype, eccvalflag_t flags);
static void setupBuiltinObject(eccobjscriptfunction_t **constructor, const eccnativefuncptr_t native, int parameterCount, eccobject_t **prototype, eccvalue_t prototypeValue, const eccobjinterntype_t *type);
static eccvalue_t accessor(const eccnativefuncptr_t getter, const eccnativefuncptr_t setter);


const struct eccpseudonsfunction_t ECCNSFunction = {
    setup,     teardown,    create,      createSized,   createWithNative,   copy,     destroy, addMember, addValue,
    addMethod, addFunction, addToObject, linkPrototype, setupBuiltinObject, accessor,
    {}
};

// MARK: - Private

static va_list empty_ap;

eccobject_t* ECC_Prototype_Function = NULL;
eccobjscriptfunction_t* ECC_CtorFunc_Function = NULL;

const eccobjinterntype_t ECC_Type_Function = {
    .text = &ECC_ConstString_FunctionType, .mark = mark, .capture = capture,
    /* XXX: don't finalize */
};

static void capture(eccobject_t* object)
{
    eccobjscriptfunction_t* self = (eccobjscriptfunction_t*)object;

    if(self->refObject)
        ++self->refObject->referenceCount;

    if(self->pair)
        ++self->pair->object.referenceCount;
}

static void mark(eccobject_t* object)
{
    eccobjscriptfunction_t* self = (eccobjscriptfunction_t*)object;

    ECCNSMemoryPool.markObject(&self->environment);

    if(self->refObject)
        ECCNSMemoryPool.markObject(self->refObject);

    if(self->pair)
        ECCNSMemoryPool.markObject(&self->pair->object);
}

// MARK: - Static Members

static eccvalue_t toChars(eccstate_t* context, eccvalue_t value)
{
    eccobjscriptfunction_t* self;
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

    return toChars(context, context->thisvalue);
}

static eccvalue_t objfunctionfn_apply(eccstate_t* context)
{
    eccvalue_t thisval, arguments;

    ECCNSContext.assertThisType(context, ECC_VALTYPE_FUNCTION);

    context->strictMode = context->parent->strictMode;

    thisval = ECCNSContext.argument(context, 0);
    if(thisval.type != ECC_VALTYPE_UNDEFINED && thisval.type != ECC_VALTYPE_NULL)
        thisval = ECCNSValue.toObject(context, thisval);

    arguments = ECCNSContext.argument(context, 1);

    if(arguments.type == ECC_VALTYPE_UNDEFINED || arguments.type == ECC_VALTYPE_NULL)
        return ECCNSOperand.callFunctionVA(context, ECC_CTXOFFSET_APPLY, context->thisvalue.data.function, thisval, 0, empty_ap);
    else
    {
        if(!ECCNSValue.isObject(arguments))
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
            thisval = ECCNSValue.toObject(context, thisval);

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
    eccobjscriptfunction_t* function;
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
    eccobjscriptfunction_t* function;
    uint16_t index, count;
    int parameterCount = 0;

    ECCNSContext.assertThisType(context, ECC_VALTYPE_FUNCTION);

    count = ECCNSContext.argumentCount(context);
    parameterCount = context->thisvalue.data.function->parameterCount - (count > 1 ? count - 1 : 0);
    function = createWithNative(objfunctionfn_bindCall, parameterCount > 0 ? parameterCount : 0);

    ECCNSObject.resizeElement(&function->environment, count ? count : 1);
    if(count)
        for(index = 0; index < count; ++index)
            function->environment.element[index].value = ECCNSContext.argument(context, index);
    else
        function->environment.element[0].value = ECCValConstUndefined;

    function->pair = context->thisvalue.data.function;
    function->boundThis = ECCNSValue.function(function);
    function->flags |= ECC_SCRIPTFUNCFLAG_NEEDARGUMENTS | ECC_SCRIPTFUNCFLAG_USEBOUNDTHIS;

    return ECCNSValue.function(function);
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
        eccstate_t subContext = {
            .parent = context,
            .thisvalue = ECCNSValue.object(&context->ecc->global->environment),
            .ecc = context->ecc,
            .depth = context->depth + 1,
            .environment = ECCNSContext.environmentRoot(context->parent),
        };

        ECCNSChars.beginAppend(&chars);
        ECCNSChars.append(&chars, "(function (");
        if(argumentCount)
            for(index = 0; index < argumentCount; ++index)
            {
                if(index == argumentCount - 1)
                    ECCNSChars.append(&chars, "){");

                value = ECCNSValue.toString(context, ECCNSContext.argument(context, index));
                ECCNSChars.append(&chars, "%.*s", ECCNSValue.stringLength(&value), ECCNSValue.stringBytes(&value));

                if(index < argumentCount - 2)
                    ECCNSChars.append(&chars, ",");
            }
        else
            ECCNSChars.append(&chars, "){");

        ECCNSChars.append(&chars, "})");

        value = ECCNSChars.endAppend(&chars);
        input = ECCNSInput.createFromBytes(ECCNSValue.stringBytes(&value), ECCNSValue.stringLength(&value), "(Function)");
        ECCNSContext.setTextIndex(context, ECC_CTXINDEXTYPE_NO);
        ECCNSScript.evalInputWithContext(context->ecc, input, &subContext);
    }

    return context->ecc->result;
}

// MARK: - Methods

static void setup()
{
    const eccvalflag_t h = ECC_VALFLAG_HIDDEN;

    ECCNSFunction.setupBuiltinObject(&ECC_CtorFunc_Function, objfunctionfn_constructor, -1, &ECC_Prototype_Function,
                                          ECCNSValue.function(createWithNative(objfunctionfn_prototypeConstructor, 0)), &ECC_Type_Function);

    ECC_CtorFunc_Function->object.prototype = ECC_Prototype_Function;

    ECCNSFunction.addToObject(ECC_Prototype_Function, "toString", objfunctionfn_toString, 0, h);
    ECCNSFunction.addToObject(ECC_Prototype_Function, "apply", objfunctionfn_apply, 2, h);
    ECCNSFunction.addToObject(ECC_Prototype_Function, "call", objfunctionfn_call, -1, h);
    ECCNSFunction.addToObject(ECC_Prototype_Function, "bind", objfunctionfn_bind, -1, h);
}

static void teardown(void)
{
    ECC_Prototype_Function = NULL;
    ECC_CtorFunc_Function = NULL;
}

static eccobjscriptfunction_t* create(eccobject_t* environment)
{
    return createSized(environment, 8);
}

static eccobjscriptfunction_t* createSized(eccobject_t* environment, uint32_t size)
{
    eccobjscriptfunction_t* self = malloc(sizeof(*self));
    ECCNSMemoryPool.addFunction(self);

    *self = ECCNSFunction.identity;

    ECCNSObject.initialize(&self->object, ECC_Prototype_Function);
    ECCNSObject.initializeSized(&self->environment, environment, size);

    return self;
}

static eccobjscriptfunction_t* createWithNative(const eccnativefuncptr_t native, int parameterCount)
{
    eccobjscriptfunction_t* self = NULL;

    if(parameterCount < 0)
    {
        self = createSized(NULL, 3);
        self->flags |= ECC_SCRIPTFUNCFLAG_NEEDARGUMENTS;
    }
    else
    {
        self = createSized(NULL, 3 + parameterCount);
        self->parameterCount = parameterCount;
    }

    self->environment.hashmapCount = self->environment.hashmapCapacity;
    self->oplist = ECCNSOpList.create(native, ECCValConstUndefined, ECC_ConstString_NativeCode);
    self->text = ECC_ConstString_NativeCode;

    ECCNSObject.addMember(&self->object, ECC_ConstKey_length, ECCNSValue.integer(abs(parameterCount)), ECC_VALFLAG_READONLY | ECC_VALFLAG_HIDDEN | ECC_VALFLAG_SEALED);

    return self;
}

static eccobjscriptfunction_t* copy(eccobjscriptfunction_t* original)
{
    eccobjscriptfunction_t* self = malloc(sizeof(*self));
    size_t byteSize;

    assert(original);
    ECCNSMemoryPool.addObject(&self->object);

    *self = *original;

    byteSize = sizeof(*self->object.hashmap) * self->object.hashmapCapacity;
    self->object.hashmap = malloc(byteSize);
    memcpy(self->object.hashmap, original->object.hashmap, byteSize);

    return self;
}

static void destroy(eccobjscriptfunction_t* self)
{
    assert(self);

    ECCNSObject.finalize(&self->object);
    ECCNSObject.finalize(&self->environment);

    if(self->oplist)
        ECCNSOpList.destroy(self->oplist), self->oplist = NULL;

    free(self), self = NULL;
}

static void addMember(eccobjscriptfunction_t* self, const char* name, eccvalue_t value, eccvalflag_t flags)
{
    assert(self);

    if(value.type == ECC_VALTYPE_FUNCTION)
        value.data.function->name = name;

    ECCNSObject.addMember(&self->object, ECCNSKey.makeWithCString(name), value, flags);
}

static eccobjscriptfunction_t* addMethod(eccobjscriptfunction_t* self, const char* name, const eccnativefuncptr_t native, int parameterCount, eccvalflag_t flags)
{
    assert(self);

    return addToObject(&self->object, name, native, parameterCount, flags);
}

static void addValue(eccobjscriptfunction_t* self, const char* name, eccvalue_t value, eccvalflag_t flags)
{
    assert(self);

    if(value.type == ECC_VALTYPE_FUNCTION)
        value.data.function->name = name;

    ECCNSObject.addMember(&self->environment, ECCNSKey.makeWithCString(name), value, flags);
}

static eccobjscriptfunction_t* addFunction(eccobjscriptfunction_t* self, const char* name, const eccnativefuncptr_t native, int parameterCount, eccvalflag_t flags)
{
    assert(self);

    return addToObject(&self->environment, name, native, parameterCount, flags);
}

static eccobjscriptfunction_t* addToObject(eccobject_t* object, const char* name, const eccnativefuncptr_t native, int parameterCount, eccvalflag_t flags)
{
    eccobjscriptfunction_t* function;

    assert(object);

    function = createWithNative(native, parameterCount);
    function->name = name;

    ECCNSObject.addMember(object, ECCNSKey.makeWithCString(name), ECCNSValue.function(function), flags);

    return function;
}

static void linkPrototype(eccobjscriptfunction_t* self, eccvalue_t prototype, eccvalflag_t flags)
{
    assert(self);

    ECCNSObject.addMember(prototype.data.object, ECC_ConstKey_constructor, ECCNSValue.function(self), ECC_VALFLAG_HIDDEN);
    ECCNSObject.addMember(&self->object, ECC_ConstKey_prototype, prototype, flags);
}

void setupBuiltinObject(eccobjscriptfunction_t** constructor,
                        const eccnativefuncptr_t native,
                        int parameterCount,
                        eccobject_t** prototype,
                        eccvalue_t prototypeValue,
                        const eccobjinterntype_t* type)
{
    eccobjscriptfunction_t* function = createWithNative(native, parameterCount);

    if(prototype)
    {
        eccobject_t* object = prototypeValue.data.object;
        object->type = type;

        if(!object->prototype)
            object->prototype = ECC_Prototype_Object;

        *prototype = object;
    }

    linkPrototype(function, prototypeValue, ECC_VALFLAG_READONLY | ECC_VALFLAG_HIDDEN | ECC_VALFLAG_SEALED);
    *constructor = function;
}

static eccvalue_t accessor(const eccnativefuncptr_t getter, const eccnativefuncptr_t setter)
{
    eccvalue_t value;
    eccobjscriptfunction_t *getterFunction = NULL, *setterFunction = NULL;
    if(setter)
        setterFunction = ECCNSFunction.createWithNative(setter, 1);

    if(getter)
    {
        getterFunction = ECCNSFunction.createWithNative(getter, 0);
        getterFunction->pair = setterFunction;
        value = ECCNSValue.function(getterFunction);
        value.flags |= ECC_VALFLAG_GETTER;
    }
    else if(setter)
    {
        value = ECCNSValue.function(setterFunction);
        value.flags |= ECC_VALFLAG_SETTER;
    }
    else
        value = ECCValConstUndefined;

    return value;
}
