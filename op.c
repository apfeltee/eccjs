//
//  op.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"

/*
  ecc_oper_retain
  ecc_oper_release
  ecc_oper_callops
  ecc_oper_localref
  ecc_oper_callvalue
  ecc_oper_intlessthan
  ecc_oper_intgreaterthan
  ecc_oper_nextopvalue
  ecc_oper_callfunction
  ecc_oper_prepareobject
  ecc_oper_callopsrelease
  ecc_oper_replacerefvalue
  ecc_oper_iterateintegerref
  ecc_oper_intlessequal
  ecc_oper_intgreaterequal
  ecc_oper_prepareobjectproperty
  ecc_oper_populateenvwithva
  ecc_oper_populateenvwithops
  ecc_oper_testintegerwontofpos
  ecc_oper_testintwontofneg
  ecc_oper_makeenvwithargs
  ecc_oper_makeenvandargswithva
  ecc_oper_makeenvandargswithops
  ecc_oper_makestackenvandargswithops
*/

#define ECC_OPS_DEBUG 0
#define opmac_next() (++context->ops)->native(context)
#define opmac_value() (context->ops)->value
#define opmac_text(O) &(context->ops + O)->text


#if _MSC_VER
    #define trap() __debugbreak()
#elif __GNUC__
    #if __i386__ || __x86_64__
        #define trap() __asm__("int $3")
    #elif __APPLE__
        #include <sys/syscall.h>
        #if __arm64__
            #define trap()                                                                                                           \
                __asm__ __volatile__("mov w0,%w0\n mov w1,%w1\n mov w16,%w2\n svc #128" ::"r"(getpid()), "r"(SIGTRAP), "r"(SYS_kill) \
                                     : "x0", "x1", "x16", "cc")
        #elif __arm__
            #define trap()                                                                                                           \
                __asm__ __volatile__("mov r0, %0\n mov r1, %1\n mov r12, %2\n swi #128" ::"r"(getpid()), "r"(SIGTRAP), "r"(SYS_kill) \
                                     : "r0", "r1", "r12", "cc")
        #endif
    #endif
#endif

#ifndef trap
    #if defined(SIGTRAP)
        #define trap() raise(SIGTRAP)
    #else
        #define trap() raise(SIGINT)
    #endif
#endif


static int g_ops_debugging = 0;

eccvalue_t ecc_oper_dotrapop(eccstate_t* context, int offset)
{
    const ecctextstring_t* text = opmac_text(offset);
    if(g_ops_debugging && text->bytes && text->length)
    {
        ecc_env_newline();
        ecc_context_printbacktrace(context);
        ecc_script_printtextinput(context->ecc, *text, 1);
        trap();
    }
    return opmac_next();
}

#if (ECC_OPS_DEBUG == 1)
eccvalue_t ecc_oper_trapop(eccstate_t* context, int offset)
{
    /*     gdb/lldb infos: p usage()     */
    return ecc_oper_dotrapop(context, offset);
}
#else
    #define ecc_oper_trapop(context, offset) opmac_next()
#endif


eccvalue_t ecc_oper_retain(eccvalue_t value)
{
    if(value.type == ECC_VALTYPE_CHARS)
        ++value.data.chars->referenceCount;
    if(value.type >= ECC_VALTYPE_OBJECT)
        ++value.data.object->referenceCount;

    return value;
}

eccvalue_t ecc_oper_release(eccvalue_t value)
{
    if(value.type == ECC_VALTYPE_CHARS)
        --value.data.chars->referenceCount;
    if(value.type >= ECC_VALTYPE_OBJECT)
    {
        if(value.data.object != NULL)
        {
            --value.data.object->referenceCount;
        }
    }
    return value;
}

int ecc_oper_intlessthan(int32_t a, int32_t b)
{
    return a < b;
}

int ecc_oper_intlessequal(int32_t a, int32_t b)
{
    return a <= b;
}

int ecc_oper_intgreaterthan(int32_t a, int32_t b)
{
    return a > b;
}

int ecc_oper_intgreaterequal(int32_t a, int32_t b)
{
    return a >= b;
}

int ecc_oper_testintegerwontofpos(int32_t a, int32_t positive)
{
    return a <= INT32_MAX - positive;
}

int ecc_oper_testintwontofneg(int32_t a, int32_t negative)
{
    return a >= INT32_MIN - negative;
}

// MARK: - Static Members

// MARK: - Methods

eccoperand_t ecc_oper_make(const eccnativefuncptr_t native, eccvalue_t value, ecctextstring_t text)
{
    return (eccoperand_t){ native, value, text };
}

const char* ecc_oper_tochars(const eccnativefuncptr_t native)
{
    const struct
    {
        const char* name;
        const eccnativefuncptr_t native;
    } functionList[] = {
        { "noop", ecc_oper_noop },
        { "value", ecc_oper_value },
        { "valueConstRef", ecc_oper_valueconstref },
        { "text", ecc_oper_text },
        { "function", ecc_oper_function },
        { "object", ecc_oper_object },
        { "array", ecc_oper_array },
        { "regexp", ecc_oper_regexp },
        { "this", ecc_oper_getthis },
        { "createLocalRef", ecc_oper_createlocalref },
        { "getLocalRefOrNull", ecc_oper_getlocalrefornull },
        { "getLocalRef", ecc_oper_getlocalref },
        { "getLocal", ecc_oper_getlocal },
        { "setLocal", ecc_oper_setlocal },
        { "deleteLocal", ecc_oper_deletelocal },
        { "getLocalSlotRef", ecc_oper_getlocalslotref },
        { "getLocalSlot", ecc_oper_getlocalslot },
        { "setLocalSlot", ecc_oper_setlocalslot },
        { "deleteLocalSlot", ecc_oper_deletelocalslot },
        { "getParentSlotRef", ecc_oper_getparentslotref },
        { "getParentSlot", ecc_oper_getparentslot },
        { "setParentSlot", ecc_oper_setparentslot },
        { "deleteParentSlot", ecc_oper_deleteparentslot },
        { "getMemberRef", ecc_oper_getmemberref },
        { "getMember", ecc_oper_getmember },
        { "setMember", ecc_oper_setmember },
        { "callMember", ecc_oper_callmember },
        { "deleteMember", ecc_oper_deletemember },
        { "getPropertyRef", ecc_oper_getpropertyref },
        { "getProperty", ecc_oper_getproperty },
        { "setProperty", ecc_oper_setproperty },
        { "callProperty", ecc_oper_callproperty },
        { "deleteProperty", ecc_oper_deleteproperty },
        { "pushEnvironment", ecc_oper_pushenvironment },
        { "popEnvironment", ecc_oper_popenvironment },
        { "exchange", ecc_oper_exchange },
        { "typeOf", ecc_oper_typeof },
        { "equal", ecc_oper_equal },
        { "notEqual", ecc_oper_notequal },
        { "identical", ecc_oper_identical },
        { "notIdentical", ecc_oper_notidentical },
        { "less", ecc_oper_less },
        { "lessOrEqual", ecc_oper_lessorequal },
        { "more", ecc_oper_more },
        { "moreOrEqual", ecc_oper_moreorequal },
        { "instanceOf", ecc_oper_instanceof },
        { "in", ecc_oper_in },
        { "add", ecc_oper_add },
        { "minus", ecc_oper_minus },
        { "multiply", ecc_oper_multiply },
        { "divide", ecc_oper_divide },
        { "modulo", ecc_oper_modulo },
        { "leftShift", ecc_oper_leftshift },
        { "rightShift", ecc_oper_rightshift },
        { "unsignedRightShift", ecc_oper_unsignedrightshift },
        { "bitwiseAnd", ecc_oper_bitwiseand },
        { "bitwiseXor", ecc_oper_bitwisexor },
        { "bitwiseOr", ecc_oper_bitwiseor },
        { "logicalAnd", ecc_oper_logicaland },
        { "logicalOr", ecc_oper_logicalor },
        { "positive", ecc_oper_positive },
        { "negative", ecc_oper_negative },
        { "invert", ecc_oper_invert },
        { "not", ecc_oper_logicalnot },
        { "construct", ecc_oper_construct },
        { "call", ecc_oper_call },
        { "eval", ecc_oper_eval },
        { "incrementRef", ecc_oper_incrementref },
        { "decrementRef", ecc_oper_decrementref },
        { "postIncrementRef", ecc_oper_postincrementref },
        { "postDecrementRef", ecc_oper_postdecrementref },
        { "addAssignRef", ecc_oper_addassignref },
        { "minusAssignRef", ecc_oper_minusassignref },
        { "multiplyAssignRef", ecc_oper_multiplyassignref },
        { "divideAssignRef", ecc_oper_divideassignref },
        { "moduloAssignRef", ecc_oper_moduloassignref },
        { "leftShiftAssignRef", ecc_oper_leftshiftassignref },
        { "rightShiftAssignRef", ecc_oper_rightshiftassignref },
        { "unsignedRightShiftAssignRef", ecc_oper_unsignedrightshiftassignref },
        { "bitAndAssignRef", ecc_oper_bitandassignref },
        { "bitXorAssignRef", ecc_oper_bitxorassignref },
        { "bitOrAssignRef", ecc_oper_bitorassignref },
        { "debugger", ecc_oper_debugger },
        { "try", ecc_oper_try },
        { "throw", ecc_oper_throw },
        { "with", ecc_oper_with },
        { "next", ecc_oper_next },
        { "nextIf", ecc_oper_nextif },
        { "autoreleaseExpression", ecc_oper_autoreleaseexpression },
        { "autoreleaseDiscard", ecc_oper_autoreleasediscard },
        { "expression", ecc_oper_expression },
        { "discard", ecc_oper_discard },
        { "discardN", ecc_oper_discardn },
        { "jump", ecc_oper_jump },
        { "jumpIf", ecc_oper_jumpif },
        { "jumpIfNot", ecc_oper_jumpifnot },
        { "repopulate", ecc_oper_repopulate },
        { "result", ecc_oper_result },
        { "resultVoid", ecc_oper_resultvoid },
        { "switchOp", ecc_oper_switchop },
        { "breaker", ecc_oper_breaker },
        { "iterate", ecc_oper_iterate },
        { "iterateLessRef", ecc_oper_iteratelessref },
        { "iterateMoreRef", ecc_oper_iteratemoreref },
        { "iterateLessOrEqualRef", ecc_oper_iteratelessorequalref },
        { "iterateMoreOrEqualRef", ecc_oper_iteratemoreorequalref },
        { "iterateInRef", ecc_oper_iterateinref },
    };

    int index;
    for(index = 0; index < (int)sizeof(functionList); ++index)
        if(functionList[index].native == native)
            return functionList[index].name;

    assert(0);
    return "unknow";
}


