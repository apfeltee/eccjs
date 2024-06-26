//
//  function.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"

static void setup(void);
static void teardown(void);
static eccobjscriptfunction_t* create(eccobject_t* environment);
static eccobjscriptfunction_t* createSized(eccobject_t* environment, uint32_t size);
static eccobjscriptfunction_t* createWithNative(const eccnativefuncptr_t native, int parameterCount);
static eccobjscriptfunction_t* copy(eccobjscriptfunction_t* original);
static void destroy(eccobjscriptfunction_t*);
static void addMember(eccobjscriptfunction_t*, const char* name, eccvalue_t value, eccvalflag_t);
static void addValue(eccobjscriptfunction_t*, const char* name, eccvalue_t value, eccvalflag_t);
static eccobjscriptfunction_t* addMethod(eccobjscriptfunction_t*, const char* name, const eccnativefuncptr_t native, int argumentCount, eccvalflag_t);
static eccobjscriptfunction_t* addFunction(eccobjscriptfunction_t*, const char* name, const eccnativefuncptr_t native, int argumentCount, eccvalflag_t);
static eccobjscriptfunction_t* addToObject(eccobject_t* object, const char* name, const eccnativefuncptr_t native, int parameterCount, eccvalflag_t);
static void linkPrototype(eccobjscriptfunction_t*, eccvalue_t prototype, eccvalflag_t);
static void
setupBuiltinObject(eccobjscriptfunction_t**, const eccnativefuncptr_t, int parameterCount, eccobject_t**, eccvalue_t prototype, const eccobjinterntype_t* type);
static eccvalue_t accessor(const eccnativefuncptr_t getter, const eccnativefuncptr_t setter);
const struct eccpseudonsfunction_t io_libecc_Function = {
    setup,     teardown,    create,      createSized,   createWithNative,   copy,     destroy, addMember, addValue,
    addMethod, addFunction, addToObject, linkPrototype, setupBuiltinObject, accessor,
    {}
};

// MARK: - Private

static va_list empty_ap;

static void mark(eccobject_t* object);
static void capture(eccobject_t* object);

eccobject_t* io_libecc_function_prototype = NULL;
eccobjscriptfunction_t* io_libecc_function_constructor = NULL;

const eccobjinterntype_t ECCObjTypeFunction = {
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

    io_libecc_Pool.markObject(&self->environment);

    if(self->refObject)
        io_libecc_Pool.markObject(self->refObject);

    if(self->pair)
        io_libecc_Pool.markObject(&self->pair->object);
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
    io_libecc_Chars.beginAppend(&chars);

    io_libecc_Chars.append(&chars, "function %s", self->name ? self->name : "anonymous");

    if(self->text.bytes == ECC_ConstString_NativeCode.bytes)
        io_libecc_Chars.append(&chars, "() [native code]");
    else
        io_libecc_Chars.append(&chars, "%.*s", self->text.length, self->text.bytes);

    return io_libecc_Chars.endAppend(&chars);
}

static eccvalue_t toString(eccstate_t* context)
{
    ECCNSContext.assertThisType(context, ECC_VALTYPE_FUNCTION);

    return toChars(context, context->thisvalue);
}

static eccvalue_t apply(eccstate_t* context)
{
    eccvalue_t thisval, arguments;

    ECCNSContext.assertThisType(context, ECC_VALTYPE_FUNCTION);

    context->strictMode = context->parent->strictMode;

    thisval = ECCNSContext.argument(context, 0);
    if(thisval.type != ECC_VALTYPE_UNDEFINED && thisval.type != ECC_VALTYPE_NULL)
        thisval = ECCNSValue.toObject(context, thisval);

    arguments = ECCNSContext.argument(context, 1);

    if(arguments.type == ECC_VALTYPE_UNDEFINED || arguments.type == ECC_VALTYPE_NULL)
        return io_libecc_Op.callFunctionVA(context, ECC_CTXOFFSET_APPLY, context->thisvalue.data.function, thisval, 0, empty_ap);
    else
    {
        if(!ECCNSValue.isObject(arguments))
            ECCNSContext.typeError(context, io_libecc_Chars.create("arguments is not an object"));

        return io_libecc_Op.callFunctionArguments(context, ECC_CTXOFFSET_APPLY, context->thisvalue.data.function, thisval, arguments.data.object);
    }
}

static eccvalue_t call(eccstate_t* context)
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

        return io_libecc_Op.callFunctionArguments(context, ECC_CTXOFFSET_CALL, context->thisvalue.data.function, thisval, &arguments);
    }
    else
        return io_libecc_Op.callFunctionVA(context, ECC_CTXOFFSET_CALL, context->thisvalue.data.function, ECCValConstUndefined, 0, empty_ap);
}

