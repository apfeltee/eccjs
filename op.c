//
//  op.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"

/*
  eccop_retain
  eccop_release
  eccop_callOps
  eccop_localRef
  eccop_callValue
  eccop_intLessThan
  eccop_intGreaterThan
  eccop_nextOpValue
  eccop_callFunction
  eccop_prepareObject
  eccop_callOpsRelease
  eccop_replaceRefValue
  eccop_iterateIntegerRef
  eccop_intLessEqual
  eccop_intGreaterEqual
  eccop_prepareObjectProperty
  eccop_populateEnvWithVA
  eccop_populateEnvWithOps
  eccop_testIntegerWontOFPos
  eccop_testIntWontOFNeg
  eccop_makeEnvWithArgs
  eccop_makeEnvAndArgsWithVA
  eccop_makeEnvAndArgsWithOps
  eccop_makeStackEnvAndArgsWithOps
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


static eccoperand_t nsopfn_make(const eccnativefuncptr_t native, eccvalue_t value, ecctextstring_t text);
static const char* nsopfn_tochars(const eccnativefuncptr_t native);
static eccvalue_t nsopfn_callfunctionarguments(eccstate_t*, eccctxoffsettype_t, eccobjfunction_t* function, eccvalue_t thisval, eccobject_t* arguments);
static eccvalue_t nsopfn_callfunctionva(eccstate_t*, eccctxoffsettype_t, eccobjfunction_t* function, eccvalue_t thisval, int argumentCount, va_list ap);
static eccvalue_t nsopfn_noop(eccstate_t*);
static eccvalue_t nsopfn_value(eccstate_t*);
static eccvalue_t nsopfn_valueConstRef(eccstate_t*);
static eccvalue_t nsopfn_text(eccstate_t*);
static eccvalue_t nsopfn_function(eccstate_t*);
static eccvalue_t nsopfn_object(eccstate_t*);
static eccvalue_t nsopfn_array(eccstate_t*);
static eccvalue_t nsopfn_regexp(eccstate_t*);
static eccvalue_t nsopfn_this(eccstate_t*);
static eccvalue_t nsopfn_createLocalRef(eccstate_t*);
static eccvalue_t nsopfn_getLocalRefOrNull(eccstate_t*);
static eccvalue_t nsopfn_getLocalRef(eccstate_t*);
static eccvalue_t nsopfn_getLocal(eccstate_t*);
static eccvalue_t nsopfn_setLocal(eccstate_t*);
static eccvalue_t nsopfn_deleteLocal(eccstate_t*);
static eccvalue_t nsopfn_getLocalSlotRef(eccstate_t*);
static eccvalue_t nsopfn_getLocalSlot(eccstate_t*);
static eccvalue_t nsopfn_setLocalSlot(eccstate_t*);
static eccvalue_t nsopfn_deleteLocalSlot(eccstate_t*);
static eccvalue_t nsopfn_getParentSlotRef(eccstate_t*);
static eccvalue_t nsopfn_getParentSlot(eccstate_t*);
static eccvalue_t nsopfn_setParentSlot(eccstate_t*);
static eccvalue_t nsopfn_deleteParentSlot(eccstate_t*);
static eccvalue_t nsopfn_getMemberRef(eccstate_t*);
static eccvalue_t nsopfn_getMember(eccstate_t*);
static eccvalue_t nsopfn_setMember(eccstate_t*);
static eccvalue_t nsopfn_callMember(eccstate_t*);
static eccvalue_t nsopfn_deleteMember(eccstate_t*);
static eccvalue_t nsopfn_getPropertyRef(eccstate_t*);
static eccvalue_t nsopfn_getProperty(eccstate_t*);
static eccvalue_t nsopfn_setProperty(eccstate_t*);
static eccvalue_t nsopfn_callProperty(eccstate_t*);
static eccvalue_t nsopfn_deleteProperty(eccstate_t*);
static eccvalue_t nsopfn_pushEnvironment(eccstate_t*);
static eccvalue_t nsopfn_popEnvironment(eccstate_t*);
static eccvalue_t nsopfn_exchange(eccstate_t*);
static eccvalue_t nsopfn_typeOf(eccstate_t*);
static eccvalue_t nsopfn_equal(eccstate_t*);
static eccvalue_t nsopfn_notEqual(eccstate_t*);
static eccvalue_t nsopfn_identical(eccstate_t*);
static eccvalue_t nsopfn_notIdentical(eccstate_t*);
static eccvalue_t nsopfn_less(eccstate_t*);
static eccvalue_t nsopfn_lessOrEqual(eccstate_t*);
static eccvalue_t nsopfn_more(eccstate_t*);
static eccvalue_t nsopfn_moreOrEqual(eccstate_t*);
static eccvalue_t nsopfn_instanceOf(eccstate_t*);
static eccvalue_t nsopfn_in(eccstate_t*);
static eccvalue_t nsopfn_add(eccstate_t*);
static eccvalue_t nsopfn_minus(eccstate_t*);
static eccvalue_t nsopfn_multiply(eccstate_t*);
static eccvalue_t nsopfn_divide(eccstate_t*);
static eccvalue_t nsopfn_modulo(eccstate_t*);
static eccvalue_t nsopfn_leftShift(eccstate_t*);
static eccvalue_t nsopfn_rightShift(eccstate_t*);
static eccvalue_t nsopfn_unsignedRightShift(eccstate_t*);
static eccvalue_t nsopfn_bitwiseAnd(eccstate_t*);
static eccvalue_t nsopfn_bitwiseXor(eccstate_t*);
static eccvalue_t nsopfn_bitwiseOr(eccstate_t*);
static eccvalue_t nsopfn_logicalAnd(eccstate_t*);
static eccvalue_t nsopfn_logicalOr(eccstate_t*);
static eccvalue_t nsopfn_positive(eccstate_t*);
static eccvalue_t nsopfn_negative(eccstate_t*);
static eccvalue_t nsopfn_invert(eccstate_t*);
static eccvalue_t nsopfn_logicalnot(eccstate_t*);
static eccvalue_t nsopfn_construct(eccstate_t*);
static eccvalue_t nsopfn_call(eccstate_t*);
static eccvalue_t nsopfn_eval(eccstate_t*);
static eccvalue_t nsopfn_incrementRef(eccstate_t*);
static eccvalue_t nsopfn_decrementRef(eccstate_t*);
static eccvalue_t nsopfn_postIncrementRef(eccstate_t*);
static eccvalue_t nsopfn_postDecrementRef(eccstate_t*);
static eccvalue_t nsopfn_addAssignRef(eccstate_t*);
static eccvalue_t nsopfn_minusAssignRef(eccstate_t*);
static eccvalue_t nsopfn_multiplyAssignRef(eccstate_t*);
static eccvalue_t nsopfn_divideAssignRef(eccstate_t*);
static eccvalue_t nsopfn_moduloAssignRef(eccstate_t*);
static eccvalue_t nsopfn_leftShiftAssignRef(eccstate_t*);
static eccvalue_t nsopfn_rightShiftAssignRef(eccstate_t*);
static eccvalue_t nsopfn_unsignedRightShiftAssignRef(eccstate_t*);
static eccvalue_t nsopfn_bitAndAssignRef(eccstate_t*);
static eccvalue_t nsopfn_bitXorAssignRef(eccstate_t*);
static eccvalue_t nsopfn_bitOrAssignRef(eccstate_t*);
static eccvalue_t nsopfn_debugger(eccstate_t*);
static eccvalue_t nsopfn_try(eccstate_t*);
static eccvalue_t nsopfn_throw(eccstate_t*);
static eccvalue_t nsopfn_with(eccstate_t*);
static eccvalue_t nsopfn_next(eccstate_t*);
static eccvalue_t nsopfn_nextIf(eccstate_t*);
static eccvalue_t nsopfn_autoreleaseExpression(eccstate_t*);
static eccvalue_t nsopfn_autoreleaseDiscard(eccstate_t*);
static eccvalue_t nsopfn_expression(eccstate_t*);
static eccvalue_t nsopfn_discard(eccstate_t*);
static eccvalue_t nsopfn_discardN(eccstate_t*);
static eccvalue_t nsopfn_jump(eccstate_t*);
static eccvalue_t nsopfn_jumpIf(eccstate_t*);
static eccvalue_t nsopfn_jumpIfNot(eccstate_t*);
static eccvalue_t nsopfn_repopulate(eccstate_t*);
static eccvalue_t nsopfn_result(eccstate_t*);
static eccvalue_t nsopfn_resultVoid(eccstate_t*);
static eccvalue_t nsopfn_switchOp(eccstate_t*);
static eccvalue_t nsopfn_breaker(eccstate_t*);
static eccvalue_t nsopfn_iterate(eccstate_t*);
static eccvalue_t nsopfn_iterateLessRef(eccstate_t*);
static eccvalue_t nsopfn_iterateMoreRef(eccstate_t*);
static eccvalue_t nsopfn_iterateLessOrEqualRef(eccstate_t*);
static eccvalue_t nsopfn_iterateMoreOrEqualRef(eccstate_t*);
static eccvalue_t nsopfn_iterateInRef(eccstate_t*);
eccvalue_t eccop_doTrapOp(eccstate_t* context, int offset);



const struct eccpseudonsop_t ECCNSOperand = {
    nsopfn_make,
    nsopfn_tochars,
    nsopfn_callfunctionarguments,
    nsopfn_callfunctionva,
    nsopfn_noop,
    nsopfn_value,
    nsopfn_valueConstRef,
    nsopfn_text,
    nsopfn_function,
    nsopfn_object,
    nsopfn_array,
    nsopfn_regexp,
    nsopfn_this,
    nsopfn_createLocalRef,
    nsopfn_getLocalRefOrNull,
    nsopfn_getLocalRef,
    nsopfn_getLocal,
    nsopfn_setLocal,
    nsopfn_deleteLocal,
    nsopfn_getLocalSlotRef,
    nsopfn_getLocalSlot,
    nsopfn_setLocalSlot,
    nsopfn_deleteLocalSlot,
    nsopfn_getParentSlotRef,
    nsopfn_getParentSlot,
    nsopfn_setParentSlot,
    nsopfn_deleteParentSlot,
    nsopfn_getMemberRef,
    nsopfn_getMember,
    nsopfn_setMember,
    nsopfn_callMember,
    nsopfn_deleteMember,
    nsopfn_getPropertyRef,
    nsopfn_getProperty,
    nsopfn_setProperty,
    nsopfn_callProperty,
    nsopfn_deleteProperty,
    nsopfn_pushEnvironment,
    nsopfn_popEnvironment,
    nsopfn_exchange,
    nsopfn_typeOf,
    nsopfn_equal,
    nsopfn_notEqual,
    nsopfn_identical,
    nsopfn_notIdentical,
    nsopfn_less,
    nsopfn_lessOrEqual,
    nsopfn_more,
    nsopfn_moreOrEqual,
    nsopfn_instanceOf,
    nsopfn_in,
    nsopfn_add,
    nsopfn_minus,
    nsopfn_multiply,
    nsopfn_divide,
    nsopfn_modulo,
    nsopfn_leftShift,
    nsopfn_rightShift,
    nsopfn_unsignedRightShift,
    nsopfn_bitwiseAnd,
    nsopfn_bitwiseXor,
    nsopfn_bitwiseOr,
    nsopfn_logicalAnd,
    nsopfn_logicalOr,
    nsopfn_positive,
    nsopfn_negative,
    nsopfn_invert,
    nsopfn_logicalnot,
    nsopfn_construct,
    nsopfn_call,
    nsopfn_eval,
    nsopfn_incrementRef,
    nsopfn_decrementRef,
    nsopfn_postIncrementRef,
    nsopfn_postDecrementRef,
    nsopfn_addAssignRef,
    nsopfn_minusAssignRef,
    nsopfn_multiplyAssignRef,
    nsopfn_divideAssignRef,
    nsopfn_moduloAssignRef,
    nsopfn_leftShiftAssignRef,
    nsopfn_rightShiftAssignRef,
    nsopfn_unsignedRightShiftAssignRef,
    nsopfn_bitAndAssignRef,
    nsopfn_bitXorAssignRef,
    nsopfn_bitOrAssignRef,
    nsopfn_debugger,
    nsopfn_try,
    nsopfn_throw,
    nsopfn_with,
    nsopfn_next,
    nsopfn_nextIf,
    nsopfn_autoreleaseExpression,
    nsopfn_autoreleaseDiscard,
    nsopfn_expression,
    nsopfn_discard,
    nsopfn_discardN,
    nsopfn_jump,
    nsopfn_jumpIf,
    nsopfn_jumpIfNot,
    nsopfn_repopulate,
    nsopfn_result,
    nsopfn_resultVoid,
    nsopfn_switchOp,
    nsopfn_breaker,
    nsopfn_iterate,
    nsopfn_iterateLessRef,
    nsopfn_iterateMoreRef,
    nsopfn_iterateLessOrEqualRef,
    nsopfn_iterateMoreOrEqualRef,
    nsopfn_iterateInRef,
    {},
};

static int g_ops_debugging = 0;

eccvalue_t eccop_doTrapOp(eccstate_t* context, int offset)
{
    const ecctextstring_t* text = opmac_text(offset);
    if(g_ops_debugging && text->bytes && text->length)
    {
        ECCNSEnv.newline();
        ECCNSContext.printBacktrace(context);
        ECCNSScript.printTextInput(context->ecc, *text, 1);
        trap();
    }
    return opmac_next();
}

#if (ECC_OPS_DEBUG == 1)
static eccvalue_t eccop_trapOp(eccstate_t* context, int offset)
{
    /*     gdb/lldb infos: p usage()     */
    return eccop_doTrapOp(context, offset);
}
#else
    #define eccop_trapOp(context, offset) opmac_next()