eccvalue_t ecc_oper_nextopvalue(eccstate_t* context)
{
    eccvalue_t value = opmac_next();
    value.flags &= ~(ECC_VALFLAG_READONLY | ECC_VALFLAG_HIDDEN | ECC_VALFLAG_SEALED);
    return value;
}

eccvalue_t ecc_oper_replacerefvalue(eccvalue_t* ref, eccvalue_t value)
{
    ref->data = value.data;
    ref->type = value.type;
    return value;
}

eccvalue_t ecc_oper_callops(eccstate_t* context, eccobject_t* environment)
{
    if(context->depth >= context->ecc->maximumCallDepth)
        ecc_context_rangeerror(context, ecc_charbuf_create("maximum depth exceeded"));

    //	if (!context->parent->strictMode)
    //		if (context->thisvalue.type == ECC_VALTYPE_UNDEFINED || context->thisvalue.type == ECC_VALTYPE_NULL)
    //			context->thisvalue = ecc_value_object(&context->ecc->global->environment);

    context->environment = environment;
    return context->ops->native(context);
}


eccvalue_t ecc_oper_callvalue(eccstate_t* context, eccvalue_t value, eccvalue_t thisval, int32_t argumentCount, int construct, const ecctextstring_t* textCall)
{
    eccvalue_t result;
    const ecctextstring_t* parentTextCall = context->textCall;

    if(value.type != ECC_VALTYPE_FUNCTION)
        ecc_context_typeerror(context, ecc_charbuf_create("'%.*s' is not a function", context->text->length, context->text->bytes));

    context->textCall = textCall;

    if(value.data.function->flags & ECC_SCRIPTFUNCFLAG_USEBOUNDTHIS)
        result = ecc_oper_callfunction(context, value.data.function, value.data.function->boundThis, argumentCount, construct);
    else
        result = ecc_oper_callfunction(context, value.data.function, thisval, argumentCount, construct);

    context->textCall = parentTextCall;
    return result;
}

eccvalue_t ecc_oper_callopsrelease(eccstate_t* context, eccobject_t* environment)
{
    eccvalue_t result;
    uint16_t index, count;

    result = ecc_oper_callops(context, environment);

    for(index = 2, count = environment->hashmapCount; index < count; ++index)
        ecc_oper_release(environment->hashmap[index].value);

    return result;
}

void ecc_oper_makeenvwithargs(eccobject_t* environment, eccobject_t* arguments, int32_t parameterCount)
{
    int32_t index = 0;
    int argumentCount = arguments->elementCount;

    ecc_oper_replacerefvalue(&environment->hashmap[2].value, ecc_oper_retain(ecc_value_object(arguments)));

    if(argumentCount <= parameterCount)
        for(; index < argumentCount; ++index)
            environment->hashmap[index + 3].value = ecc_oper_retain(arguments->element[index].value);
    else
    {
        for(; index < parameterCount; ++index)
            environment->hashmap[index + 3].value = ecc_oper_retain(arguments->element[index].value);
    }
}

void ecc_oper_makeenvandargswithva(eccobject_t* environment, int32_t parameterCount, int32_t argumentCount, va_list ap)
{
    int32_t index = 0;
    eccobject_t* arguments = ecc_args_createsized(argumentCount);

    ecc_oper_replacerefvalue(&environment->hashmap[2].value, ecc_oper_retain(ecc_value_object(arguments)));

    if(argumentCount <= parameterCount)
        for(; index < argumentCount; ++index)
            environment->hashmap[index + 3].value = arguments->element[index].value = ecc_oper_retain(va_arg(ap, eccvalue_t));
    else
    {
        for(; index < parameterCount; ++index)
            environment->hashmap[index + 3].value = arguments->element[index].value = ecc_oper_retain(va_arg(ap, eccvalue_t));

        for(; index < argumentCount; ++index)
            arguments->element[index].value = ecc_oper_retain(va_arg(ap, eccvalue_t));
    }
}

void ecc_oper_populateenvwithva(eccobject_t* environment, int32_t parameterCount, int32_t argumentCount, va_list ap)
{
    int32_t index = 0;
    if(argumentCount <= parameterCount)
        for(; index < argumentCount; ++index)
            environment->hashmap[index + 3].value = ecc_oper_retain(va_arg(ap, eccvalue_t));
    else
        for(; index < parameterCount; ++index)
            environment->hashmap[index + 3].value = ecc_oper_retain(va_arg(ap, eccvalue_t));
}

void ecc_oper_makestackenvandargswithops(eccstate_t* context, eccobject_t* environment, eccobject_t* arguments, int32_t parameterCount, int32_t argumentCount)
{
    int32_t index = 0;

    ecc_oper_replacerefvalue(&environment->hashmap[2].value, ecc_value_object(arguments));

    if(argumentCount <= parameterCount)
        for(; index < argumentCount; ++index)
            environment->hashmap[index + 3].value = arguments->element[index].value = ecc_oper_retain(ecc_oper_nextopvalue(context));
    else
    {
        for(; index < parameterCount; ++index)
            environment->hashmap[index + 3].value = arguments->element[index].value = ecc_oper_retain(ecc_oper_nextopvalue(context));

        for(; index < argumentCount; ++index)
            arguments->element[index].value = ecc_oper_nextopvalue(context);
    }
}

void ecc_oper_makeenvandargswithops(eccstate_t* context, eccobject_t* environment, eccobject_t* arguments, int32_t parameterCount, int32_t argumentCount)
{
    ecc_oper_makestackenvandargswithops(context, environment, arguments, parameterCount, argumentCount);

    ecc_oper_retain(ecc_value_object(arguments));

    if(argumentCount > parameterCount)
    {
        int32_t index = parameterCount;
        for(; index < argumentCount; ++index)
            ecc_oper_retain(arguments->element[index].value);
    }
}

void ecc_oper_populateenvwithops(eccstate_t* context, eccobject_t* environment, int32_t parameterCount, int32_t argumentCount)
{
    int32_t index = 0;
    if(argumentCount <= parameterCount)
        for(; index < argumentCount; ++index)
            environment->hashmap[index + 3].value = ecc_oper_retain(ecc_oper_nextopvalue(context));
    else
    {
        for(; index < parameterCount; ++index)
            environment->hashmap[index + 3].value = ecc_oper_retain(ecc_oper_nextopvalue(context));

        for(; index < argumentCount; ++index)
            opmac_next();
    }
}

eccvalue_t ecc_oper_callfunctionarguments(eccstate_t* context, int offset, eccobjfunction_t* function, eccvalue_t thisval, eccobject_t* arguments)
{
    eccstate_t subContext = {};
    subContext.ops = function->oplist->ops;
    subContext.thisvalue = thisval;
    subContext.parent = context;
    subContext.ecc = context->ecc;
    subContext.argumentOffset = offset;
    subContext.depth = context->depth + 1;
    subContext.strictMode = function->flags & ECC_SCRIPTFUNCFLAG_STRICTMODE;
    subContext.refObject = function->refObject;
    if(function->flags & ECC_SCRIPTFUNCFLAG_NEEDHEAP)
    {
        eccobject_t* environment = ecc_object_copy(&function->environment);
        if(function->flags & ECC_SCRIPTFUNCFLAG_NEEDARGUMENTS)
        {
            eccobject_t* copy = ecc_args_createsized(arguments->elementCount);
            memcpy(copy->element, arguments->element, sizeof(*copy->element) * copy->elementCount);
            arguments = copy;
        }
        ecc_oper_makeenvwithargs(environment, arguments, function->parameterCount);
        return ecc_oper_callops(&subContext, environment);
    }
    else
    {
        eccobject_t environment = function->environment;
        ecchashmap_t hashmap[function->environment.hashmapCapacity];
        memcpy(hashmap, function->environment.hashmap, sizeof(hashmap));
        environment.hashmap = hashmap;
        ecc_oper_makeenvwithargs(&environment, arguments, function->parameterCount);
        return ecc_oper_callopsrelease(&subContext, &environment);
    }
}