static eccvalue_t bindCall(eccstate_t* context)
{
    eccobjscriptfunction_t* function;
    eccobject_t* arguments;
    uint16_t count, length;

    ECCNSContext.assertThisType(context, ECC_VALTYPE_FUNCTION);

    context->strictMode = context->parent->strictMode;

    function = context->thisvalue.data.function;

    count = ECCNSContext.argumentCount(context);
    length = (function->environment.elementCount - 1) + count;
    arguments = io_libecc_Array.createSized(length);

    memcpy(arguments->element, function->environment.element + 1, sizeof(*arguments->element) * (function->environment.elementCount - 1));
    memcpy(arguments->element + (function->environment.elementCount - 1), context->environment->hashmap[2].value.data.object->element,
           sizeof(*arguments->element) * (context->environment->hashmap[2].value.data.object->elementCount));

    return io_libecc_Op.callFunctionArguments(context, 0, context->thisvalue.data.function->pair, function->environment.element[0].elemvalue, arguments);
}

static eccvalue_t bind(eccstate_t* context)
{
    eccobjscriptfunction_t* function;
    uint16_t index, count;
    int parameterCount = 0;

    ECCNSContext.assertThisType(context, ECC_VALTYPE_FUNCTION);

    count = ECCNSContext.argumentCount(context);
    parameterCount = context->thisvalue.data.function->parameterCount - (count > 1 ? count - 1 : 0);
    function = createWithNative(bindCall, parameterCount > 0 ? parameterCount : 0);

    ECCNSObject.resizeElement(&function->environment, count ? count : 1);
    if(count)
        for(index = 0; index < count; ++index)
            function->environment.element[index].elemvalue = ECCNSContext.argument(context, index);
    else
        function->environment.element[0].elemvalue = ECCValConstUndefined;

    function->pair = context->thisvalue.data.function;
    function->boundThis = ECCNSValue.function(function);
    function->flags |= ECC_SCRIPTFUNCFLAG_NEEDARGUMENTS | ECC_SCRIPTFUNCFLAG_USEBOUNDTHIS;

    return ECCNSValue.function(function);
}

static eccvalue_t prototypeConstructor(eccstate_t* context)
{
    (void)context;
    return ECCValConstUndefined;
}

static eccvalue_t constructor(eccstate_t* context)
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

        io_libecc_Chars.beginAppend(&chars);
        io_libecc_Chars.append(&chars, "(function (");
        if(argumentCount)
            for(index = 0; index < argumentCount; ++index)
            {
                if(index == argumentCount - 1)
                    io_libecc_Chars.append(&chars, "){");

                value = ECCNSValue.toString(context, ECCNSContext.argument(context, index));
                io_libecc_Chars.append(&chars, "%.*s", ECCNSValue.stringLength(&value), ECCNSValue.stringBytes(&value));

                if(index < argumentCount - 2)
                    io_libecc_Chars.append(&chars, ",");
            }
        else
            io_libecc_Chars.append(&chars, "){");

        io_libecc_Chars.append(&chars, "})");

        value = io_libecc_Chars.endAppend(&chars);
        input = io_libecc_Input.createFromBytes(ECCNSValue.stringBytes(&value), ECCNSValue.stringLength(&value), "(io_libecc_Function)");
        ECCNSContext.setTextIndex(context, ECC_CTXINDEXTYPE_NO);
        ECCNSScript.evalInputWithContext(context->ecc, input, &subContext);
    }

    return context->ecc->result;
}

// MARK: - Methods

void setup()
{
    const eccvalflag_t h = ECC_VALFLAG_HIDDEN;

    io_libecc_Function.setupBuiltinObject(&io_libecc_function_constructor, constructor, -1, &io_libecc_function_prototype,
                                          ECCNSValue.function(createWithNative(prototypeConstructor, 0)), &ECCObjTypeFunction);

    io_libecc_function_constructor->object.prototype = io_libecc_function_prototype;

    io_libecc_Function.addToObject(io_libecc_function_prototype, "toString", toString, 0, h);
    io_libecc_Function.addToObject(io_libecc_function_prototype, "apply", apply, 2, h);
    io_libecc_Function.addToObject(io_libecc_function_prototype, "call", call, -1, h);
    io_libecc_Function.addToObject(io_libecc_function_prototype, "bind", bind, -1, h);
}

void teardown(void)
{
    io_libecc_function_prototype = NULL;
    io_libecc_function_constructor = NULL;
}

eccobjscriptfunction_t* create(eccobject_t* environment)
{
    return createSized(environment, 8);
}