#endif


static eccvalue_t eccop_retain(eccvalue_t value)
{
    if(value.type == ECC_VALTYPE_CHARS)
        ++value.data.chars->referenceCount;
    if(value.type >= ECC_VALTYPE_OBJECT)
        ++value.data.object->referenceCount;

    return value;
}

static eccvalue_t eccop_release(eccvalue_t value)
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

static int eccop_intLessThan(int32_t a, int32_t b)
{
    return a < b;
}

static int eccop_intLessEqual(int32_t a, int32_t b)
{
    return a <= b;
}

static int eccop_intGreaterThan(int32_t a, int32_t b)
{
    return a > b;
}

static int eccop_intGreaterEqual(int32_t a, int32_t b)
{
    return a >= b;
}

static int eccop_testIntegerWontOFPos(int32_t a, int32_t positive)
{
    return a <= INT32_MAX - positive;
}

static int eccop_testIntWontOFNeg(int32_t a, int32_t negative)
{
    return a >= INT32_MIN - negative;
}

// MARK: - Static Members

// MARK: - Methods

eccoperand_t nsopfn_make(const eccnativefuncptr_t native, eccvalue_t value, ecctextstring_t text)
{
    return (eccoperand_t){ native, value, text };
}

const char* nsopfn_tochars(const eccnativefuncptr_t native)
{
    static const struct
    {
        const char* name;
        const eccnativefuncptr_t native;
    } functionList[] = {
        { "noop", nsopfn_noop },
        { "value", nsopfn_value },
        { "valueConstRef", nsopfn_valueConstRef },
        { "text", nsopfn_text },
        { "function", nsopfn_function },
        { "object", nsopfn_object },
        { "array", nsopfn_array },
        { "regexp", nsopfn_regexp },
        { "this", nsopfn_this },
        { "createLocalRef", nsopfn_createLocalRef },
        { "getLocalRefOrNull", nsopfn_getLocalRefOrNull },
        { "getLocalRef", nsopfn_getLocalRef },
        { "getLocal", nsopfn_getLocal },
        { "setLocal", nsopfn_setLocal },
        { "deleteLocal", nsopfn_deleteLocal },
        { "getLocalSlotRef", nsopfn_getLocalSlotRef },
        { "getLocalSlot", nsopfn_getLocalSlot },
        { "setLocalSlot", nsopfn_setLocalSlot },
        { "deleteLocalSlot", nsopfn_deleteLocalSlot },
        { "getParentSlotRef", nsopfn_getParentSlotRef },
        { "getParentSlot", nsopfn_getParentSlot },
        { "setParentSlot", nsopfn_setParentSlot },
        { "deleteParentSlot", nsopfn_deleteParentSlot },
        { "getMemberRef", nsopfn_getMemberRef },
        { "getMember", nsopfn_getMember },
        { "setMember", nsopfn_setMember },
        { "callMember", nsopfn_callMember },
        { "deleteMember", nsopfn_deleteMember },
        { "getPropertyRef", nsopfn_getPropertyRef },
        { "getProperty", nsopfn_getProperty },
        { "setProperty", nsopfn_setProperty },
        { "callProperty", nsopfn_callProperty },
        { "deleteProperty", nsopfn_deleteProperty },
        { "pushEnvironment", nsopfn_pushEnvironment },
        { "popEnvironment", nsopfn_popEnvironment },
        { "exchange", nsopfn_exchange },
        { "typeOf", nsopfn_typeOf },
        { "equal", nsopfn_equal },
        { "notEqual", nsopfn_notEqual },
        { "identical", nsopfn_identical },
        { "notIdentical", nsopfn_notIdentical },
        { "less", nsopfn_less },
        { "lessOrEqual", nsopfn_lessOrEqual },
        { "more", nsopfn_more },
        { "moreOrEqual", nsopfn_moreOrEqual },
        { "instanceOf", nsopfn_instanceOf },
        { "in", nsopfn_in },
        { "add", nsopfn_add },
        { "minus", nsopfn_minus },
        { "multiply", nsopfn_multiply },
        { "divide", nsopfn_divide },
        { "modulo", nsopfn_modulo },
        { "leftShift", nsopfn_leftShift },
        { "rightShift", nsopfn_rightShift },
        { "unsignedRightShift", nsopfn_unsignedRightShift },
        { "bitwiseAnd", nsopfn_bitwiseAnd },
        { "bitwiseXor", nsopfn_bitwiseXor },
        { "bitwiseOr", nsopfn_bitwiseOr },
        { "logicalAnd", nsopfn_logicalAnd },
        { "logicalOr", nsopfn_logicalOr },
        { "positive", nsopfn_positive },
        { "negative", nsopfn_negative },
        { "invert", nsopfn_invert },
        { "not", nsopfn_logicalnot },
        { "construct", nsopfn_construct },
        { "call", nsopfn_call },
        { "eval", nsopfn_eval },
        { "incrementRef", nsopfn_incrementRef },
        { "decrementRef", nsopfn_decrementRef },
        { "postIncrementRef", nsopfn_postIncrementRef },
        { "postDecrementRef", nsopfn_postDecrementRef },
        { "addAssignRef", nsopfn_addAssignRef },
        { "minusAssignRef", nsopfn_minusAssignRef },
        { "multiplyAssignRef", nsopfn_multiplyAssignRef },
        { "divideAssignRef", nsopfn_divideAssignRef },
        { "moduloAssignRef", nsopfn_moduloAssignRef },
        { "leftShiftAssignRef", nsopfn_leftShiftAssignRef },
        { "rightShiftAssignRef", nsopfn_rightShiftAssignRef },
        { "unsignedRightShiftAssignRef", nsopfn_unsignedRightShiftAssignRef },
        { "bitAndAssignRef", nsopfn_bitAndAssignRef },
        { "bitXorAssignRef", nsopfn_bitXorAssignRef },
        { "bitOrAssignRef", nsopfn_bitOrAssignRef },
        { "debugger", nsopfn_debugger },
        { "try", nsopfn_try },
        { "throw", nsopfn_throw },
        { "with", nsopfn_with },
        { "next", nsopfn_next },
        { "nextIf", nsopfn_nextIf },
        { "autoreleaseExpression", nsopfn_autoreleaseExpression },
        { "autoreleaseDiscard", nsopfn_autoreleaseDiscard },
        { "expression", nsopfn_expression },
        { "discard", nsopfn_discard },
        { "discardN", nsopfn_discardN },
        { "jump", nsopfn_jump },
        { "jumpIf", nsopfn_jumpIf },
        { "jumpIfNot", nsopfn_jumpIfNot },
        { "repopulate", nsopfn_repopulate },
        { "result", nsopfn_result },
        { "resultVoid", nsopfn_resultVoid },
        { "switchOp", nsopfn_switchOp },
        { "breaker", nsopfn_breaker },
        { "iterate", nsopfn_iterate },
        { "iterateLessRef", nsopfn_iterateLessRef },
        { "iterateMoreRef", nsopfn_iterateMoreRef },
        { "iterateLessOrEqualRef", nsopfn_iterateLessOrEqualRef },
        { "iterateMoreOrEqualRef", nsopfn_iterateMoreOrEqualRef },
        { "iterateInRef", nsopfn_iterateInRef },
    };

    int index;
    for(index = 0; index < (int)sizeof(functionList); ++index)
        if(functionList[index].native == native)
            return functionList[index].name;

    assert(0);
    return "unknow";
}