eccvalue_t ecc_oper_callfunctionva(eccstate_t* context, int offset, eccobjfunction_t* function, eccvalue_t thisval, int argumentCount, va_list ap)
{
    eccstate_t subContext = {};
    subContext.ops = function->oplist->ops;
    subContext.thisvalue = thisval;
    subContext.parent = context;
    subContext.ecc = context->ecc;
    subContext.argumentOffset = offset;
    subContext.depth = context->depth + 1;
    subContext.strictMode = function->flags & ECC_SCRIPTFUNCFLAG_STRICTMODE;
    subContext.refObject = function->refObject;

    if(function->flags & ECC_SCRIPTFUNCFLAG_NEEDHEAP)
    {
        eccobject_t* environment = ecc_object_copy(&function->environment);

        if(function->flags & ECC_SCRIPTFUNCFLAG_NEEDARGUMENTS)
        {
            ecc_oper_makeenvandargswithva(environment, function->parameterCount, argumentCount, ap);

            if(!context->strictMode)
            {
                ecc_object_addmember(environment->hashmap[2].value.data.object, ECC_ConstKey_callee, ecc_oper_retain(ecc_value_function(function)), ECC_VALFLAG_HIDDEN);
                ecc_object_addmember(environment->hashmap[2].value.data.object, ECC_ConstKey_length, ecc_value_integer(argumentCount), ECC_VALFLAG_HIDDEN);
            }
        }
        else
            ecc_oper_populateenvwithva(environment, function->parameterCount, argumentCount, ap);

        return ecc_oper_callops(&subContext, environment);
    }
    else
    {
        eccobject_t environment = function->environment;
        ecchashmap_t hashmap[function->environment.hashmapCapacity];

        memcpy(hashmap, function->environment.hashmap, sizeof(hashmap));
        environment.hashmap = hashmap;

        ecc_oper_populateenvwithva(&environment, function->parameterCount, argumentCount, ap);

        return ecc_oper_callopsrelease(&subContext, &environment);
    }
}

eccvalue_t ecc_oper_callfunction(eccstate_t* context, eccobjfunction_t* const function, eccvalue_t thisval, int32_t argumentCount, int construct)
{
    eccstate_t subContext = {};
    subContext.ops = function->oplist->ops;
    subContext.thisvalue = thisval;
    subContext.parent = context;
    subContext.ecc = context->ecc;
    subContext.construct = construct;
    subContext.depth = context->depth + 1;
    subContext.strictMode = function->flags & ECC_SCRIPTFUNCFLAG_STRICTMODE;
    subContext.refObject = function->refObject;

    if(function->flags & ECC_SCRIPTFUNCFLAG_NEEDHEAP)
    {
        eccobject_t* environment = ecc_object_copy(&function->environment);

        if(function->flags & ECC_SCRIPTFUNCFLAG_NEEDARGUMENTS)
        {
            ecc_oper_makeenvandargswithops(context, environment, ecc_args_createsized(argumentCount), function->parameterCount, argumentCount);

            if(!context->strictMode)
            {
                ecc_object_addmember(environment->hashmap[2].value.data.object, ECC_ConstKey_callee, ecc_oper_retain(ecc_value_function(function)), ECC_VALFLAG_HIDDEN);
                ecc_object_addmember(environment->hashmap[2].value.data.object, ECC_ConstKey_length, ecc_value_integer(argumentCount), ECC_VALFLAG_HIDDEN);
            }
        }
        else
            ecc_oper_populateenvwithops(context, environment, function->parameterCount, argumentCount);

        return ecc_oper_callops(&subContext, environment);
    }
    else if(function->flags & ECC_SCRIPTFUNCFLAG_NEEDARGUMENTS)
    {
        eccobject_t environment = function->environment;
        eccobject_t arguments;
        memset(&arguments, 0, sizeof(eccobject_t));
        ecchashmap_t hashmap[function->environment.hashmapCapacity];
        ecchashitem_t element[argumentCount];

        memcpy(hashmap, function->environment.hashmap, sizeof(hashmap));
        environment.hashmap = hashmap;
        arguments.element = element;
        arguments.elementCount = argumentCount;
        ecc_oper_makestackenvandargswithops(context, &environment, &arguments, function->parameterCount, argumentCount);

        return ecc_oper_callopsrelease(&subContext, &environment);
    }
    else
    {
        eccobject_t environment = function->environment;
        ecchashmap_t hashmap[function->environment.hashmapCapacity];

        memcpy(hashmap, function->environment.hashmap, sizeof(hashmap));
        environment.hashmap = hashmap;
        ecc_oper_populateenvwithops(context, &environment, function->parameterCount, argumentCount);

        return ecc_oper_callopsrelease(&subContext, &environment);
    }
}

eccvalue_t ecc_oper_construct(eccstate_t* context)
{
    const ecctextstring_t* textCall = opmac_text(0);
    const ecctextstring_t* text = opmac_text(1);
    int32_t argumentCount = opmac_value().data.integer;
    eccvalue_t value, *prototype, object, function = opmac_next();

    if(function.type != ECC_VALTYPE_FUNCTION)
        goto error;

    prototype = ecc_object_member(&function.data.function->object, ECC_ConstKey_prototype, 0);
    if(!prototype)
        goto error;

    if(!ecc_value_isobject(*prototype))
        object = ecc_value_object(ecc_object_create(ECC_Prototype_Object));
    else if(prototype->type == ECC_VALTYPE_FUNCTION)
    {
        object = ecc_value_object(ecc_object_create(NULL));
        object.data.object->prototype = &prototype->data.function->object;
    }
    else if(prototype->type == ECC_VALTYPE_OBJECT)
        object = ecc_value_object(ecc_object_create(prototype->data.object));
    else
        object = ECCValConstUndefined;

    ecc_context_settext(context, text);
    value = ecc_oper_callvalue(context, function, object, argumentCount, 1, textCall);

    if(ecc_value_isobject(value))
        return value;
    else
        return object;

    error:
    {
        context->textCall = textCall;
        ecc_context_settextindex(context, ECC_CTXINDEXTYPE_FUNC);
        ecc_context_typeerror(context, ecc_charbuf_create("'%.*s' is not a constructor", text->length, text->bytes));
    }
    return ECCValConstUndefined;
}

eccvalue_t ecc_oper_call(eccstate_t* context)
{
    const ecctextstring_t* textCall = opmac_text(0);
    const ecctextstring_t* text = opmac_text(1);
    int32_t argumentCount = opmac_value().data.integer;
    eccvalue_t value;
    eccvalue_t thisval;

    context->inEnvironmentObject = 0;
    value = opmac_next();

    if(context->inEnvironmentObject)
    {
        eccstate_t* seek = context;
        while(seek->parent && seek->parent->refObject == context->refObject)
            seek = seek->parent;

        ++seek->environment->referenceCount;
        thisval = ecc_value_objectvalue(seek->environment);
    }
    else
        thisval = ECCValConstUndefined;

    ecc_context_settext(context, text);
    return ecc_oper_callvalue(context, value, thisval, argumentCount, 0, textCall);
}

eccvalue_t ecc_oper_eval(eccstate_t* context)
{
    eccvalue_t value;
    eccioinput_t* input;
    int32_t argumentCount = opmac_value().data.integer;
    eccstate_t subContext = {};
    subContext.parent = context;
    subContext.thisvalue = context->thisvalue;
    subContext.environment = context->environment;
    subContext.ecc = context->ecc;
    subContext.depth = context->depth + 1;
    subContext.strictMode = context->strictMode;

    if(!argumentCount)
        return ECCValConstUndefined;

    value = opmac_next();
    while(--argumentCount)
        opmac_next();

    if(!ecc_value_isstring(value) || !ecc_value_isprimitive(value))
        return value;

    input = ecc_ioinput_createfrombytes(ecc_value_stringbytes(&value), ecc_value_stringlength(&value), "(eval)");
    ecc_script_evalinputwithcontext(context->ecc, input, &subContext);

    value = context->ecc->result;
    context->ecc->result = ECCValConstUndefined;
    return value;
}

// Expression

eccvalue_t ecc_oper_noop(eccstate_t* context)
{
    (void)context;
    return ECCValConstUndefined;
}

eccvalue_t ecc_oper_value(eccstate_t* context)
{
    return opmac_value();
}

eccvalue_t ecc_oper_valueconstref(eccstate_t* context)
{
    return ecc_value_reference((eccvalue_t*)&opmac_value());
}

eccvalue_t ecc_oper_text(eccstate_t* context)
{
    return ecc_value_text(opmac_text(0));
}

eccvalue_t ecc_oper_regexp(eccstate_t* context)
{
    const ecctextstring_t* text = opmac_text(0);
    eccobjerror_t* error = NULL;
    ecccharbuffer_t* chars = ecc_charbuf_createwithbytes(text->length, text->bytes);
    eccobjregexp_t* regexp = ecc_regexp_create(chars, &error, context->ecc->sloppyMode ? ECC_REGEXOPT_ALLOWUNICODEFLAGS : 0);
    if(error)
    {
        error->text.bytes = text->bytes + (error->text.bytes - chars->bytes);
        ecc_context_throw(context, ecc_value_error(error));
    }
    return ecc_value_regexp(regexp);
}

eccvalue_t ecc_oper_function(eccstate_t* context)
{
    eccobject_t* prototype;
    eccvalue_t value = opmac_value(), result;

    eccobjfunction_t* function = ecc_function_copy(value.data.function);
    function->object.prototype = &value.data.function->object;
    function->environment.prototype = context->environment;
    if(context->refObject)
    {
        ++context->refObject->referenceCount;
        function->refObject = context->refObject;
    }

    prototype = ecc_object_create(ECC_Prototype_Object);
    ecc_function_linkprototype(function, ecc_value_object(prototype), ECC_VALFLAG_SEALED);

    ++prototype->referenceCount;
    ++context->environment->referenceCount;
    ++function->object.referenceCount;

    result = ecc_value_function(function);
    result.flags = value.flags;
    return result;
}