eccobjscriptfunction_t* createSized(eccobject_t* environment, uint32_t size)
{
    eccobjscriptfunction_t* self = malloc(sizeof(*self));
    io_libecc_Pool.addFunction(self);

    *self = io_libecc_Function.identity;

    ECCNSObject.initialize(&self->object, io_libecc_function_prototype);
    ECCNSObject.initializeSized(&self->environment, environment, size);

    return self;
}

eccobjscriptfunction_t* createWithNative(const eccnativefuncptr_t native, int parameterCount)
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
    self->oplist = io_libecc_OpList.create(native, ECCValConstUndefined, ECC_ConstString_NativeCode);
    self->text = ECC_ConstString_NativeCode;

    ECCNSObject.addMember(&self->object, io_libecc_key_length, ECCNSValue.integer(abs(parameterCount)), ECC_VALFLAG_READONLY | ECC_VALFLAG_HIDDEN | ECC_VALFLAG_SEALED);

    return self;
}

eccobjscriptfunction_t* copy(eccobjscriptfunction_t* original)
{
    eccobjscriptfunction_t* self = malloc(sizeof(*self));
    size_t byteSize;

    assert(original);
    io_libecc_Pool.addObject(&self->object);

    *self = *original;

    byteSize = sizeof(*self->object.hashmap) * self->object.hashmapCapacity;
    self->object.hashmap = malloc(byteSize);
    memcpy(self->object.hashmap, original->object.hashmap, byteSize);

    return self;
}

void destroy(eccobjscriptfunction_t* self)
{
    assert(self);

    ECCNSObject.finalize(&self->object);
    ECCNSObject.finalize(&self->environment);

    if(self->oplist)
        io_libecc_OpList.destroy(self->oplist), self->oplist = NULL;

    free(self), self = NULL;
}

void addMember(eccobjscriptfunction_t* self, const char* name, eccvalue_t value, eccvalflag_t flags)
{
    assert(self);

    if(value.type == ECC_VALTYPE_FUNCTION)
        value.data.function->name = name;

    ECCNSObject.addMember(&self->object, io_libecc_Key.makeWithCString(name), value, flags);
}

eccobjscriptfunction_t* addMethod(eccobjscriptfunction_t* self, const char* name, const eccnativefuncptr_t native, int parameterCount, eccvalflag_t flags)
{
    assert(self);

    return addToObject(&self->object, name, native, parameterCount, flags);
}

void addValue(eccobjscriptfunction_t* self, const char* name, eccvalue_t value, eccvalflag_t flags)
{
    assert(self);

    if(value.type == ECC_VALTYPE_FUNCTION)
        value.data.function->name = name;

    ECCNSObject.addMember(&self->environment, io_libecc_Key.makeWithCString(name), value, flags);
}

eccobjscriptfunction_t* addFunction(eccobjscriptfunction_t* self, const char* name, const eccnativefuncptr_t native, int parameterCount, eccvalflag_t flags)
{
    assert(self);

    return addToObject(&self->environment, name, native, parameterCount, flags);
}

eccobjscriptfunction_t* addToObject(eccobject_t* object, const char* name, const eccnativefuncptr_t native, int parameterCount, eccvalflag_t flags)
{
    eccobjscriptfunction_t* function;

    assert(object);

    function = createWithNative(native, parameterCount);
    function->name = name;

    ECCNSObject.addMember(object, io_libecc_Key.makeWithCString(name), ECCNSValue.function(function), flags);

    return function;
}

void linkPrototype(eccobjscriptfunction_t* self, eccvalue_t prototype, eccvalflag_t flags)
{
    assert(self);

    ECCNSObject.addMember(prototype.data.object, io_libecc_key_constructor, ECCNSValue.function(self), ECC_VALFLAG_HIDDEN);
    ECCNSObject.addMember(&self->object, io_libecc_key_prototype, prototype, flags);
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
            object->prototype = io_libecc_object_prototype;

        *prototype = object;
    }

    linkPrototype(function, prototypeValue, ECC_VALFLAG_READONLY | ECC_VALFLAG_HIDDEN | ECC_VALFLAG_SEALED);
    *constructor = function;
}

eccvalue_t accessor(const eccnativefuncptr_t getter, const eccnativefuncptr_t setter)
{
    eccvalue_t value;
    eccobjscriptfunction_t *getterFunction = NULL, *setterFunction = NULL;
    if(setter)
        setterFunction = io_libecc_Function.createWithNative(setter, 1);

    if(getter)
    {
        getterFunction = io_libecc_Function.createWithNative(getter, 0);
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