static eccvalue_t eccop_nextOpValue(eccstate_t* context)
{
    eccvalue_t value = opmac_next();
    value.flags &= ~(ECC_VALFLAG_READONLY | ECC_VALFLAG_HIDDEN | ECC_VALFLAG_SEALED);
    return value;
}

static eccvalue_t eccop_replaceRefValue(eccvalue_t* ref, eccvalue_t value)
{
    ref->data = value.data;
    ref->type = value.type;
    return value;
}

static inline eccvalue_t eccop_callOps(eccstate_t* context, eccobject_t* environment)
{
    if(context->depth >= context->ecc->maximumCallDepth)
        ECCNSContext.rangeError(context, ECCNSChars.create("maximum depth exceeded"));

    //	if (!context->parent->strictMode)
    //		if (context->thisvalue.type == ECC_VALTYPE_UNDEFINED || context->thisvalue.type == ECC_VALTYPE_NULL)
    //			context->thisvalue = ECCNSValue.object(&context->ecc->global->environment);

    context->environment = environment;
    return context->ops->native(context);
}

static inline eccvalue_t eccop_callOpsRelease(eccstate_t* context, eccobject_t* environment)
{
    eccvalue_t result;
    uint16_t index, count;

    result = eccop_callOps(context, environment);

    for(index = 2, count = environment->hashmapCount; index < count; ++index)
        eccop_release(environment->hashmap[index].value);

    return result;
}

static inline void eccop_makeEnvWithArgs(eccobject_t* environment, eccobject_t* arguments, int32_t parameterCount)
{
    int32_t index = 0;
    int argumentCount = arguments->elementCount;

    eccop_replaceRefValue(&environment->hashmap[2].value, eccop_retain(ECCNSValue.object(arguments)));

    if(argumentCount <= parameterCount)
        for(; index < argumentCount; ++index)
            environment->hashmap[index + 3].value = eccop_retain(arguments->element[index].value);
    else
    {
        for(; index < parameterCount; ++index)
            environment->hashmap[index + 3].value = eccop_retain(arguments->element[index].value);
    }
}

static inline void eccop_makeEnvAndArgsWithVA(eccobject_t* environment, int32_t parameterCount, int32_t argumentCount, va_list ap)
{
    int32_t index = 0;
    eccobject_t* arguments = ECCNSArguments.createSized(argumentCount);

    eccop_replaceRefValue(&environment->hashmap[2].value, eccop_retain(ECCNSValue.object(arguments)));

    if(argumentCount <= parameterCount)
        for(; index < argumentCount; ++index)
            environment->hashmap[index + 3].value = arguments->element[index].value = eccop_retain(va_arg(ap, eccvalue_t));
    else
    {
        for(; index < parameterCount; ++index)
            environment->hashmap[index + 3].value = arguments->element[index].value = eccop_retain(va_arg(ap, eccvalue_t));

        for(; index < argumentCount; ++index)
            arguments->element[index].value = eccop_retain(va_arg(ap, eccvalue_t));
    }
}

static inline void eccop_populateEnvWithVA(eccobject_t* environment, int32_t parameterCount, int32_t argumentCount, va_list ap)
{
    int32_t index = 0;
    if(argumentCount <= parameterCount)
        for(; index < argumentCount; ++index)
            environment->hashmap[index + 3].value = eccop_retain(va_arg(ap, eccvalue_t));
    else
        for(; index < parameterCount; ++index)
            environment->hashmap[index + 3].value = eccop_retain(va_arg(ap, eccvalue_t));
}

static inline void eccop_makeStackEnvAndArgsWithOps(eccstate_t* context, eccobject_t* environment, eccobject_t* arguments, int32_t parameterCount, int32_t argumentCount)
{
    int32_t index = 0;

    eccop_replaceRefValue(&environment->hashmap[2].value, ECCNSValue.object(arguments));

    if(argumentCount <= parameterCount)
        for(; index < argumentCount; ++index)
            environment->hashmap[index + 3].value = arguments->element[index].value = eccop_retain(eccop_nextOpValue(context));
    else
    {
        for(; index < parameterCount; ++index)
            environment->hashmap[index + 3].value = arguments->element[index].value = eccop_retain(eccop_nextOpValue(context));

        for(; index < argumentCount; ++index)
            arguments->element[index].value = eccop_nextOpValue(context);
    }
}

static inline void eccop_makeEnvAndArgsWithOps(eccstate_t* context, eccobject_t* environment, eccobject_t* arguments, int32_t parameterCount, int32_t argumentCount)
{
    eccop_makeStackEnvAndArgsWithOps(context, environment, arguments, parameterCount, argumentCount);

    eccop_retain(ECCNSValue.object(arguments));

    if(argumentCount > parameterCount)
    {
        int32_t index = parameterCount;
        for(; index < argumentCount; ++index)
            eccop_retain(arguments->element[index].value);
    }
}

static inline void eccop_populateEnvWithOps(eccstate_t* context, eccobject_t* environment, int32_t parameterCount, int32_t argumentCount)
{
    int32_t index = 0;
    if(argumentCount <= parameterCount)
        for(; index < argumentCount; ++index)
            environment->hashmap[index + 3].value = eccop_retain(eccop_nextOpValue(context));
    else
    {
        for(; index < parameterCount; ++index)
            environment->hashmap[index + 3].value = eccop_retain(eccop_nextOpValue(context));

        for(; index < argumentCount; ++index)
            opmac_next();
    }
}

eccvalue_t nsopfn_callfunctionarguments(eccstate_t* context, eccctxoffsettype_t offset, eccobjfunction_t* function, eccvalue_t thisval, eccobject_t* arguments)
{
    eccstate_t subContext = {
        .ops = function->oplist->ops,
        .thisvalue = thisval,
        .parent = context,
        .ecc = context->ecc,
        .argumentOffset = offset,
        .depth = context->depth + 1,
        .strictMode = function->flags & ECC_SCRIPTFUNCFLAG_STRICTMODE,
        .refObject = function->refObject,
    };

    if(function->flags & ECC_SCRIPTFUNCFLAG_NEEDHEAP)
    {
        eccobject_t* environment = ECCNSObject.copy(&function->environment);

        if(function->flags & ECC_SCRIPTFUNCFLAG_NEEDARGUMENTS)
        {
            eccobject_t* copy = ECCNSArguments.createSized(arguments->elementCount);
            memcpy(copy->element, arguments->element, sizeof(*copy->element) * copy->elementCount);
            arguments = copy;
        }
        eccop_makeEnvWithArgs(environment, arguments, function->parameterCount);

        return eccop_callOps(&subContext, environment);
    }
    else
    {
        eccobject_t environment = function->environment;
        ecchashmap_t hashmap[function->environment.hashmapCapacity];

        memcpy(hashmap, function->environment.hashmap, sizeof(hashmap));
        environment.hashmap = hashmap;
        eccop_makeEnvWithArgs(&environment, arguments, function->parameterCount);

        return eccop_callOpsRelease(&subContext, &environment);
    }
}

eccvalue_t nsopfn_callfunctionva(eccstate_t* context, eccctxoffsettype_t offset, eccobjfunction_t* function, eccvalue_t thisval, int argumentCount, va_list ap)
{
    eccstate_t subContext = {
        .ops = function->oplist->ops,
        .thisvalue = thisval,
        .parent = context,
        .ecc = context->ecc,
        .argumentOffset = offset,
        .depth = context->depth + 1,
        .strictMode = function->flags & ECC_SCRIPTFUNCFLAG_STRICTMODE,
        .refObject = function->refObject,
    };

    if(function->flags & ECC_SCRIPTFUNCFLAG_NEEDHEAP)
    {
        eccobject_t* environment = ECCNSObject.copy(&function->environment);

        if(function->flags & ECC_SCRIPTFUNCFLAG_NEEDARGUMENTS)
        {
            eccop_makeEnvAndArgsWithVA(environment, function->parameterCount, argumentCount, ap);

            if(!context->strictMode)
            {
                ECCNSObject.addMember(environment->hashmap[2].value.data.object, ECC_ConstKey_callee, eccop_retain(ECCNSValue.function(function)), ECC_VALFLAG_HIDDEN);
                ECCNSObject.addMember(environment->hashmap[2].value.data.object, ECC_ConstKey_length, ECCNSValue.integer(argumentCount), ECC_VALFLAG_HIDDEN);
            }
        }
        else
            eccop_populateEnvWithVA(environment, function->parameterCount, argumentCount, ap);

        return eccop_callOps(&subContext, environment);
    }
    else
    {
        eccobject_t environment = function->environment;
        ecchashmap_t hashmap[function->environment.hashmapCapacity];

        memcpy(hashmap, function->environment.hashmap, sizeof(hashmap));
        environment.hashmap = hashmap;

        eccop_populateEnvWithVA(&environment, function->parameterCount, argumentCount, ap);

        return eccop_callOpsRelease(&subContext, &environment);
    }
}