eccvalue_t ecc_oper_object(eccstate_t* context)
{
    eccobject_t* object = ecc_object_create(ECC_Prototype_Object);
    eccvalue_t property, value;
    uint32_t count;

    object->flags |= ECC_OBJFLAG_MARK;

    for(count = opmac_value().data.integer; count--;)
    {
        property = opmac_next();
        value = ecc_oper_retain(ecc_oper_nextopvalue(context));

        if(property.type == ECC_VALTYPE_KEY)
            ecc_object_addmember(object, property.data.key, value, 0);
        else if(property.type == ECC_VALTYPE_INTEGER)
            ecc_object_addelement(object, property.data.integer, value, 0);
    }
    return ecc_value_object(object);
}

eccvalue_t ecc_oper_array(eccstate_t* context)
{
    uint32_t length = opmac_value().data.integer;
    eccobject_t* object = ecc_array_createsized(length);
    eccvalue_t value;
    uint32_t index;

    object->flags |= ECC_OBJFLAG_MARK;

    for(index = 0; index < length; ++index)
    {
        value = ecc_oper_retain(ecc_oper_nextopvalue(context));
        object->element[index].value = value;
    }
    return ecc_value_object(object);
}

eccvalue_t ecc_oper_getthis(eccstate_t* context)
{
    return context->thisvalue;
}

eccvalue_t* ecc_oper_localref(eccstate_t* context, eccindexkey_t key, const ecctextstring_t* text, int required)
{
    eccvalue_t* ref;

    ref = ecc_object_member(context->environment, key, 0);

    if(!context->strictMode)
    {
        context->inEnvironmentObject = context->refObject != NULL;
        if(!ref && context->refObject)
        {
            context->inEnvironmentObject = 0;
            ref = ecc_object_member(context->refObject, key, 0);
        }
    }

    if(!ref && required)
    {
        ecc_context_settext(context, text);
        ecc_context_referenceerror(context, ecc_charbuf_create("'%.*s' is not defined", ecc_keyidx_textof(key)->length, ecc_keyidx_textof(key)->bytes));
    }
    return ref;
}

eccvalue_t ecc_oper_createlocalref(eccstate_t* context)
{
    eccindexkey_t key = opmac_value().data.key;
    eccvalue_t* ref = ecc_oper_localref(context, key, opmac_text(0), context->strictMode);

    if(!ref)
        ref = ecc_object_addmember(&context->ecc->global->environment, key, ECCValConstUndefined, 0);

    return ecc_value_reference(ref);
}

eccvalue_t ecc_oper_getlocalrefornull(eccstate_t* context)
{
    return ecc_value_reference(ecc_oper_localref(context, opmac_value().data.key, opmac_text(0), 0));
}

eccvalue_t ecc_oper_getlocalref(eccstate_t* context)
{
    return ecc_value_reference(ecc_oper_localref(context, opmac_value().data.key, opmac_text(0), 1));
}

eccvalue_t ecc_oper_getlocal(eccstate_t* context)
{
    return *ecc_oper_localref(context, opmac_value().data.key, opmac_text(0), 1);
}

eccvalue_t ecc_oper_setlocal(eccstate_t* context)
{
    const ecctextstring_t* text = opmac_text(0);
    eccindexkey_t key = opmac_value().data.key;
    eccvalue_t value = opmac_next();

    eccvalue_t* ref = ecc_oper_localref(context, key, text, context->strictMode);

    if(!ref)
        ref = ecc_object_addmember(&context->ecc->global->environment, key, ECCValConstUndefined, 0);

    if(ref->flags & ECC_VALFLAG_READONLY)
        return value;

    ecc_oper_retain(value);
    ecc_oper_release(*ref);
    ecc_oper_replacerefvalue(ref, value);
    return value;
}

eccvalue_t ecc_oper_deletelocal(eccstate_t* context)
{
    eccvalue_t* ref = ecc_oper_localref(context, opmac_value().data.key, opmac_text(0), 0);

    if(!ref)
        return ECCValConstTrue;

    if(ref->flags & ECC_VALFLAG_SEALED)
        return ECCValConstFalse;

    *ref = ECCValConstNone;
    return ECCValConstTrue;
}

eccvalue_t ecc_oper_getlocalslotref(eccstate_t* context)
{
    return ecc_value_reference(&context->environment->hashmap[opmac_value().data.integer].value);
}

eccvalue_t ecc_oper_getlocalslot(eccstate_t* context)
{
    return context->environment->hashmap[opmac_value().data.integer].value;
}

eccvalue_t ecc_oper_setlocalslot(eccstate_t* context)
{
    int32_t slot = opmac_value().data.integer;
    eccvalue_t value = opmac_next();
    eccvalue_t* ref = &context->environment->hashmap[slot].value;

    if(ref->flags & ECC_VALFLAG_READONLY)
        return value;

    ecc_oper_retain(value);
    ecc_oper_release(*ref);
    ecc_oper_replacerefvalue(ref, value);
    return value;
}

eccvalue_t ecc_oper_deletelocalslot(eccstate_t* context)
{
    (void)context;
    return ECCValConstFalse;
}

eccvalue_t ecc_oper_getparentslotref(eccstate_t* context)
{
    int32_t slot = opmac_value().data.integer & 0xffff;
    int32_t count = opmac_value().data.integer >> 16;
    eccobject_t* object = context->environment;
    while(count--)
        object = object->prototype;

    return ecc_value_reference(&object->hashmap[slot].value);
}

eccvalue_t ecc_oper_getparentslot(eccstate_t* context)
{
    return *ecc_oper_getparentslotref(context).data.reference;
}

eccvalue_t ecc_oper_setparentslot(eccstate_t* context)
{
    const ecctextstring_t* text = opmac_text(0);
    eccvalue_t* ref = ecc_oper_getparentslotref(context).data.reference;
    eccvalue_t value = opmac_next();
    if(ref->flags & ECC_VALFLAG_READONLY)
    {
        if(context->strictMode)
        {
            ecctextstring_t property = *ecc_keyidx_textof(ref->key);
            ecc_context_settext(context, text);
            ecc_context_typeerror(context, ecc_charbuf_create("'%.*s' is read-only", property.length, property.bytes));
        }
    }
    else
    {
        ecc_oper_retain(value);
        ecc_oper_release(*ref);
        ecc_oper_replacerefvalue(ref, value);
    }
    return value;
}

eccvalue_t ecc_oper_deleteparentslot(eccstate_t* context)
{
    (void)context;
    return ECCValConstFalse;
}

void ecc_oper_prepareobject(eccstate_t* context, eccvalue_t* object)
{
    const ecctextstring_t* textObject = opmac_text(1);
    *object = opmac_next();

    if(ecc_value_isprimitive(*object))
    {
        ecc_context_settext(context, textObject);
        *object = ecc_value_toobject(context, *object);
    }
}

eccvalue_t ecc_oper_getmemberref(eccstate_t* context)
{
    const ecctextstring_t* text = opmac_text(0);
    eccindexkey_t key = opmac_value().data.key;
    eccvalue_t object, *ref;

    ecc_oper_prepareobject(context, &object);

    context->refObject = object.data.object;
    ref = ecc_object_member(object.data.object, key, ECC_VALFLAG_ASOWN);

    if(!ref)
    {
        if(object.data.object->flags & ECC_OBJFLAG_SEALED)
        {
            ecc_context_settext(context, text);
            ecc_context_typeerror(context, ecc_charbuf_create("object is not extensible"));
        }
        ref = ecc_object_addmember(object.data.object, key, ECCValConstUndefined, 0);
    }

    return ecc_value_reference(ref);
}

eccvalue_t ecc_oper_getmember(eccstate_t* context)
{
    eccindexkey_t key = opmac_value().data.key;
    eccvalue_t object;

    ecc_oper_prepareobject(context, &object);

    return ecc_object_getmember(context, object.data.object, key);
}

eccvalue_t ecc_oper_setmember(eccstate_t* context)
{
    const ecctextstring_t* text = opmac_text(0);
    eccindexkey_t key = opmac_value().data.key;
    eccvalue_t object, value;

    ecc_oper_prepareobject(context, &object);
    value = ecc_oper_retain(opmac_next());

    ecc_context_settext(context, text);
    ecc_object_putmember(context, object.data.object, key, value);

    return value;
}

eccvalue_t ecc_oper_callmember(eccstate_t* context)
{
    const ecctextstring_t* textCall = opmac_text(0);
    int32_t argumentCount = opmac_value().data.integer;
    const ecctextstring_t* text = &(++context->ops)->text;
    eccindexkey_t key = opmac_value().data.key;
    eccvalue_t object;

    ecc_oper_prepareobject(context, &object);

    ecc_context_settext(context, text);
    return ecc_oper_callvalue(context, ecc_object_getmember(context, object.data.object, key), object, argumentCount, 0, textCall);
}

eccvalue_t ecc_oper_deletemember(eccstate_t* context)
{
    const ecctextstring_t* text = opmac_text(0);
    eccindexkey_t key = opmac_value().data.key;
    eccvalue_t object;
    int result;

    ecc_oper_prepareobject(context, &object);

    result = ecc_object_deletemember(object.data.object, key);
    if(!result && context->strictMode)
    {
        ecc_context_settext(context, text);
        ecc_context_typeerror(context, ecc_charbuf_create("'%.*s' is non-configurable", ecc_keyidx_textof(key)->length, ecc_keyidx_textof(key)->bytes));
    }

    return ecc_value_truth(result);
}

void ecc_oper_prepareobjectproperty(eccstate_t* context, eccvalue_t* object, eccvalue_t* property)
{
    const ecctextstring_t* textProperty;

    ecc_oper_prepareobject(context, object);

    textProperty = opmac_text(1);
    *property = opmac_next();

    if(ecc_value_isobject(*property))
    {
        ecc_context_settext(context, textProperty);
        *property = ecc_value_toprimitive(context, *property, ECC_VALHINT_STRING);
    }
}

eccvalue_t ecc_oper_getpropertyref(eccstate_t* context)
{
    const ecctextstring_t* text = opmac_text(1);
    eccvalue_t object, property;
    eccvalue_t* ref;

    ecc_oper_prepareobjectproperty(context, &object, &property);

    context->refObject = object.data.object;
    ref = ecc_object_property(object.data.object, property, ECC_VALFLAG_ASOWN);

    if(!ref)
    {
        if(object.data.object->flags & ECC_OBJFLAG_SEALED)
        {
            ecc_context_settext(context, text);
            ecc_context_typeerror(context, ecc_charbuf_create("object is not extensible"));
        }
        ref = ecc_object_addproperty(object.data.object, property, ECCValConstUndefined, 0);
    }

    return ecc_value_reference(ref);
}

eccvalue_t ecc_oper_getproperty(eccstate_t* context)
{
    eccvalue_t object, property;

    ecc_oper_prepareobjectproperty(context, &object, &property);

    return ecc_object_getproperty(context, object.data.object, property);
}

eccvalue_t ecc_oper_setproperty(eccstate_t* context)
{
    const ecctextstring_t* text = opmac_text(0);
    eccvalue_t object, property, value;

    ecc_oper_prepareobjectproperty(context, &object, &property);

    value = ecc_oper_retain(opmac_next());
    value.flags = 0;

    ecc_context_settext(context, text);
    ecc_object_putproperty(context, object.data.object, property, value);

    return value;
}

eccvalue_t ecc_oper_callproperty(eccstate_t* context)
{
    const ecctextstring_t* textCall = opmac_text(0);
    int32_t argumentCount = opmac_value().data.integer;
    const ecctextstring_t* text = &(++context->ops)->text;
    eccvalue_t object, property;

    ecc_oper_prepareobjectproperty(context, &object, &property);

    ecc_context_settext(context, text);
    return ecc_oper_callvalue(context, ecc_object_getproperty(context, object.data.object, property), object, argumentCount, 0, textCall);
}

eccvalue_t ecc_oper_deleteproperty(eccstate_t* context)
{
    const ecctextstring_t* text = opmac_text(0);
    eccvalue_t object, property;
    int result;

    ecc_oper_prepareobjectproperty(context, &object, &property);

    result = ecc_object_deleteproperty(object.data.object, property);
    if(!result && context->strictMode)
    {
        eccvalue_t string = ecc_value_tostring(context, property);
        ecc_context_settext(context, text);
        ecc_context_typeerror(context, ecc_charbuf_create("'%.*s' is non-configurable", ecc_value_stringlength(&string), ecc_value_stringbytes(&string)));
    }
    return ecc_value_truth(result);
}

eccvalue_t ecc_oper_pushenvironment(eccstate_t* context)
{
    if(context->refObject)
        context->refObject = ecc_object_create(context->refObject);
    else
        context->environment = ecc_object_create(context->environment);

    return opmac_value();
}

eccvalue_t ecc_oper_popenvironment(eccstate_t* context)
{
    if(context->refObject)
    {
        eccobject_t* refObject = context->refObject;
        context->refObject = context->refObject->prototype;
        refObject->prototype = NULL;
    }
    else
    {
        eccobject_t* environment = context->environment;
        context->environment = context->environment->prototype;
        environment->prototype = NULL;
    }
    return opmac_value();
}

eccvalue_t ecc_oper_exchange(eccstate_t* context)
{
    eccvalue_t value = opmac_value();
    opmac_next();
    return value;
}

eccvalue_t ecc_oper_typeof(eccstate_t* context)
{
    eccvalue_t value = opmac_next();
    if(value.type == ECC_VALTYPE_REFERENCE)
    {
        eccvalue_t* ref = value.data.reference;
        if(!ref)
            return ecc_value_text(&ECC_ConstString_Undefined);
        else
            value = *ref;
    }
    return ecc_value_totype(value);
}

#define prepareAB                               \
    const ecctextstring_t* text = opmac_text(1);    \
    eccvalue_t a = opmac_next();                    \
    const ecctextstring_t* textAlt = opmac_text(1); \
    eccvalue_t b = opmac_next();

eccvalue_t ecc_oper_equal(eccstate_t* context)
{
    prepareAB

    if(a.type == ECC_VALTYPE_BINARY && b.type == ECC_VALTYPE_BINARY) return ecc_value_truth(a.data.binary == b.data.binary);
    else
    {
        ecc_context_settexts(context, text, textAlt);
        return ecc_value_equals(context, a, b);
    }
}

eccvalue_t ecc_oper_notequal(eccstate_t* context)
{
    prepareAB

    if(a.type == ECC_VALTYPE_BINARY && b.type == ECC_VALTYPE_BINARY) return ecc_value_truth(a.data.binary != b.data.binary);
    else
    {
        ecc_context_settexts(context, text, textAlt);
        return ecc_value_truth(!ecc_value_istrue(ecc_value_equals(context, a, b)));
    }
}

eccvalue_t ecc_oper_identical(eccstate_t* context)
{
    prepareAB

    if(a.type == ECC_VALTYPE_BINARY && b.type == ECC_VALTYPE_BINARY) return ecc_value_truth(a.data.binary == b.data.binary);
    else
    {
        ecc_context_settexts(context, text, textAlt);
        return ecc_value_same(context, a, b);
    }
}

eccvalue_t ecc_oper_notidentical(eccstate_t* context)
{
    prepareAB

    if(a.type == ECC_VALTYPE_BINARY && b.type == ECC_VALTYPE_BINARY) return ecc_value_truth(a.data.binary != b.data.binary);
    else
    {
        ecc_context_settexts(context, text, textAlt);
        return ecc_value_truth(!ecc_value_istrue(ecc_value_same(context, a, b)));
    }
}

eccvalue_t ecc_oper_less(eccstate_t* context)
{
    prepareAB

    if(a.type == ECC_VALTYPE_BINARY && b.type == ECC_VALTYPE_BINARY) return ecc_value_truth(a.data.binary < b.data.binary);
    else
    {
        ecc_context_settexts(context, text, textAlt);
        return ecc_value_less(context, a, b);
    }
}

eccvalue_t ecc_oper_lessorequal(eccstate_t* context)
{
    prepareAB

    if(a.type == ECC_VALTYPE_BINARY && b.type == ECC_VALTYPE_BINARY) return ecc_value_truth(a.data.binary <= b.data.binary);
    else
    {
        ecc_context_settexts(context, text, textAlt);
        return ecc_value_lessorequal(context, a, b);
    }
}

eccvalue_t ecc_oper_more(eccstate_t* context)
{
    prepareAB

    if(a.type == ECC_VALTYPE_BINARY && b.type == ECC_VALTYPE_BINARY) return ecc_value_truth(a.data.binary > b.data.binary);
    else
    {
        ecc_context_settexts(context, text, textAlt);
        return ecc_value_more(context, a, b);
    }
}

eccvalue_t ecc_oper_moreorequal(eccstate_t* context)
{
    prepareAB

    if(a.type == ECC_VALTYPE_BINARY && b.type == ECC_VALTYPE_BINARY) return ecc_value_truth(a.data.binary >= b.data.binary);
    else
    {
        ecc_context_settexts(context, text, textAlt);
        return ecc_value_moreorequal(context, a, b);
    }
}

eccvalue_t ecc_oper_instanceof(eccstate_t* context)
{
    eccvalue_t a = opmac_next();
    const ecctextstring_t* textAlt = opmac_text(1);
    eccvalue_t b = opmac_next();

    if(b.type != ECC_VALTYPE_FUNCTION)
    {
        ecc_context_settext(context, textAlt);
        ecc_context_typeerror(context, ecc_charbuf_create("'%.*s' is not a function", textAlt->length, textAlt->bytes));
    }

    b = ecc_object_getmember(context, b.data.object, ECC_ConstKey_prototype);
    if(!ecc_value_isobject(b))
    {
        ecc_context_settext(context, textAlt);
        ecc_context_typeerror(context, ecc_charbuf_create("'%.*s'.prototype not an object", textAlt->length, textAlt->bytes));
    }

    if(ecc_value_isobject(a))
    {
        eccobject_t* object;

        object = a.data.object;
        while((object = object->prototype))
            if(object == b.data.object)
                return ECCValConstTrue;
    }
    return ECCValConstFalse;
}

eccvalue_t ecc_oper_in(eccstate_t* context)
{
    eccvalue_t property = opmac_next();
    eccvalue_t object = opmac_next();
    eccvalue_t* ref;

    if(!ecc_value_isobject(object))
        ecc_context_typeerror(context, ecc_charbuf_create("'%.*s' not an object", context->ops->text.length, context->ops->text.bytes));

    ref = ecc_object_property(object.data.object, ecc_value_tostring(context, property), 0);

    return ecc_value_truth(ref != NULL);
}