static inline eccvalue_t eccop_callFunction(eccstate_t* context, eccobjfunction_t* const function, eccvalue_t thisval, int32_t argumentCount, int construct)
{
    eccstate_t subContext = {
        .ops = function->oplist->ops,
        .thisvalue = thisval,
        .parent = context,
        .ecc = context->ecc,
        .construct = construct,
        .depth = context->depth + 1,
        .strictMode = function->flags & ECC_SCRIPTFUNCFLAG_STRICTMODE,
        .refObject = function->refObject,
    };

    if(function->flags & ECC_SCRIPTFUNCFLAG_NEEDHEAP)
    {
        eccobject_t* environment = ECCNSObject.copy(&function->environment);

        if(function->flags & ECC_SCRIPTFUNCFLAG_NEEDARGUMENTS)
        {
            eccop_makeEnvAndArgsWithOps(context, environment, ECCNSArguments.createSized(argumentCount), function->parameterCount, argumentCount);

            if(!context->strictMode)
            {
                ECCNSObject.addMember(environment->hashmap[2].value.data.object, ECC_ConstKey_callee, eccop_retain(ECCNSValue.function(function)), ECC_VALFLAG_HIDDEN);
                ECCNSObject.addMember(environment->hashmap[2].value.data.object, ECC_ConstKey_length, ECCNSValue.integer(argumentCount), ECC_VALFLAG_HIDDEN);
            }
        }
        else
            eccop_populateEnvWithOps(context, environment, function->parameterCount, argumentCount);

        return eccop_callOps(&subContext, environment);
    }
    else if(function->flags & ECC_SCRIPTFUNCFLAG_NEEDARGUMENTS)
    {
        eccobject_t environment = function->environment;
        eccobject_t arguments = ECCNSObject.identity;
        ecchashmap_t hashmap[function->environment.hashmapCapacity];
        eccobjelement_t element[argumentCount];

        memcpy(hashmap, function->environment.hashmap, sizeof(hashmap));
        environment.hashmap = hashmap;
        arguments.element = element;
        arguments.elementCount = argumentCount;
        eccop_makeStackEnvAndArgsWithOps(context, &environment, &arguments, function->parameterCount, argumentCount);

        return eccop_callOpsRelease(&subContext, &environment);
    }
    else
    {
        eccobject_t environment = function->environment;
        ecchashmap_t hashmap[function->environment.hashmapCapacity];

        memcpy(hashmap, function->environment.hashmap, sizeof(hashmap));
        environment.hashmap = hashmap;
        eccop_populateEnvWithOps(context, &environment, function->parameterCount, argumentCount);

        return eccop_callOpsRelease(&subContext, &environment);
    }
}

static inline eccvalue_t eccop_callValue(eccstate_t* context, eccvalue_t value, eccvalue_t thisval, int32_t argumentCount, int construct, const ecctextstring_t* textCall)
{
    eccvalue_t result;
    const ecctextstring_t* parentTextCall = context->textCall;

    if(value.type != ECC_VALTYPE_FUNCTION)
        ECCNSContext.typeError(context, ECCNSChars.create("'%.*s' is not a function", context->text->length, context->text->bytes));

    context->textCall = textCall;

    if(value.data.function->flags & ECC_SCRIPTFUNCFLAG_USEBOUNDTHIS)
        result = eccop_callFunction(context, value.data.function, value.data.function->boundThis, argumentCount, construct);
    else
        result = eccop_callFunction(context, value.data.function, thisval, argumentCount, construct);

    context->textCall = parentTextCall;
    return result;
}

eccvalue_t nsopfn_construct(eccstate_t* context)
{
    const ecctextstring_t* textCall = opmac_text(0);
    const ecctextstring_t* text = opmac_text(1);
    int32_t argumentCount = opmac_value().data.integer;
    eccvalue_t value, *prototype, object, function = opmac_next();

    if(function.type != ECC_VALTYPE_FUNCTION)
        goto error;

    prototype = ECCNSObject.member(&function.data.function->object, ECC_ConstKey_prototype, 0);
    if(!prototype)
        goto error;

    if(!ECCNSValue.isObject(*prototype))
        object = ECCNSValue.object(ECCNSObject.create(ECC_Prototype_Object));
    else if(prototype->type == ECC_VALTYPE_FUNCTION)
    {
        object = ECCNSValue.object(ECCNSObject.create(NULL));
        object.data.object->prototype = &prototype->data.function->object;
    }
    else if(prototype->type == ECC_VALTYPE_OBJECT)
        object = ECCNSValue.object(ECCNSObject.create(prototype->data.object));
    else
        object = ECCValConstUndefined;

    ECCNSContext.setText(context, text);
    value = eccop_callValue(context, function, object, argumentCount, 1, textCall);

    if(ECCNSValue.isObject(value))
        return value;
    else
        return object;

error:
    context->textCall = textCall;
    ECCNSContext.setTextIndex(context, ECC_CTXINDEXTYPE_FUNC);
    ECCNSContext.typeError(context, ECCNSChars.create("'%.*s' is not a constructor", text->length, text->bytes));
}

eccvalue_t nsopfn_call(eccstate_t* context)
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
        thisval = ECCNSValue.objectValue(seek->environment);
    }
    else
        thisval = ECCValConstUndefined;

    ECCNSContext.setText(context, text);
    return eccop_callValue(context, value, thisval, argumentCount, 0, textCall);
}

eccvalue_t nsopfn_eval(eccstate_t* context)
{
    eccvalue_t value;
    eccioinput_t* input;
    int32_t argumentCount = opmac_value().data.integer;
    eccstate_t subContext = {
        .parent = context,
        .thisvalue = context->thisvalue,
        .environment = context->environment,
        .ecc = context->ecc,
        .depth = context->depth + 1,
        .strictMode = context->strictMode,
    };

    if(!argumentCount)
        return ECCValConstUndefined;

    value = opmac_next();
    while(--argumentCount)
        opmac_next();

    if(!ECCNSValue.isString(value) || !ECCNSValue.isPrimitive(value))
        return value;

    input = ECCNSInput.createFromBytes(ECCNSValue.stringBytes(&value), ECCNSValue.stringLength(&value), "(eval)");
    ECCNSScript.evalInputWithContext(context->ecc, input, &subContext);

    value = context->ecc->result;
    context->ecc->result = ECCValConstUndefined;
    return value;
}

// Expression

eccvalue_t nsopfn_noop(eccstate_t* context)
{
    (void)context;
    return ECCValConstUndefined;
}

eccvalue_t nsopfn_value(eccstate_t* context)
{
    return opmac_value();
}

eccvalue_t nsopfn_valueConstRef(eccstate_t* context)
{
    return ECCNSValue.reference((eccvalue_t*)&opmac_value());
}

eccvalue_t nsopfn_text(eccstate_t* context)
{
    return ECCNSValue.text(opmac_text(0));
}

eccvalue_t nsopfn_regexp(eccstate_t* context)
{
    const ecctextstring_t* text = opmac_text(0);
    eccobjerror_t* error = NULL;
    ecccharbuffer_t* chars = ECCNSChars.createWithBytes(text->length, text->bytes);
    eccobjregexp_t* regexp = ECCNSRegExp.create(chars, &error, context->ecc->sloppyMode ? ECC_REGEXOPT_ALLOWUNICODEFLAGS : 0);
    if(error)
    {
        error->text.bytes = text->bytes + (error->text.bytes - chars->bytes);
        ECCNSContext.doThrow(context, ECCNSValue.error(error));
    }
    return ECCNSValue.regexp(regexp);
}

eccvalue_t nsopfn_function(eccstate_t* context)
{
    eccobject_t* prototype;
    eccvalue_t value = opmac_value(), result;

    eccobjfunction_t* function = ECCNSFunction.copy(value.data.function);
    function->object.prototype = &value.data.function->object;
    function->environment.prototype = context->environment;
    if(context->refObject)
    {
        ++context->refObject->referenceCount;
        function->refObject = context->refObject;
    }

    prototype = ECCNSObject.create(ECC_Prototype_Object);
    ECCNSFunction.linkPrototype(function, ECCNSValue.object(prototype), ECC_VALFLAG_SEALED);

    ++prototype->referenceCount;
    ++context->environment->referenceCount;
    ++function->object.referenceCount;

    result = ECCNSValue.function(function);
    result.flags = value.flags;
    return result;
}

eccvalue_t nsopfn_object(eccstate_t* context)
{
    eccobject_t* object = ECCNSObject.create(ECC_Prototype_Object);
    eccvalue_t property, value;
    uint32_t count;

    object->flags |= ECC_OBJFLAG_MARK;

    for(count = opmac_value().data.integer; count--;)
    {
        property = opmac_next();
        value = eccop_retain(eccop_nextOpValue(context));

        if(property.type == ECC_VALTYPE_KEY)
            ECCNSObject.addMember(object, property.data.key, value, 0);
        else if(property.type == ECC_VALTYPE_INTEGER)
            ECCNSObject.addElement(object, property.data.integer, value, 0);
    }
    return ECCNSValue.object(object);
}

eccvalue_t nsopfn_array(eccstate_t* context)
{
    uint32_t length = opmac_value().data.integer;
    eccobject_t* object = ECCNSArray.createSized(length);
    eccvalue_t value;
    uint32_t index;

    object->flags |= ECC_OBJFLAG_MARK;

    for(index = 0; index < length; ++index)
    {
        value = eccop_retain(eccop_nextOpValue(context));
        object->element[index].value = value;
    }
    return ECCNSValue.object(object);
}

eccvalue_t nsopfn_this(eccstate_t* context)
{
    return context->thisvalue;
}

static eccvalue_t* eccop_localRef(eccstate_t* context, eccindexkey_t key, const ecctextstring_t* text, int required)
{
    eccvalue_t* ref;

    ref = ECCNSObject.member(context->environment, key, 0);

    if(!context->strictMode)
    {
        context->inEnvironmentObject = context->refObject != NULL;
        if(!ref && context->refObject)
        {
            context->inEnvironmentObject = 0;
            ref = ECCNSObject.member(context->refObject, key, 0);
        }
    }

    if(!ref && required)
    {
        ECCNSContext.setText(context, text);
        ECCNSContext.referenceError(context, ECCNSChars.create("'%.*s' is not defined", ECCNSKey.textOf(key)->length, ECCNSKey.textOf(key)->bytes));
    }
    return ref;
}