eccvalue_t ecc_oper_add(eccstate_t* context)
{
    prepareAB

    if(a.type == ECC_VALTYPE_BINARY && b.type == ECC_VALTYPE_BINARY)
    {
        a.data.binary += b.data.binary;
        return a;
    }
    else
    {
        ecc_context_settexts(context, text, textAlt);
        return ecc_value_add(context, a, b);
    }
}

eccvalue_t ecc_oper_minus(eccstate_t* context)
{
    eccvalue_t a = opmac_next();
    eccvalue_t b = opmac_next();
    if(a.type == ECC_VALTYPE_BINARY && b.type == ECC_VALTYPE_BINARY)
    {
        a.data.binary -= b.data.binary;
        return a;
    }
    else
        return ecc_value_binary(ecc_value_tobinary(context, a).data.binary - ecc_value_tobinary(context, b).data.binary);
}

eccvalue_t ecc_oper_multiply(eccstate_t* context)
{
    eccvalue_t a = opmac_next();
    eccvalue_t b = opmac_next();
    if(a.type == ECC_VALTYPE_BINARY && b.type == ECC_VALTYPE_BINARY)
    {
        a.data.binary *= b.data.binary;
        return a;
    }
    else
        return ecc_value_binary(ecc_value_tobinary(context, a).data.binary * ecc_value_tobinary(context, b).data.binary);
}

eccvalue_t ecc_oper_divide(eccstate_t* context)
{
    eccvalue_t a = opmac_next();
    eccvalue_t b = opmac_next();
    if(a.type == ECC_VALTYPE_BINARY && b.type == ECC_VALTYPE_BINARY)
    {
        a.data.binary /= b.data.binary;
        return a;
    }
    else
        return ecc_value_binary(ecc_value_tobinary(context, a).data.binary / ecc_value_tobinary(context, b).data.binary);
}

eccvalue_t ecc_oper_modulo(eccstate_t* context)
{
    eccvalue_t a = opmac_next();
    eccvalue_t b = opmac_next();
    if(a.type == ECC_VALTYPE_BINARY && b.type == ECC_VALTYPE_BINARY)
    {
        a.data.binary = fmod(a.data.binary, b.data.binary);
        return a;
    }
    else
        return ecc_value_binary(fmod(ecc_value_tobinary(context, a).data.binary, ecc_value_tobinary(context, b).data.binary));
}

eccvalue_t ecc_oper_leftshift(eccstate_t* context)
{
    eccvalue_t a = opmac_next();
    eccvalue_t b = opmac_next();
    return ecc_value_binary(ecc_value_tointeger(context, a).data.integer << (uint32_t)ecc_value_tointeger(context, b).data.integer);
}

eccvalue_t ecc_oper_rightshift(eccstate_t* context)
{
    eccvalue_t a = opmac_next();
    eccvalue_t b = opmac_next();
    return ecc_value_binary(ecc_value_tointeger(context, a).data.integer >> (uint32_t)ecc_value_tointeger(context, b).data.integer);
}

eccvalue_t ecc_oper_unsignedrightshift(eccstate_t* context)
{
    eccvalue_t a = opmac_next();
    eccvalue_t b = opmac_next();
    return ecc_value_binary((uint32_t)ecc_value_tointeger(context, a).data.integer >> (uint32_t)ecc_value_tointeger(context, b).data.integer);
}

eccvalue_t ecc_oper_bitwiseand(eccstate_t* context)
{
    eccvalue_t a = opmac_next();
    eccvalue_t b = opmac_next();
    return ecc_value_binary(ecc_value_tointeger(context, a).data.integer & ecc_value_tointeger(context, b).data.integer);
}

eccvalue_t ecc_oper_bitwisexor(eccstate_t* context)
{
    eccvalue_t a = opmac_next();
    eccvalue_t b = opmac_next();
    return ecc_value_binary(ecc_value_tointeger(context, a).data.integer ^ ecc_value_tointeger(context, b).data.integer);
}

eccvalue_t ecc_oper_bitwiseor(eccstate_t* context)
{
    eccvalue_t a = opmac_next();
    eccvalue_t b = opmac_next();
    return ecc_value_binary(ecc_value_tointeger(context, a).data.integer | ecc_value_tointeger(context, b).data.integer);
}

eccvalue_t ecc_oper_logicaland(eccstate_t* context)
{
    const int32_t opCount = opmac_value().data.integer;
    eccvalue_t value = opmac_next();

    if(!ecc_value_istrue(value))
    {
        context->ops += opCount;
        return value;
    }
    else
        return opmac_next();
}

eccvalue_t ecc_oper_logicalor(eccstate_t* context)
{
    const int32_t opCount = opmac_value().data.integer;
    eccvalue_t value = opmac_next();

    if(ecc_value_istrue(value))
    {
        context->ops += opCount;
        return value;
    }
    else
        return opmac_next();
}

eccvalue_t ecc_oper_positive(eccstate_t* context)
{
    eccvalue_t a = opmac_next();
    if(a.type == ECC_VALTYPE_BINARY)
        return a;
    else
        return ecc_value_tobinary(context, a);
}

eccvalue_t ecc_oper_negative(eccstate_t* context)
{
    eccvalue_t a = opmac_next();
    if(a.type == ECC_VALTYPE_BINARY)
        return ecc_value_binary(-a.data.binary);
    else
        return ecc_value_binary(-ecc_value_tobinary(context, a).data.binary);
}

eccvalue_t ecc_oper_invert(eccstate_t* context)
{
    eccvalue_t a = opmac_next();
    return ecc_value_binary(~ecc_value_tointeger(context, a).data.integer);
}

eccvalue_t ecc_oper_logicalnot(eccstate_t * context)
{
    eccvalue_t a = opmac_next();
    return ecc_value_truth(!ecc_value_istrue(a));
}

// MARK: assignement

#define unaryBinaryOpRef(OP)                                                                               \
    eccobject_t* refObject = context->refObject;                                                           \
    const ecctextstring_t* text = opmac_text(0);                                                               \
    eccvalue_t* ref = opmac_next().data.reference;                                                             \
    eccvalue_t a;                                                                                          \
    double result;                                                                                         \
                                                                                                           \
    a = *ref;                                                                                              \
    if(a.flags & (ECC_VALFLAG_READONLY | ECC_VALFLAG_ACCESSOR))                                            \
    {                                                                                                      \
        ecc_context_settext(context, text);                                                               \
        a = ecc_value_tobinary(context, ecc_oper_release(ecc_object_getvalue(context, context->refObject, ref))); \
        result = OP;                                                                                       \
        ecc_object_putvalue(context, context->refObject, ref, a);                                         \
        return ecc_value_binary(result);                                                                  \
    }                                                                                                      \
    else if(a.type != ECC_VALTYPE_BINARY)                                                                  \
        a = ecc_value_tobinary(context, ecc_oper_release(a));                                                      \
                                                                                                           \
    result = OP;                                                                                           \
    ecc_oper_replacerefvalue(ref, a);                                                                               \
    context->refObject = refObject;                                                                        \
    return ecc_value_binary(result);

eccvalue_t ecc_oper_incrementref(eccstate_t* context){ unaryBinaryOpRef(++a.data.binary) }

eccvalue_t ecc_oper_decrementref(eccstate_t* context){ unaryBinaryOpRef(--a.data.binary) }

eccvalue_t ecc_oper_postincrementref(eccstate_t* context){ unaryBinaryOpRef(a.data.binary++) }

eccvalue_t ecc_oper_postdecrementref(eccstate_t* context){ unaryBinaryOpRef(a.data.binary--) }

#define assignOpRef(OP, TYPE, CONV)                                                \
    eccobject_t* refObject = context->refObject;                                   \
    const ecctextstring_t* text = opmac_text(0);                                       \
    eccvalue_t* ref = opmac_next().data.reference;                                     \
    eccvalue_t a, b = opmac_next();                                                    \
                                                                                   \
    if(b.type != TYPE)                                                             \
        b = CONV(context, b);                                                      \
                                                                                   \
    a = *ref;                                                                      \
    if(a.flags & (ECC_VALFLAG_READONLY | ECC_VALFLAG_ACCESSOR))                    \
    {                                                                              \
        ecc_context_settext(context, text);                                       \
        a = CONV(context, ecc_object_getvalue(context, context->refObject, ref)); \
        OP;                                                                        \
        return ecc_object_putvalue(context, context->refObject, ref, a);          \
    }                                                                              \
    else if(a.type != TYPE)                                                        \
        a = CONV(context, ecc_oper_release(a));                                             \
                                                                                   \
    OP;                                                                            \
    ecc_oper_replacerefvalue(ref, a);                                                       \
    context->refObject = refObject;                                                \
    return a;

#define assignBinaryOpRef(OP) assignOpRef(OP, ECC_VALTYPE_BINARY, ecc_value_tobinary)
#define assignIntegerOpRef(OP) assignOpRef(OP, ECC_VALTYPE_INTEGER, ecc_value_tointeger)

eccvalue_t ecc_oper_addassignref(eccstate_t* context)
{
    eccobject_t* refObject = context->refObject;
    const ecctextstring_t* text = opmac_text(1);
    eccvalue_t* ref = opmac_next().data.reference;
    const ecctextstring_t* textAlt = opmac_text(1);
    eccvalue_t a, b = opmac_next();

    ecc_context_settexts(context, text, textAlt);

    a = *ref;
    if(a.flags & (ECC_VALFLAG_READONLY | ECC_VALFLAG_ACCESSOR))
    {
        a = ecc_object_getvalue(context, context->refObject, ref);
        a = ecc_oper_retain(ecc_value_add(context, a, b));
        return ecc_object_putvalue(context, context->refObject, ref, a);
    }

    if(a.type == ECC_VALTYPE_BINARY && b.type == ECC_VALTYPE_BINARY)
    {
        a.data.binary += b.data.binary;
        return *ref = a;
    }

    a = ecc_oper_retain(ecc_value_add(context, ecc_oper_release(a), b));
    ecc_oper_replacerefvalue(ref, a);
    context->refObject = refObject;
    return a;
}

eccvalue_t ecc_oper_minusassignref(eccstate_t* context)
{
    assignBinaryOpRef(a.data.binary -= b.data.binary);
}

eccvalue_t ecc_oper_multiplyassignref(eccstate_t* context)
{
    assignBinaryOpRef(a.data.binary *= b.data.binary);
}

eccvalue_t ecc_oper_divideassignref(eccstate_t* context)
{
    assignBinaryOpRef(a.data.binary /= b.data.binary);
}

eccvalue_t ecc_oper_moduloassignref(eccstate_t* context)
{
    assignBinaryOpRef(a.data.binary = fmod(a.data.binary, b.data.binary));
}

eccvalue_t ecc_oper_leftshiftassignref(eccstate_t* context)
{
    assignIntegerOpRef(a.data.integer <<= (uint32_t)b.data.integer);
}

eccvalue_t ecc_oper_rightshiftassignref(eccstate_t* context)
{
    assignIntegerOpRef(a.data.integer >>= (uint32_t)b.data.integer);
}

eccvalue_t ecc_oper_unsignedrightshiftassignref(eccstate_t* context)
{
    assignIntegerOpRef(a.data.integer = (uint32_t)a.data.integer >> (uint32_t)b.data.integer);
}

eccvalue_t ecc_oper_bitandassignref(eccstate_t* context)
{
    assignIntegerOpRef(a.data.integer &= (uint32_t)b.data.integer);
}

eccvalue_t ecc_oper_bitxorassignref(eccstate_t* context)
{
    assignIntegerOpRef(a.data.integer ^= (uint32_t)b.data.integer);
}

eccvalue_t ecc_oper_bitorassignref(eccstate_t* context)
{
    assignIntegerOpRef(a.data.integer |= (uint32_t)b.data.integer);
}

// MARK: Statement

eccvalue_t ecc_oper_debugger(eccstate_t* context)
{
    g_ops_debugging = 1;
    return ecc_oper_trapop(context, 0);
}

eccvalue_t ecc_oper_try(eccstate_t* context)
{
    eccobject_t* environment = context->environment;
    eccobject_t* refObject = context->refObject;
    const eccoperand_t* end = context->ops + opmac_value().data.integer;
    eccindexkey_t key;

    const eccoperand_t*  rethrowOps = NULL;
     int rethrow = 0, breaker = 0;
     eccvalue_t value = ECCValConstUndefined;
    eccvalue_t finallyValue;
    uint32_t indices[3];

    ecc_mempool_getindices(indices);

    if(!setjmp(*ecc_script_pushenv(context->ecc)))// try
        value = opmac_next();
    else
    {
        value = context->ecc->result;
        context->ecc->result = ECCValConstUndefined;
        rethrowOps = context->ops;

        if(!rethrow)// catch
        {
            ecc_mempool_unreferencefromindices(indices);

            rethrow = 1;
            context->ops = end + 1;// bypass catch jump
            context->environment = environment;
            context->refObject = refObject;
            key = opmac_next().data.key;

            if(!ecc_keyidx_isequal(key, ECC_ConstKey_none))
            {
                if(value.type == ECC_VALTYPE_FUNCTION)
                {
                    value.data.function->flags |= ECC_SCRIPTFUNCFLAG_USEBOUNDTHIS;
                    value.data.function->boundThis = ecc_value_object(context->environment);
                }
                ecc_object_addmember(context->environment, key, value, ECC_VALFLAG_SEALED);
                value = opmac_next();// execute until noop
                rethrow = 0;
                if(context->breaker)
                    ecc_oper_popenvironment(context);
            }
        }
        else// rethrow
            ecc_oper_popenvironment(context);
    }

    ecc_script_popenv(context->ecc);

    breaker = context->breaker;
    context->breaker = 0;
    context->ops = end;// op[end] = ecc_oper_jump, to after catch
    finallyValue = opmac_next();// jump to after catch, and execute until noop

    if(context->breaker) /* return breaker */
        return finallyValue;
    else if(rethrow)
    {
        context->ops = rethrowOps;
        ecc_context_throw(context, ecc_oper_retain(value));
    }
    else if(breaker)
    {
        context->breaker = breaker;
        return value;
    }
    return opmac_next();
}

eccvalue_t ecc_oper_throw(eccstate_t * context)
{
    context->ecc->text = *opmac_text(1);
    ecc_context_throw(context, ecc_oper_retain(ecc_oper_trapop(context, 0)));
    return ECCValConstUndefined;
}

eccvalue_t ecc_oper_with(eccstate_t* context)
{
    eccobject_t* environment = context->environment;
    eccobject_t* refObject = context->refObject;
    eccobject_t* object = ecc_value_toobject(context, opmac_next()).data.object;
    eccvalue_t value;
    if(!refObject)
    {
        context->refObject = context->environment;
    }
    context->environment = object;
    value = opmac_next();
    context->environment = environment;
    context->refObject = refObject;
    if(context->breaker)
    {
        return value;
    }
    return opmac_next();
}

eccvalue_t ecc_oper_next(eccstate_t* context)
{
    return opmac_next();
}

eccvalue_t ecc_oper_nextif(eccstate_t* context)
{
    eccvalue_t value = opmac_value();

    if(!ecc_value_istrue(ecc_oper_trapop(context, 1)))
        return value;

    return opmac_next();
}

eccvalue_t ecc_oper_autoreleaseexpression(eccstate_t* context)
{
    uint32_t indices[3];

    ecc_mempool_getindices(indices);
    ecc_oper_release(context->ecc->result);
    context->ecc->result = ecc_oper_retain(ecc_oper_trapop(context, 1));
    ecc_mempool_collectunreferencedfromindices(indices);
    return opmac_next();
}

eccvalue_t ecc_oper_autoreleasediscard(eccstate_t* context)
{
    uint32_t indices[3];

    ecc_mempool_getindices(indices);
    ecc_oper_trapop(context, 1);
    ecc_mempool_collectunreferencedfromindices(indices);
    return opmac_next();
}

eccvalue_t ecc_oper_expression(eccstate_t* context)
{
    ecc_oper_release(context->ecc->result);
    context->ecc->result = ecc_oper_retain(ecc_oper_trapop(context, 1));
    return opmac_next();
}

eccvalue_t ecc_oper_discard(eccstate_t* context)
{
    ecc_oper_trapop(context, 1);
    return opmac_next();
}

eccvalue_t ecc_oper_discardn(eccstate_t* context)
{
    switch(opmac_value().data.integer)
    {
        default:
            {
                ecc_script_fatal("Invalid discardN : %d", opmac_value().data.integer);
            }
            /* fallthrough */
        case 16:
            {
                ecc_oper_trapop(context, 1);
            }
            /* fallthrough */
        case 15:
            {
                ecc_oper_trapop(context, 1);
            }
            /* fallthrough */
        case 14:
            {
                ecc_oper_trapop(context, 1);
            }
            /* fallthrough */
        case 13:
            {
                ecc_oper_trapop(context, 1);
            }
            /* fallthrough */
        case 12:
            {
                ecc_oper_trapop(context, 1);
            }
            /* fallthrough */
        case 11:
            {
                ecc_oper_trapop(context, 1);
            }
            /* fallthrough */
        case 10:
            {
                ecc_oper_trapop(context, 1);
            }
            /* fallthrough */
        case 9:
            {
                ecc_oper_trapop(context, 1);
            }
            /* fallthrough */
        case 8:
            {
                ecc_oper_trapop(context, 1);
            }
            /* fallthrough */
        case 7:
            {
                ecc_oper_trapop(context, 1);
            }
            /* fallthrough */
        case 6:
            {
                ecc_oper_trapop(context, 1);
            }
            /* fallthrough */
        case 5:
            {
                ecc_oper_trapop(context, 1);
            }
            /* fallthrough */
        case 4:
            {
                ecc_oper_trapop(context, 1);
            }
            /* fallthrough */
        case 3:
            {
                ecc_oper_trapop(context, 1);
            }
            /* fallthrough */
        case 2:
            {
                ecc_oper_trapop(context, 1);
            }
            /* fallthrough */
        case 1:
            {
                ecc_oper_trapop(context, 1);
            }
            /* fallthrough */
    }
    return opmac_next();
}

eccvalue_t ecc_oper_jump(eccstate_t* context)
{
    int32_t offset = opmac_value().data.integer;
    context->ops += offset;
    return opmac_next();
}

eccvalue_t ecc_oper_jumpif(eccstate_t* context)
{
    int32_t offset = opmac_value().data.integer;
    eccvalue_t value;

    value = ecc_oper_trapop(context, 1);
    if(ecc_value_istrue(value))
        context->ops += offset;

    return opmac_next();
}