eccvalue_t nsopfn_createLocalRef(eccstate_t* context)
{
    eccindexkey_t key = opmac_value().data.key;
    eccvalue_t* ref = eccop_localRef(context, key, opmac_text(0), context->strictMode);

    if(!ref)
        ref = ECCNSObject.addMember(&context->ecc->global->environment, key, ECCValConstUndefined, 0);

    return ECCNSValue.reference(ref);
}

eccvalue_t nsopfn_getLocalRefOrNull(eccstate_t* context)
{
    return ECCNSValue.reference(eccop_localRef(context, opmac_value().data.key, opmac_text(0), 0));
}

eccvalue_t nsopfn_getLocalRef(eccstate_t* context)
{
    return ECCNSValue.reference(eccop_localRef(context, opmac_value().data.key, opmac_text(0), 1));
}

eccvalue_t nsopfn_getLocal(eccstate_t* context)
{
    return *eccop_localRef(context, opmac_value().data.key, opmac_text(0), 1);
}

eccvalue_t nsopfn_setLocal(eccstate_t* context)
{
    const ecctextstring_t* text = opmac_text(0);
    eccindexkey_t key = opmac_value().data.key;
    eccvalue_t value = opmac_next();

    eccvalue_t* ref = eccop_localRef(context, key, text, context->strictMode);

    if(!ref)
        ref = ECCNSObject.addMember(&context->ecc->global->environment, key, ECCValConstUndefined, 0);

    if(ref->flags & ECC_VALFLAG_READONLY)
        return value;

    eccop_retain(value);
    eccop_release(*ref);
    eccop_replaceRefValue(ref, value);
    return value;
}

eccvalue_t nsopfn_deleteLocal(eccstate_t* context)
{
    eccvalue_t* ref = eccop_localRef(context, opmac_value().data.key, opmac_text(0), 0);

    if(!ref)
        return ECCValConstTrue;

    if(ref->flags & ECC_VALFLAG_SEALED)
        return ECCValConstFalse;

    *ref = ECCValConstNone;
    return ECCValConstTrue;
}

eccvalue_t nsopfn_getLocalSlotRef(eccstate_t* context)
{
    return ECCNSValue.reference(&context->environment->hashmap[opmac_value().data.integer].value);
}

eccvalue_t nsopfn_getLocalSlot(eccstate_t* context)
{
    return context->environment->hashmap[opmac_value().data.integer].value;
}

eccvalue_t nsopfn_setLocalSlot(eccstate_t* context)
{
    int32_t slot = opmac_value().data.integer;
    eccvalue_t value = opmac_next();
    eccvalue_t* ref = &context->environment->hashmap[slot].value;

    if(ref->flags & ECC_VALFLAG_READONLY)
        return value;

    eccop_retain(value);
    eccop_release(*ref);
    eccop_replaceRefValue(ref, value);
    return value;
}

eccvalue_t nsopfn_deleteLocalSlot(eccstate_t* context)
{
    (void)context;
    return ECCValConstFalse;
}

eccvalue_t nsopfn_getParentSlotRef(eccstate_t* context)
{
    int32_t slot = opmac_value().data.integer & 0xffff;
    int32_t count = opmac_value().data.integer >> 16;
    eccobject_t* object = context->environment;
    while(count--)
        object = object->prototype;

    return ECCNSValue.reference(&object->hashmap[slot].value);
}

eccvalue_t nsopfn_getParentSlot(eccstate_t* context)
{
    return *nsopfn_getParentSlotRef(context).data.reference;
}

eccvalue_t nsopfn_setParentSlot(eccstate_t* context)
{
    const ecctextstring_t* text = opmac_text(0);
    eccvalue_t* ref = nsopfn_getParentSlotRef(context).data.reference;
    eccvalue_t value = opmac_next();
    if(ref->flags & ECC_VALFLAG_READONLY)
    {
        if(context->strictMode)
        {
            ecctextstring_t property = *ECCNSKey.textOf(ref->key);
            ECCNSContext.setText(context, text);
            ECCNSContext.typeError(context, ECCNSChars.create("'%.*s' is read-only", property.length, property.bytes));
        }
    }
    else
    {
        eccop_retain(value);
        eccop_release(*ref);
        eccop_replaceRefValue(ref, value);
    }
    return value;
}

eccvalue_t nsopfn_deleteParentSlot(eccstate_t* context)
{
    (void)context;
    return ECCValConstFalse;
}

static void eccop_prepareObject(eccstate_t* context, eccvalue_t* object)
{
    const ecctextstring_t* textObject = opmac_text(1);
    *object = opmac_next();

    if(ECCNSValue.isPrimitive(*object))
    {
        ECCNSContext.setText(context, textObject);
        *object = ECCNSValue.toObject(context, *object);
    }
}

eccvalue_t nsopfn_getMemberRef(eccstate_t* context)
{
    const ecctextstring_t* text = opmac_text(0);
    eccindexkey_t key = opmac_value().data.key;
    eccvalue_t object, *ref;

    eccop_prepareObject(context, &object);

    context->refObject = object.data.object;
    ref = ECCNSObject.member(object.data.object, key, ECC_VALFLAG_ASOWN);

    if(!ref)
    {
        if(object.data.object->flags & ECC_OBJFLAG_SEALED)
        {
            ECCNSContext.setText(context, text);
            ECCNSContext.typeError(context, ECCNSChars.create("object is not extensible"));
        }
        ref = ECCNSObject.addMember(object.data.object, key, ECCValConstUndefined, 0);
    }

    return ECCNSValue.reference(ref);
}

eccvalue_t nsopfn_getMember(eccstate_t* context)
{
    eccindexkey_t key = opmac_value().data.key;
    eccvalue_t object;

    eccop_prepareObject(context, &object);

    return ECCNSObject.getMember(context, object.data.object, key);
}

eccvalue_t nsopfn_setMember(eccstate_t* context)
{
    const ecctextstring_t* text = opmac_text(0);
    eccindexkey_t key = opmac_value().data.key;
    eccvalue_t object, value;

    eccop_prepareObject(context, &object);
    value = eccop_retain(opmac_next());

    ECCNSContext.setText(context, text);
    ECCNSObject.putMember(context, object.data.object, key, value);

    return value;
}

eccvalue_t nsopfn_callMember(eccstate_t* context)
{
    const ecctextstring_t* textCall = opmac_text(0);
    int32_t argumentCount = opmac_value().data.integer;
    const ecctextstring_t* text = &(++context->ops)->text;
    eccindexkey_t key = opmac_value().data.key;
    eccvalue_t object;

    eccop_prepareObject(context, &object);

    ECCNSContext.setText(context, text);
    return eccop_callValue(context, ECCNSObject.getMember(context, object.data.object, key), object, argumentCount, 0, textCall);
}

eccvalue_t nsopfn_deleteMember(eccstate_t* context)
{
    const ecctextstring_t* text = opmac_text(0);
    eccindexkey_t key = opmac_value().data.key;
    eccvalue_t object;
    int result;

    eccop_prepareObject(context, &object);

    result = ECCNSObject.deleteMember(object.data.object, key);
    if(!result && context->strictMode)
    {
        ECCNSContext.setText(context, text);
        ECCNSContext.typeError(context, ECCNSChars.create("'%.*s' is non-configurable", ECCNSKey.textOf(key)->length, ECCNSKey.textOf(key)->bytes));
    }

    return ECCNSValue.truth(result);
}

static void eccop_prepareObjectProperty(eccstate_t* context, eccvalue_t* object, eccvalue_t* property)
{
    const ecctextstring_t* textProperty;

    eccop_prepareObject(context, object);

    textProperty = opmac_text(1);
    *property = opmac_next();

    if(ECCNSValue.isObject(*property))
    {
        ECCNSContext.setText(context, textProperty);
        *property = ECCNSValue.toPrimitive(context, *property, ECC_VALHINT_STRING);
    }
}

eccvalue_t nsopfn_getPropertyRef(eccstate_t* context)
{
    const ecctextstring_t* text = opmac_text(1);
    eccvalue_t object, property;
    eccvalue_t* ref;

    eccop_prepareObjectProperty(context, &object, &property);

    context->refObject = object.data.object;
    ref = ECCNSObject.property(object.data.object, property, ECC_VALFLAG_ASOWN);

    if(!ref)
    {
        if(object.data.object->flags & ECC_OBJFLAG_SEALED)
        {
            ECCNSContext.setText(context, text);
            ECCNSContext.typeError(context, ECCNSChars.create("object is not extensible"));
        }
        ref = ECCNSObject.addProperty(object.data.object, property, ECCValConstUndefined, 0);
    }

    return ECCNSValue.reference(ref);
}

eccvalue_t nsopfn_getProperty(eccstate_t* context)
{
    eccvalue_t object, property;

    eccop_prepareObjectProperty(context, &object, &property);

    return ECCNSObject.getProperty(context, object.data.object, property);
}

eccvalue_t nsopfn_setProperty(eccstate_t* context)
{
    const ecctextstring_t* text = opmac_text(0);
    eccvalue_t object, property, value;

    eccop_prepareObjectProperty(context, &object, &property);

    value = eccop_retain(opmac_next());
    value.flags = 0;

    ECCNSContext.setText(context, text);
    ECCNSObject.putProperty(context, object.data.object, property, value);

    return value;
}

eccvalue_t nsopfn_callProperty(eccstate_t* context)
{
    const ecctextstring_t* textCall = opmac_text(0);
    int32_t argumentCount = opmac_value().data.integer;
    const ecctextstring_t* text = &(++context->ops)->text;
    eccvalue_t object, property;

    eccop_prepareObjectProperty(context, &object, &property);

    ECCNSContext.setText(context, text);
    return eccop_callValue(context, ECCNSObject.getProperty(context, object.data.object, property), object, argumentCount, 0, textCall);
}

eccvalue_t nsopfn_deleteProperty(eccstate_t* context)
{
    const ecctextstring_t* text = opmac_text(0);
    eccvalue_t object, property;
    int result;

    eccop_prepareObjectProperty(context, &object, &property);

    result = ECCNSObject.deleteProperty(object.data.object, property);
    if(!result && context->strictMode)
    {
        eccvalue_t string = ECCNSValue.toString(context, property);
        ECCNSContext.setText(context, text);
        ECCNSContext.typeError(context, ECCNSChars.create("'%.*s' is non-configurable", ECCNSValue.stringLength(&string), ECCNSValue.stringBytes(&string)));
    }
    return ECCNSValue.truth(result);
}

eccvalue_t nsopfn_pushEnvironment(eccstate_t* context)
{
    if(context->refObject)
        context->refObject = ECCNSObject.create(context->refObject);
    else
        context->environment = ECCNSObject.create(context->environment);

    return opmac_value();
}

eccvalue_t nsopfn_popEnvironment(eccstate_t* context)
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

eccvalue_t nsopfn_exchange(eccstate_t* context)
{
    eccvalue_t value = opmac_value();
    opmac_next();
    return value;
}

eccvalue_t nsopfn_typeOf(eccstate_t* context)
{
    eccvalue_t value = opmac_next();
    if(value.type == ECC_VALTYPE_REFERENCE)
    {
        eccvalue_t* ref = value.data.reference;
        if(!ref)
            return ECCNSValue.text(&ECC_ConstString_Undefined);
        else
            value = *ref;
    }
    return ECCNSValue.toType(value);
}

#define prepareAB                               \
    const ecctextstring_t* text = opmac_text(1);    \
    eccvalue_t a = opmac_next();                    \
    const ecctextstring_t* textAlt = opmac_text(1); \
    eccvalue_t b = opmac_next();

eccvalue_t nsopfn_equal(eccstate_t* context)
{
    prepareAB

    if(a.type == ECC_VALTYPE_BINARY && b.type == ECC_VALTYPE_BINARY) return ECCNSValue.truth(a.data.binary == b.data.binary);
    else
    {
        ECCNSContext.setTexts(context, text, textAlt);
        return ECCNSValue.equals(context, a, b);
    }
}

eccvalue_t nsopfn_notEqual(eccstate_t* context)
{
    prepareAB

    if(a.type == ECC_VALTYPE_BINARY && b.type == ECC_VALTYPE_BINARY) return ECCNSValue.truth(a.data.binary != b.data.binary);
    else
    {
        ECCNSContext.setTexts(context, text, textAlt);
        return ECCNSValue.truth(!ECCNSValue.isTrue(ECCNSValue.equals(context, a, b)));
    }
}

eccvalue_t nsopfn_identical(eccstate_t* context)
{
    prepareAB

    if(a.type == ECC_VALTYPE_BINARY && b.type == ECC_VALTYPE_BINARY) return ECCNSValue.truth(a.data.binary == b.data.binary);
    else
    {
        ECCNSContext.setTexts(context, text, textAlt);
        return ECCNSValue.same(context, a, b);
    }
}

eccvalue_t nsopfn_notIdentical(eccstate_t* context)
{
    prepareAB

    if(a.type == ECC_VALTYPE_BINARY && b.type == ECC_VALTYPE_BINARY) return ECCNSValue.truth(a.data.binary != b.data.binary);
    else
    {
        ECCNSContext.setTexts(context, text, textAlt);
        return ECCNSValue.truth(!ECCNSValue.isTrue(ECCNSValue.same(context, a, b)));
    }
}

eccvalue_t nsopfn_less(eccstate_t* context)
{
    prepareAB

    if(a.type == ECC_VALTYPE_BINARY && b.type == ECC_VALTYPE_BINARY) return ECCNSValue.truth(a.data.binary < b.data.binary);
    else
    {
        ECCNSContext.setTexts(context, text, textAlt);
        return ECCNSValue.less(context, a, b);
    }
}

eccvalue_t nsopfn_lessOrEqual(eccstate_t* context)
{
    prepareAB

    if(a.type == ECC_VALTYPE_BINARY && b.type == ECC_VALTYPE_BINARY) return ECCNSValue.truth(a.data.binary <= b.data.binary);
    else
    {
        ECCNSContext.setTexts(context, text, textAlt);
        return ECCNSValue.lessOrEqual(context, a, b);
    }
}

eccvalue_t nsopfn_more(eccstate_t* context)
{
    prepareAB

    if(a.type == ECC_VALTYPE_BINARY && b.type == ECC_VALTYPE_BINARY) return ECCNSValue.truth(a.data.binary > b.data.binary);
    else
    {
        ECCNSContext.setTexts(context, text, textAlt);
        return ECCNSValue.more(context, a, b);
    }
}

eccvalue_t nsopfn_moreOrEqual(eccstate_t* context)
{
    prepareAB

    if(a.type == ECC_VALTYPE_BINARY && b.type == ECC_VALTYPE_BINARY) return ECCNSValue.truth(a.data.binary >= b.data.binary);
    else
    {
        ECCNSContext.setTexts(context, text, textAlt);
        return ECCNSValue.moreOrEqual(context, a, b);
    }
}

eccvalue_t nsopfn_instanceOf(eccstate_t* context)
{
    eccvalue_t a = opmac_next();
    const ecctextstring_t* textAlt = opmac_text(1);
    eccvalue_t b = opmac_next();

    if(b.type != ECC_VALTYPE_FUNCTION)
    {
        ECCNSContext.setText(context, textAlt);
        ECCNSContext.typeError(context, ECCNSChars.create("'%.*s' is not a function", textAlt->length, textAlt->bytes));
    }

    b = ECCNSObject.getMember(context, b.data.object, ECC_ConstKey_prototype);
    if(!ECCNSValue.isObject(b))
    {
        ECCNSContext.setText(context, textAlt);
        ECCNSContext.typeError(context, ECCNSChars.create("'%.*s'.prototype not an object", textAlt->length, textAlt->bytes));
    }

    if(ECCNSValue.isObject(a))
    {
        eccobject_t* object;

        object = a.data.object;
        while((object = object->prototype))
            if(object == b.data.object)
                return ECCValConstTrue;
    }
    return ECCValConstFalse;
}

eccvalue_t nsopfn_in(eccstate_t* context)
{
    eccvalue_t property = opmac_next();
    eccvalue_t object = opmac_next();
    eccvalue_t* ref;

    if(!ECCNSValue.isObject(object))
        ECCNSContext.typeError(context, ECCNSChars.create("'%.*s' not an object", context->ops->text.length, context->ops->text.bytes));

    ref = ECCNSObject.property(object.data.object, ECCNSValue.toString(context, property), 0);

    return ECCNSValue.truth(ref != NULL);
}

eccvalue_t nsopfn_add(eccstate_t* context)
{
    prepareAB

    if(a.type == ECC_VALTYPE_BINARY && b.type == ECC_VALTYPE_BINARY)
    {
        a.data.binary += b.data.binary;
        return a;
    }
    else
    {
        ECCNSContext.setTexts(context, text, textAlt);
        return ECCNSValue.add(context, a, b);
    }
}

eccvalue_t nsopfn_minus(eccstate_t* context)
{
    eccvalue_t a = opmac_next();
    eccvalue_t b = opmac_next();
    if(a.type == ECC_VALTYPE_BINARY && b.type == ECC_VALTYPE_BINARY)
    {
        a.data.binary -= b.data.binary;
        return a;
    }
    else
        return ECCNSValue.binary(ECCNSValue.toBinary(context, a).data.binary - ECCNSValue.toBinary(context, b).data.binary);
}

eccvalue_t nsopfn_multiply(eccstate_t* context)
{
    eccvalue_t a = opmac_next();
    eccvalue_t b = opmac_next();
    if(a.type == ECC_VALTYPE_BINARY && b.type == ECC_VALTYPE_BINARY)
    {
        a.data.binary *= b.data.binary;
        return a;
    }
    else
        return ECCNSValue.binary(ECCNSValue.toBinary(context, a).data.binary * ECCNSValue.toBinary(context, b).data.binary);
}

eccvalue_t nsopfn_divide(eccstate_t* context)
{
    eccvalue_t a = opmac_next();
    eccvalue_t b = opmac_next();
    if(a.type == ECC_VALTYPE_BINARY && b.type == ECC_VALTYPE_BINARY)
    {
        a.data.binary /= b.data.binary;
        return a;
    }
    else
        return ECCNSValue.binary(ECCNSValue.toBinary(context, a).data.binary / ECCNSValue.toBinary(context, b).data.binary);
}

eccvalue_t nsopfn_modulo(eccstate_t* context)
{
    eccvalue_t a = opmac_next();
    eccvalue_t b = opmac_next();
    if(a.type == ECC_VALTYPE_BINARY && b.type == ECC_VALTYPE_BINARY)
    {
        a.data.binary = fmod(a.data.binary, b.data.binary);
        return a;
    }
    else
        return ECCNSValue.binary(fmod(ECCNSValue.toBinary(context, a).data.binary, ECCNSValue.toBinary(context, b).data.binary));
}

eccvalue_t nsopfn_leftShift(eccstate_t* context)
{
    eccvalue_t a = opmac_next();
    eccvalue_t b = opmac_next();
    return ECCNSValue.binary(ECCNSValue.toInteger(context, a).data.integer << (uint32_t)ECCNSValue.toInteger(context, b).data.integer);
}

eccvalue_t nsopfn_rightShift(eccstate_t* context)
{
    eccvalue_t a = opmac_next();
    eccvalue_t b = opmac_next();
    return ECCNSValue.binary(ECCNSValue.toInteger(context, a).data.integer >> (uint32_t)ECCNSValue.toInteger(context, b).data.integer);
}

eccvalue_t nsopfn_unsignedRightShift(eccstate_t* context)
{
    eccvalue_t a = opmac_next();
    eccvalue_t b = opmac_next();
    return ECCNSValue.binary((uint32_t)ECCNSValue.toInteger(context, a).data.integer >> (uint32_t)ECCNSValue.toInteger(context, b).data.integer);
}

eccvalue_t nsopfn_bitwiseAnd(eccstate_t* context)
{
    eccvalue_t a = opmac_next();
    eccvalue_t b = opmac_next();
    return ECCNSValue.binary(ECCNSValue.toInteger(context, a).data.integer & ECCNSValue.toInteger(context, b).data.integer);
}

eccvalue_t nsopfn_bitwiseXor(eccstate_t* context)
{
    eccvalue_t a = opmac_next();
    eccvalue_t b = opmac_next();
    return ECCNSValue.binary(ECCNSValue.toInteger(context, a).data.integer ^ ECCNSValue.toInteger(context, b).data.integer);
}