eccvalue_t ecc_oper_jumpifnot(eccstate_t* context)
{
    int32_t offset = opmac_value().data.integer;
    eccvalue_t value;

    value = ecc_oper_trapop(context, 1);
    if(!ecc_value_istrue(value))
        context->ops += offset;

    return opmac_next();
}

eccvalue_t ecc_oper_result(eccstate_t* context)
{
    eccvalue_t result = ecc_oper_trapop(context, 0);
    context->breaker = -1;
    return result;
}

eccvalue_t ecc_oper_repopulate(eccstate_t* context)
{
    uint32_t index, count, arguments = opmac_value().data.integer + 3;
    int32_t offset = opmac_next().data.integer;
    const eccoperand_t* nextOps = context->ops + offset;

    {
        ecchashmap_t hashmap[context->environment->hashmapCapacity];
        count = arguments <= context->environment->hashmapCapacity ? arguments : context->environment->hashmapCapacity;

        for(index = 0; index < 3; ++index)
            hashmap[index].value = context->environment->hashmap[index].value;

        for(; index < count; ++index)
        {
            ecc_oper_release(context->environment->hashmap[index].value);
            hashmap[index].value = ecc_oper_retain(opmac_next());
        }

        if(index < context->environment->hashmapCapacity)
        {
            for(; index < context->environment->hashmapCapacity; ++index)
            {
                ecc_oper_release(context->environment->hashmap[index].value);
                hashmap[index].value = ECCValConstNone;
            }
        }
        else
            for(; index < arguments; ++index)
                opmac_next();

        if(context->environment->hashmap[2].value.type == ECC_VALTYPE_OBJECT)
        {
            eccobject_t* argvals = context->environment->hashmap[2].value.data.object;

            for(index = 3; index < context->environment->hashmapCapacity; ++index)
                argvals->element[index - 3].value = hashmap[index].value;
        }

        memcpy(context->environment->hashmap, hashmap, sizeof(hashmap));
    }

    context->ops = nextOps;
    return opmac_next();
}

eccvalue_t ecc_oper_resultvoid(eccstate_t* context)
{
    eccvalue_t result = ECCValConstUndefined;
    context->breaker = -1;
    return result;
}

eccvalue_t ecc_oper_switchop(eccstate_t* context)
{
    int32_t offset = opmac_value().data.integer;
    const eccoperand_t* nextOps = context->ops + offset;
    eccvalue_t value, caseValue;
    const ecctextstring_t* text = opmac_text(1);

    value = ecc_oper_trapop(context, 1);

    while(context->ops < nextOps)
    {
        const ecctextstring_t* textAlt = opmac_text(1);
        caseValue = opmac_next();

        ecc_context_settexts(context, text, textAlt);
        if(ecc_value_istrue(ecc_value_same(context, value, caseValue)))
        {
            offset = opmac_next().data.integer;
            context->ops = nextOps + offset;
            break;
        }
        else
            ++context->ops;
    }

    value = opmac_next();
    if(context->breaker && --context->breaker)
        return value;
    else
    {
        context->ops = nextOps + 2 + nextOps[2].value.data.integer;
        return opmac_next();
    }
}

// MARK: Iteration

#define stepIteration(value, nextOps, then)                         \
    {                                                               \
        uint32_t indices[3];                                        \
        ecc_mempool_getindices(indices);                         \
        value = opmac_next();                                           \
        if(context->breaker && --context->breaker)                  \
        {                                                           \
            if(--context->breaker)                                  \
                return value; /* return breaker */                  \
            else                                                    \
                then;                                               \
        }                                                           \
        else                                                        \
        {                                                           \
            ecc_mempool_collectunreferencedfromindices(indices); \
            context->ops = nextOps;                                 \
        }                                                           \
    }

eccvalue_t ecc_oper_breaker(eccstate_t* context)
{
    context->breaker = opmac_value().data.integer;
    return ECCValConstUndefined;
}

eccvalue_t ecc_oper_iterate(eccstate_t* context)
{
    const eccoperand_t* startOps = context->ops;
    const eccoperand_t* endOps = startOps;
    const eccoperand_t* nextOps = startOps + 1;
    eccvalue_t value;
    int32_t skipOp = opmac_value().data.integer;

    context->ops = nextOps + skipOp;

    while(ecc_value_istrue(opmac_next()))
        stepIteration(value, nextOps, break);

    context->ops = endOps;

    return opmac_next();
}

eccvalue_t ecc_oper_iterateintegerref(eccstate_t *context, eccoperfncompareint_t compareInteger, eccoperfnwontoverflow_t wontOverflow, eccoperfncomparevalue_t compareValue, eccoperfnvaluestep_t valueStep)
{
    eccobject_t* refObject = context->refObject;
    const eccoperand_t* endOps = context->ops + opmac_value().data.integer;
    eccvalue_t stepValue = opmac_next();
    eccvalue_t* indexRef = opmac_next().data.reference;
    eccvalue_t* countRef = opmac_next().data.reference;
    const eccoperand_t* nextOps = context->ops;
    eccvalue_t value;

    if(indexRef->type == ECC_VALTYPE_BINARY && indexRef->data.binary >= INT32_MIN && indexRef->data.binary <= INT32_MAX)
    {
        eccvalue_t integerValue = ecc_value_tointeger(context, *indexRef);
        if(indexRef->data.binary == integerValue.data.integer)
            *indexRef = integerValue;
    }

    if(countRef->type == ECC_VALTYPE_BINARY && countRef->data.binary >= INT32_MIN && countRef->data.binary <= INT32_MAX)
    {
        eccvalue_t integerValue = ecc_value_tointeger(context, *countRef);
        if(countRef->data.binary == integerValue.data.integer)
            *countRef = integerValue;
    }

    if(indexRef->type == ECC_VALTYPE_INTEGER && countRef->type == ECC_VALTYPE_INTEGER)
    {
        int32_t step = valueStep == ecc_value_add ? stepValue.data.integer : -stepValue.data.integer;
        if(!wontOverflow(countRef->data.integer, step - 1))
            goto deoptimize;

        for(; compareInteger(indexRef->data.integer, countRef->data.integer); indexRef->data.integer += step)
            stepIteration(value, nextOps, goto done);
    }

deoptimize:
    for(; ecc_value_istrue(compareValue(context, *indexRef, *countRef)); *indexRef = valueStep(context, *indexRef, stepValue))
        stepIteration(value, nextOps, break);

done:
    context->refObject = refObject;
    context->ops = endOps;
    return opmac_next();
}

eccvalue_t ecc_oper_iteratelessref(eccstate_t* context)
{
    return ecc_oper_iterateintegerref(context, ecc_oper_intlessthan, ecc_oper_testintegerwontofpos, ecc_value_less, ecc_value_add);
}

eccvalue_t ecc_oper_iteratelessorequalref(eccstate_t* context)
{
    return ecc_oper_iterateintegerref(context, ecc_oper_intlessequal, ecc_oper_testintegerwontofpos, ecc_value_lessorequal, ecc_value_add);
}

eccvalue_t ecc_oper_iteratemoreref(eccstate_t* context)
{
    return ecc_oper_iterateintegerref(context, ecc_oper_intgreaterthan, ecc_oper_testintwontofneg, ecc_value_more, ecc_value_subtract);
}

eccvalue_t ecc_oper_iteratemoreorequalref(eccstate_t* context)
{
    return ecc_oper_iterateintegerref(context, ecc_oper_intgreaterequal, ecc_oper_testintwontofneg, ecc_value_moreorequal, ecc_value_subtract);
}

eccvalue_t ecc_oper_iterateinref(eccstate_t* context)
{
    eccobject_t* refObject = context->refObject;
    eccvalue_t* ref = opmac_next().data.reference;
    eccvalue_t target = opmac_next();
    eccvalue_t value = opmac_next(), key;
    eccappendbuffer_t chars;
    eccobject_t* object;
    const eccoperand_t* startOps = context->ops;
    const eccoperand_t* endOps = startOps + value.data.integer;
    uint32_t index, count;

    if(ecc_value_isobject(target))
    {
        object = target.data.object;
        do
        {
            count = object->elementCount < object->elementCapacity ? object->elementCount : object->elementCapacity;
            for(index = 0; index < count; ++index)
            {
                ecchashitem_t* element = object->element + index;

                if(element->value.check != 1 || (element->value.flags & ECC_VALFLAG_HIDDEN))
                    continue;

                if(object != target.data.object && &element->value != ecc_object_element(target.data.object, index, 0))
                    continue;

                ecc_charbuf_beginappend(&chars);
                ecc_charbuf_append(&chars, "%d", index);
                key = ecc_charbuf_endappend(&chars);
                ecc_oper_replacerefvalue(ref, key);

                stepIteration(value, startOps, break);
            }
        } while((object = object->prototype));

        object = target.data.object;
        do
        {
            count = object->hashmapCount;
            for(index = 2; index < count; ++index)
            {
                ecchashmap_t* hashmap = object->hashmap + index;

                if(hashmap->value.check != 1 || (hashmap->value.flags & ECC_VALFLAG_HIDDEN))
                    continue;

                if(object != target.data.object && &hashmap->value != ecc_object_member(target.data.object, hashmap->value.key, 0))
                    continue;

                key = ecc_value_text(ecc_keyidx_textof(hashmap->value.key));
                ecc_oper_replacerefvalue(ref, key);

                stepIteration(value, startOps, break);
            }
        } while((object = object->prototype));
    }

    context->refObject = refObject;
    context->ops = endOps;
    return opmac_next();
}