eccvalue_t nsopfn_bitwiseOr(eccstate_t* context)
{
    eccvalue_t a = opmac_next();
    eccvalue_t b = opmac_next();
    return ECCNSValue.binary(ECCNSValue.toInteger(context, a).data.integer | ECCNSValue.toInteger(context, b).data.integer);
}

eccvalue_t nsopfn_logicalAnd(eccstate_t* context)
{
    const int32_t opCount = opmac_value().data.integer;
    eccvalue_t value = opmac_next();

    if(!ECCNSValue.isTrue(value))
    {
        context->ops += opCount;
        return value;
    }
    else
        return opmac_next();
}

eccvalue_t nsopfn_logicalOr(eccstate_t* context)
{
    const int32_t opCount = opmac_value().data.integer;
    eccvalue_t value = opmac_next();

    if(ECCNSValue.isTrue(value))
    {
        context->ops += opCount;
        return value;
    }
    else
        return opmac_next();
}

eccvalue_t nsopfn_positive(eccstate_t* context)
{
    eccvalue_t a = opmac_next();
    if(a.type == ECC_VALTYPE_BINARY)
        return a;
    else
        return ECCNSValue.toBinary(context, a);
}

eccvalue_t nsopfn_negative(eccstate_t* context)
{
    eccvalue_t a = opmac_next();
    if(a.type == ECC_VALTYPE_BINARY)
        return ECCNSValue.binary(-a.data.binary);
    else
        return ECCNSValue.binary(-ECCNSValue.toBinary(context, a).data.binary);
}

eccvalue_t nsopfn_invert(eccstate_t* context)
{
    eccvalue_t a = opmac_next();
    return ECCNSValue.binary(~ECCNSValue.toInteger(context, a).data.integer);
}

eccvalue_t nsopfn_logicalnot(eccstate_t * context)
{
    eccvalue_t a = opmac_next();
    return ECCNSValue.truth(!ECCNSValue.isTrue(a));
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
        ECCNSContext.setText(context, text);                                                               \
        a = ECCNSValue.toBinary(context, eccop_release(ECCNSObject.getValue(context, context->refObject, ref))); \
        result = OP;                                                                                       \
        ECCNSObject.putValue(context, context->refObject, ref, a);                                         \
        return ECCNSValue.binary(result);                                                                  \
    }                                                                                                      \
    else if(a.type != ECC_VALTYPE_BINARY)                                                                  \
        a = ECCNSValue.toBinary(context, eccop_release(a));                                                      \
                                                                                                           \
    result = OP;                                                                                           \
    eccop_replaceRefValue(ref, a);                                                                               \
    context->refObject = refObject;                                                                        \
    return ECCNSValue.binary(result);

eccvalue_t nsopfn_incrementRef(eccstate_t* context){ unaryBinaryOpRef(++a.data.binary) }

eccvalue_t nsopfn_decrementRef(eccstate_t* context){ unaryBinaryOpRef(--a.data.binary) }

eccvalue_t nsopfn_postIncrementRef(eccstate_t* context){ unaryBinaryOpRef(a.data.binary++) }

eccvalue_t nsopfn_postDecrementRef(eccstate_t* context){ unaryBinaryOpRef(a.data.binary--) }

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
        ECCNSContext.setText(context, text);                                       \
        a = CONV(context, ECCNSObject.getValue(context, context->refObject, ref)); \
        OP;                                                                        \
        return ECCNSObject.putValue(context, context->refObject, ref, a);          \
    }                                                                              \
    else if(a.type != TYPE)                                                        \
        a = CONV(context, eccop_release(a));                                             \
                                                                                   \
    OP;                                                                            \
    eccop_replaceRefValue(ref, a);                                                       \
    context->refObject = refObject;                                                \
    return a;

#define assignBinaryOpRef(OP) assignOpRef(OP, ECC_VALTYPE_BINARY, ECCNSValue.toBinary)
#define assignIntegerOpRef(OP) assignOpRef(OP, ECC_VALTYPE_INTEGER, ECCNSValue.toInteger)

eccvalue_t nsopfn_addAssignRef(eccstate_t* context)
{
    eccobject_t* refObject = context->refObject;
    const ecctextstring_t* text = opmac_text(1);
    eccvalue_t* ref = opmac_next().data.reference;
    const ecctextstring_t* textAlt = opmac_text(1);
    eccvalue_t a, b = opmac_next();

    ECCNSContext.setTexts(context, text, textAlt);

    a = *ref;
    if(a.flags & (ECC_VALFLAG_READONLY | ECC_VALFLAG_ACCESSOR))
    {
        a = ECCNSObject.getValue(context, context->refObject, ref);
        a = eccop_retain(ECCNSValue.add(context, a, b));
        return ECCNSObject.putValue(context, context->refObject, ref, a);
    }

    if(a.type == ECC_VALTYPE_BINARY && b.type == ECC_VALTYPE_BINARY)
    {
        a.data.binary += b.data.binary;
        return *ref = a;
    }

    a = eccop_retain(ECCNSValue.add(context, eccop_release(a), b));
    eccop_replaceRefValue(ref, a);
    context->refObject = refObject;
    return a;
}

eccvalue_t nsopfn_minusAssignRef(eccstate_t* context)
{
    assignBinaryOpRef(a.data.binary -= b.data.binary);
}

eccvalue_t nsopfn_multiplyAssignRef(eccstate_t* context)
{
    assignBinaryOpRef(a.data.binary *= b.data.binary);
}

eccvalue_t nsopfn_divideAssignRef(eccstate_t* context)
{
    assignBinaryOpRef(a.data.binary /= b.data.binary);
}

eccvalue_t nsopfn_moduloAssignRef(eccstate_t* context)
{
    assignBinaryOpRef(a.data.binary = fmod(a.data.binary, b.data.binary));
}

eccvalue_t nsopfn_leftShiftAssignRef(eccstate_t* context)
{
    assignIntegerOpRef(a.data.integer <<= (uint32_t)b.data.integer);
}

eccvalue_t nsopfn_rightShiftAssignRef(eccstate_t* context)
{
    assignIntegerOpRef(a.data.integer >>= (uint32_t)b.data.integer);
}

eccvalue_t nsopfn_unsignedRightShiftAssignRef(eccstate_t* context)
{
    assignIntegerOpRef(a.data.integer = (uint32_t)a.data.integer >> (uint32_t)b.data.integer);
}

eccvalue_t nsopfn_bitAndAssignRef(eccstate_t* context)
{
    assignIntegerOpRef(a.data.integer &= (uint32_t)b.data.integer);
}

eccvalue_t nsopfn_bitXorAssignRef(eccstate_t* context)
{
    assignIntegerOpRef(a.data.integer ^= (uint32_t)b.data.integer);
}

eccvalue_t nsopfn_bitOrAssignRef(eccstate_t* context)
{
    assignIntegerOpRef(a.data.integer |= (uint32_t)b.data.integer);
}

// MARK: Statement

eccvalue_t nsopfn_debugger(eccstate_t* context)
{
    g_ops_debugging = 1;
    return eccop_trapOp(context, 0);
}

io_libecc_ecc_useframe eccvalue_t nsopfn_try(eccstate_t* context)
{
    eccobject_t* environment = context->environment;
    eccobject_t* refObject = context->refObject;
    const eccoperand_t* end = context->ops + opmac_value().data.integer;
    eccindexkey_t key;

    const eccoperand_t* volatile rethrowOps = NULL;
    volatile int rethrow = 0, breaker = 0;
    volatile eccvalue_t value = ECCValConstUndefined;
    eccvalue_t finallyValue;
    uint32_t indices[3];

    ECCNSMemoryPool.getIndices(indices);

    if(!setjmp(*ECCNSScript.pushEnv(context->ecc)))// try
        value = opmac_next();
    else
    {
        value = context->ecc->result;
        context->ecc->result = ECCValConstUndefined;
        rethrowOps = context->ops;

        if(!rethrow)// catch
        {
            ECCNSMemoryPool.unreferenceFromIndices(indices);

            rethrow = 1;
            context->ops = end + 1;// bypass catch jump
            context->environment = environment;
            context->refObject = refObject;
            key = opmac_next().data.key;

            if(!ECCNSKey.isEqual(key, ECC_ConstKey_none))
            {
                if(value.type == ECC_VALTYPE_FUNCTION)
                {
                    value.data.function->flags |= ECC_SCRIPTFUNCFLAG_USEBOUNDTHIS;
                    value.data.function->boundThis = ECCNSValue.object(context->environment);
                }
                ECCNSObject.addMember(context->environment, key, value, ECC_VALFLAG_SEALED);
                value = opmac_next();// execute until noop
                rethrow = 0;
                if(context->breaker)
                    nsopfn_popEnvironment(context);
            }
        }
        else// rethrow
            nsopfn_popEnvironment(context);
    }

    ECCNSScript.popEnv(context->ecc);

    breaker = context->breaker;
    context->breaker = 0;
    context->ops = end;// op[end] = ECCNSOperand.jump, to after catch
    finallyValue = opmac_next();// jump to after catch, and execute until noop

    if(context->breaker) /* return breaker */
        return finallyValue;
    else if(rethrow)
    {
        context->ops = rethrowOps;
        ECCNSContext.doThrow(context, eccop_retain(value));
    }
    else if(breaker)
    {
        context->breaker = breaker;
        return value;
    }
    else
        return opmac_next();
}

io_libecc_ecc_noreturn eccvalue_t nsopfn_throw(eccstate_t * context)
{
    context->ecc->text = *opmac_text(1);
    ECCNSContext.doThrow(context, eccop_retain(eccop_trapOp(context, 0)));
}

eccvalue_t nsopfn_with(eccstate_t* context)
{
    eccobject_t* environment = context->environment;
    eccobject_t* refObject = context->refObject;
    eccobject_t* object = ECCNSValue.toObject(context, opmac_next()).data.object;
    eccvalue_t value;

    if(!refObject)
        context->refObject = context->environment;

    context->environment = object;
    value = opmac_next();
    context->environment = environment;
    context->refObject = refObject;

    if(context->breaker)
        return value;
    else
        return opmac_next();
}

eccvalue_t nsopfn_next(eccstate_t* context)
{
    return opmac_next();
}

eccvalue_t nsopfn_nextIf(eccstate_t* context)
{
    eccvalue_t value = opmac_value();

    if(!ECCNSValue.isTrue(eccop_trapOp(context, 1)))
        return value;

    return opmac_next();
}

eccvalue_t nsopfn_autoreleaseExpression(eccstate_t* context)
{
    uint32_t indices[3];

    ECCNSMemoryPool.getIndices(indices);
    eccop_release(context->ecc->result);
    context->ecc->result = eccop_retain(eccop_trapOp(context, 1));
    ECCNSMemoryPool.collectUnreferencedFromIndices(indices);
    return opmac_next();
}

eccvalue_t nsopfn_autoreleaseDiscard(eccstate_t* context)
{
    uint32_t indices[3];

    ECCNSMemoryPool.getIndices(indices);
    eccop_trapOp(context, 1);
    ECCNSMemoryPool.collectUnreferencedFromIndices(indices);
    return opmac_next();
}

eccvalue_t nsopfn_expression(eccstate_t* context)
{
    eccop_release(context->ecc->result);
    context->ecc->result = eccop_retain(eccop_trapOp(context, 1));
    return opmac_next();
}

eccvalue_t nsopfn_discard(eccstate_t* context)
{
    eccop_trapOp(context, 1);
    return opmac_next();
}

eccvalue_t nsopfn_discardN(eccstate_t* context)
{
    switch(opmac_value().data.integer)
    {
        default:
            {
                ECCNSScript.fatal("Invalid discardN : %d", opmac_value().data.integer);
            }
            /* fallthrough */
        case 16:
            {
                eccop_trapOp(context, 1);
            }
            /* fallthrough */
        case 15:
            {
                eccop_trapOp(context, 1);
            }
            /* fallthrough */
        case 14:
            {
                eccop_trapOp(context, 1);
            }
            /* fallthrough */
        case 13:
            {
                eccop_trapOp(context, 1);
            }
            /* fallthrough */
        case 12:
            {
                eccop_trapOp(context, 1);
            }
            /* fallthrough */
        case 11:
            {
                eccop_trapOp(context, 1);
            }
            /* fallthrough */
        case 10:
            {
                eccop_trapOp(context, 1);
            }
            /* fallthrough */
        case 9:
            {
                eccop_trapOp(context, 1);
            }
            /* fallthrough */
        case 8:
            {
                eccop_trapOp(context, 1);
            }
            /* fallthrough */
        case 7:
            {
                eccop_trapOp(context, 1);
            }
            /* fallthrough */
        case 6:
            {
                eccop_trapOp(context, 1);
            }
            /* fallthrough */
        case 5:
            {
                eccop_trapOp(context, 1);
            }
            /* fallthrough */
        case 4:
            {
                eccop_trapOp(context, 1);
            }
            /* fallthrough */
        case 3:
            {
                eccop_trapOp(context, 1);
            }
            /* fallthrough */
        case 2:
            {
                eccop_trapOp(context, 1);
            }
            /* fallthrough */
        case 1:
            {
                eccop_trapOp(context, 1);
            }
            /* fallthrough */
    }
    return opmac_next();
}

eccvalue_t nsopfn_jump(eccstate_t* context)
{
    int32_t offset = opmac_value().data.integer;
    context->ops += offset;
    return opmac_next();
}

eccvalue_t nsopfn_jumpIf(eccstate_t* context)
{
    int32_t offset = opmac_value().data.integer;
    eccvalue_t value;

    value = eccop_trapOp(context, 1);
    if(ECCNSValue.isTrue(value))
        context->ops += offset;

    return opmac_next();
}

eccvalue_t nsopfn_jumpIfNot(eccstate_t* context)
{
    int32_t offset = opmac_value().data.integer;
    eccvalue_t value;

    value = eccop_trapOp(context, 1);
    if(!ECCNSValue.isTrue(value))
        context->ops += offset;

    return opmac_next();
}

eccvalue_t nsopfn_result(eccstate_t* context)
{
    eccvalue_t result = eccop_trapOp(context, 0);
    context->breaker = -1;
    return result;
}

eccvalue_t nsopfn_repopulate(eccstate_t* context)
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
            eccop_release(context->environment->hashmap[index].value);
            hashmap[index].value = eccop_retain(opmac_next());
        }

        if(index < context->environment->hashmapCapacity)
        {
            for(; index < context->environment->hashmapCapacity; ++index)
            {
                eccop_release(context->environment->hashmap[index].value);
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

eccvalue_t nsopfn_resultVoid(eccstate_t* context)
{
    eccvalue_t result = ECCValConstUndefined;
    context->breaker = -1;
    return result;
}

eccvalue_t nsopfn_switchOp(eccstate_t* context)
{
    int32_t offset = opmac_value().data.integer;
    const eccoperand_t* nextOps = context->ops + offset;
    eccvalue_t value, caseValue;
    const ecctextstring_t* text = opmac_text(1);

    value = eccop_trapOp(context, 1);

    while(context->ops < nextOps)
    {
        const ecctextstring_t* textAlt = opmac_text(1);
        caseValue = opmac_next();

        ECCNSContext.setTexts(context, text, textAlt);
        if(ECCNSValue.isTrue(ECCNSValue.same(context, value, caseValue)))
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
        ECCNSMemoryPool.getIndices(indices);                         \
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
            ECCNSMemoryPool.collectUnreferencedFromIndices(indices); \
            context->ops = nextOps;                                 \
        }                                                           \
    }

eccvalue_t nsopfn_breaker(eccstate_t* context)
{
    context->breaker = opmac_value().data.integer;
    return ECCValConstUndefined;
}

eccvalue_t nsopfn_iterate(eccstate_t* context)
{
    const eccoperand_t* startOps = context->ops;
    const eccoperand_t* endOps = startOps;
    const eccoperand_t* nextOps = startOps + 1;
    eccvalue_t value;
    int32_t skipOp = opmac_value().data.integer;

    context->ops = nextOps + skipOp;

    while(ECCNSValue.isTrue(opmac_next()))
        stepIteration(value, nextOps, break);

    context->ops = endOps;

    return opmac_next();
}

static eccvalue_t eccop_iterateIntegerRef(eccstate_t* context,
                                    int (*compareInteger)(int32_t, int32_t),
                                    int (*wontOverflow)(int32_t, int32_t),
                                    eccvalue_t (*compareValue)(eccstate_t*, eccvalue_t, eccvalue_t),
                                    eccvalue_t (*valueStep)(eccstate_t*, eccvalue_t, eccvalue_t))
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
        eccvalue_t integerValue = ECCNSValue.toInteger(context, *indexRef);
        if(indexRef->data.binary == integerValue.data.integer)
            *indexRef = integerValue;
    }

    if(countRef->type == ECC_VALTYPE_BINARY && countRef->data.binary >= INT32_MIN && countRef->data.binary <= INT32_MAX)
    {
        eccvalue_t integerValue = ECCNSValue.toInteger(context, *countRef);
        if(countRef->data.binary == integerValue.data.integer)
            *countRef = integerValue;
    }

    if(indexRef->type == ECC_VALTYPE_INTEGER && countRef->type == ECC_VALTYPE_INTEGER)
    {
        int32_t step = valueStep == ECCNSValue.add ? stepValue.data.integer : -stepValue.data.integer;
        if(!wontOverflow(countRef->data.integer, step - 1))
            goto deoptimize;

        for(; compareInteger(indexRef->data.integer, countRef->data.integer); indexRef->data.integer += step)
            stepIteration(value, nextOps, goto done);
    }

deoptimize:
    for(; ECCNSValue.isTrue(compareValue(context, *indexRef, *countRef)); *indexRef = valueStep(context, *indexRef, stepValue))
        stepIteration(value, nextOps, break);

done:
    context->refObject = refObject;
    context->ops = endOps;
    return opmac_next();
}

eccvalue_t nsopfn_iterateLessRef(eccstate_t* context)
{
    return eccop_iterateIntegerRef(context, eccop_intLessThan, eccop_testIntegerWontOFPos, ECCNSValue.less, ECCNSValue.add);
}

eccvalue_t nsopfn_iterateLessOrEqualRef(eccstate_t* context)
{
    return eccop_iterateIntegerRef(context, eccop_intLessEqual, eccop_testIntegerWontOFPos, ECCNSValue.lessOrEqual, ECCNSValue.add);
}

eccvalue_t nsopfn_iterateMoreRef(eccstate_t* context)
{
    return eccop_iterateIntegerRef(context, eccop_intGreaterThan, eccop_testIntWontOFNeg, ECCNSValue.more, ECCNSValue.subtract);
}

eccvalue_t nsopfn_iterateMoreOrEqualRef(eccstate_t* context)
{
    return eccop_iterateIntegerRef(context, eccop_intGreaterEqual, eccop_testIntWontOFNeg, ECCNSValue.moreOrEqual, ECCNSValue.subtract);
}

eccvalue_t nsopfn_iterateInRef(eccstate_t* context)
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

    if(ECCNSValue.isObject(target))
    {
        object = target.data.object;
        do
        {
            count = object->elementCount < object->elementCapacity ? object->elementCount : object->elementCapacity;
            for(index = 0; index < count; ++index)
            {
                eccobjelement_t* element = object->element + index;

                if(element->value.check != 1 || (element->value.flags & ECC_VALFLAG_HIDDEN))
                    continue;

                if(object != target.data.object && &element->value != ECCNSObject.element(target.data.object, index, 0))
                    continue;

                ECCNSChars.beginAppend(&chars);
                ECCNSChars.append(&chars, "%d", index);
                key = ECCNSChars.endAppend(&chars);
                eccop_replaceRefValue(ref, key);

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

                if(object != target.data.object && &hashmap->value != ECCNSObject.member(target.data.object, hashmap->value.key, 0))
                    continue;

                key = ECCNSValue.text(ECCNSKey.textOf(hashmap->value.key));
                eccop_replaceRefValue(ref, key);

                stepIteration(value, startOps, break);
            }
        } while((object = object->prototype));
    }

    context->refObject = refObject;
    context->ops = endOps;
    return opmac_next();
}
