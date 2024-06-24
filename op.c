//
//  op.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"

// MARK: - Private

#define nextOp() (++context->ops)->native(context)
#define opValue() (context->ops)->value
#define opText(O) &(context->ops + O)->text

#if DEBUG

	#if _MSC_VER
		#define trap() __debugbreak()
	#elif __GNUC__
		#if __i386__ || __x86_64__
			#define trap() __asm__ ("int $3")
		#elif __APPLE__
			#include <sys/syscall.h>
			#if __arm64__
				#define trap() __asm__ __volatile__ ("mov w0,%w0\n mov w1,%w1\n mov w16,%w2\n svc #128":: "r"(getpid()), "r"(SIGTRAP), "r"(SYS_kill): "x0", "x1", "x16", "cc")
			#elif __arm__
				#define trap() __asm__ __volatile__ ("mov r0, %0\n mov r1, %1\n mov r12, %2\n swi #128":: "r"(getpid()), "r"(SIGTRAP), "r"(SYS_kill): "r0", "r1", "r12", "cc")
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

static int debug = 0;

extern
void usage(void)
{
	ECCNSEnv.printColor(0, io_libecc_env_bold, "\n\t-- libecc: basic gdb/lldb commands --\n");
	ECCNSEnv.printColor(io_libecc_env_green, io_libecc_env_bold, "\tstep-in\n");
	fprintf(stderr, "\t  c\n");
	ECCNSEnv.printColor(io_libecc_env_green, io_libecc_env_bold, "\tcontinue\n");
	fprintf(stderr, "\t  p debug=0\n");
	fprintf(stderr, "\t  c\n\n");
}

static
eccvalue_t trapOp_(eccstate_t *context, int offset)
{
	const ecctextstring_t *text = opText(offset);
	if (debug && text->bytes && text->length)
	{
		ECCNSEnv.newline();
		ECCNSContext.printBacktrace(context);
		ECCNSScript.printTextInput(context->ecc, *text, 1);
		trap();
	}
	return nextOp();
}
#define _ return trapOp_(context, offset);

static
eccvalue_t trapOp(eccstate_t *context, int offset)
{
_/*     gdb/lldb infos: p usage()     */
}


#undef _

#else
	#define trapOp(context, offset) nextOp()
#endif

//

static struct io_libecc_Op make(const io_libecc_native_io_libecc_Function native, eccvalue_t value, ecctextstring_t text);
static const char* toChars(const io_libecc_native_io_libecc_Function native);
static eccvalue_t
callFunctionArguments(eccstate_t* const, enum io_libecc_context_Offset, struct io_libecc_Function* function, eccvalue_t this, eccobject_t* arguments);
static eccvalue_t
callFunctionVA(eccstate_t* const, enum io_libecc_context_Offset, struct io_libecc_Function* function, eccvalue_t this, int argumentCount, va_list ap);
static eccvalue_t noop(eccstate_t* const);
static eccvalue_t value(eccstate_t* const);
static eccvalue_t valueConstRef(eccstate_t* const);
static eccvalue_t text(eccstate_t* const);
static eccvalue_t function(eccstate_t* const);
static eccvalue_t object(eccstate_t* const);
static eccvalue_t array(eccstate_t* const);
static eccvalue_t regexp(eccstate_t* const);
static eccvalue_t this(eccstate_t* const);
static eccvalue_t createLocalRef(eccstate_t* const);
static eccvalue_t getLocalRefOrNull(eccstate_t* const);
static eccvalue_t getLocalRef(eccstate_t* const);
static eccvalue_t getLocal(eccstate_t* const);
static eccvalue_t setLocal(eccstate_t* const);
static eccvalue_t deleteLocal(eccstate_t* const);
static eccvalue_t getLocalSlotRef(eccstate_t* const);
static eccvalue_t getLocalSlot(eccstate_t* const);
static eccvalue_t setLocalSlot(eccstate_t* const);
static eccvalue_t deleteLocalSlot(eccstate_t* const);
static eccvalue_t getParentSlotRef(eccstate_t* const);
static eccvalue_t getParentSlot(eccstate_t* const);
static eccvalue_t setParentSlot(eccstate_t* const);
static eccvalue_t deleteParentSlot(eccstate_t* const);
static eccvalue_t getMemberRef(eccstate_t* const);
static eccvalue_t getMember(eccstate_t* const);
static eccvalue_t setMember(eccstate_t* const);
static eccvalue_t callMember(eccstate_t* const);
static eccvalue_t deleteMember(eccstate_t* const);
static eccvalue_t getPropertyRef(eccstate_t* const);
static eccvalue_t getProperty(eccstate_t* const);
static eccvalue_t setProperty(eccstate_t* const);
static eccvalue_t callProperty(eccstate_t* const);
static eccvalue_t deleteProperty(eccstate_t* const);
static eccvalue_t pushEnvironment(eccstate_t* const);
static eccvalue_t popEnvironment(eccstate_t* const);
static eccvalue_t exchange(eccstate_t* const);
static eccvalue_t typeOf(eccstate_t* const);
static eccvalue_t equal(eccstate_t* const);
static eccvalue_t notEqual(eccstate_t* const);
static eccvalue_t identical(eccstate_t* const);
static eccvalue_t notIdentical(eccstate_t* const);
static eccvalue_t less(eccstate_t* const);
static eccvalue_t lessOrEqual(eccstate_t* const);
static eccvalue_t more(eccstate_t* const);
static eccvalue_t moreOrEqual(eccstate_t* const);
static eccvalue_t instanceOf(eccstate_t* const);
static eccvalue_t in(eccstate_t* const);
static eccvalue_t add(eccstate_t* const);
static eccvalue_t minus(eccstate_t* const);
static eccvalue_t multiply(eccstate_t* const);
static eccvalue_t divide(eccstate_t* const);
static eccvalue_t modulo(eccstate_t* const);
static eccvalue_t leftShift(eccstate_t* const);
static eccvalue_t rightShift(eccstate_t* const);
static eccvalue_t unsignedRightShift(eccstate_t* const);
static eccvalue_t bitwiseAnd(eccstate_t* const);
static eccvalue_t bitwiseXor(eccstate_t* const);
static eccvalue_t bitwiseOr(eccstate_t* const);
static eccvalue_t logicalAnd(eccstate_t* const);
static eccvalue_t logicalOr(eccstate_t* const);
static eccvalue_t positive(eccstate_t* const);
static eccvalue_t negative(eccstate_t* const);
static eccvalue_t invert(eccstate_t* const);
static eccvalue_t not(eccstate_t* const);
static eccvalue_t construct(eccstate_t* const);
static eccvalue_t call(eccstate_t* const);
static eccvalue_t eval(eccstate_t* const);
static eccvalue_t incrementRef(eccstate_t* const);
static eccvalue_t decrementRef(eccstate_t* const);
static eccvalue_t postIncrementRef(eccstate_t* const);
static eccvalue_t postDecrementRef(eccstate_t* const);
static eccvalue_t addAssignRef(eccstate_t* const);
static eccvalue_t minusAssignRef(eccstate_t* const);
static eccvalue_t multiplyAssignRef(eccstate_t* const);
static eccvalue_t divideAssignRef(eccstate_t* const);
static eccvalue_t moduloAssignRef(eccstate_t* const);
static eccvalue_t leftShiftAssignRef(eccstate_t* const);
static eccvalue_t rightShiftAssignRef(eccstate_t* const);
static eccvalue_t unsignedRightShiftAssignRef(eccstate_t* const);
static eccvalue_t bitAndAssignRef(eccstate_t* const);
static eccvalue_t bitXorAssignRef(eccstate_t* const);
static eccvalue_t bitOrAssignRef(eccstate_t* const);
static eccvalue_t debugger(eccstate_t* const);
static eccvalue_t try(eccstate_t* const);
static eccvalue_t throw(eccstate_t* const);
static eccvalue_t with(eccstate_t* const);
static eccvalue_t next(eccstate_t* const);
static eccvalue_t nextIf(eccstate_t* const);
static eccvalue_t autoreleaseExpression(eccstate_t* const);
static eccvalue_t autoreleaseDiscard(eccstate_t* const);
static eccvalue_t expression(eccstate_t* const);
static eccvalue_t discard(eccstate_t* const);
static eccvalue_t discardN(eccstate_t* const);
static eccvalue_t jump(eccstate_t* const);
static eccvalue_t jumpIf(eccstate_t* const);
static eccvalue_t jumpIfNot(eccstate_t* const);
static eccvalue_t repopulate(eccstate_t* const);
static eccvalue_t result(eccstate_t* const);
static eccvalue_t resultVoid(eccstate_t* const);
static eccvalue_t switchOp(eccstate_t* const);
static eccvalue_t breaker(eccstate_t* const);
static eccvalue_t iterate(eccstate_t* const);
static eccvalue_t iterateLessRef(eccstate_t* const);
static eccvalue_t iterateMoreRef(eccstate_t* const);
static eccvalue_t iterateLessOrEqualRef(eccstate_t* const);
static eccvalue_t iterateMoreOrEqualRef(eccstate_t* const);
static eccvalue_t iterateInRef(eccstate_t* const);
const struct type_io_libecc_Op io_libecc_Op = {
    make,
    toChars,
    callFunctionArguments,
    callFunctionVA,
    noop,
    value,
    valueConstRef,
    text,
    function,
    object,
    array,
    regexp,
    this,
    createLocalRef,
    getLocalRefOrNull,
    getLocalRef,
    getLocal,
    setLocal,
    deleteLocal,
    getLocalSlotRef,
    getLocalSlot,
    setLocalSlot,
    deleteLocalSlot,
    getParentSlotRef,
    getParentSlot,
    setParentSlot,
    deleteParentSlot,
    getMemberRef,
    getMember,
    setMember,
    callMember,
    deleteMember,
    getPropertyRef,
    getProperty,
    setProperty,
    callProperty,
    deleteProperty,
    pushEnvironment,
    popEnvironment,
    exchange,
    typeOf,
    equal,
    notEqual,
    identical,
    notIdentical,
    less,
    lessOrEqual,
    more,
    moreOrEqual,
    instanceOf,
    in,
    add,
    minus,
    multiply,
    divide,
    modulo,
    leftShift,
    rightShift,
    unsignedRightShift,
    bitwiseAnd,
    bitwiseXor,
    bitwiseOr,
    logicalAnd,
    logicalOr,
    positive,
    negative,
    invert,
    not,
    construct,
    call,
    eval,
    incrementRef,
    decrementRef,
    postIncrementRef,
    postDecrementRef,
    addAssignRef,
    minusAssignRef,
    multiplyAssignRef,
    divideAssignRef,
    moduloAssignRef,
    leftShiftAssignRef,
    rightShiftAssignRef,
    unsignedRightShiftAssignRef,
    bitAndAssignRef,
    bitXorAssignRef,
    bitOrAssignRef,
    debugger,
    try,
    throw,
    with,
    next,
    nextIf,
    autoreleaseExpression,
    autoreleaseDiscard,
    expression,
    discard,
    discardN,
    jump,
    jumpIf,
    jumpIfNot,
    repopulate,
    result,
    resultVoid,
    switchOp,
    breaker,
    iterate,
    iterateLessRef,
    iterateMoreRef,
    iterateLessOrEqualRef,
    iterateMoreOrEqualRef,
    iterateInRef,
};

static
eccvalue_t retain (eccvalue_t value)
{
	if (value.type == ECC_VALTYPE_CHARS)
		++value.data.chars->referenceCount;
	if (value.type >= ECC_VALTYPE_OBJECT)
		++value.data.object->referenceCount;
	
	return value;
}

static
eccvalue_t release (eccvalue_t value)
{
	if (value.type == ECC_VALTYPE_CHARS)
		--value.data.chars->referenceCount;
	if (value.type >= ECC_VALTYPE_OBJECT)
		--value.data.object->referenceCount;
	
	return value;
}

static
int integerLess(int32_t a, int32_t b)
{
	return a < b;
}

static
int integerLessOrEqual(int32_t a, int32_t b)
{
	return a <= b;
}

static
int integerMore(int32_t a, int32_t b)
{
	return a > b;
}

static
int integerMoreOrEqual(int32_t a, int32_t b)
{
	return a >= b;
}

static
int integerWontOverflowPositive(int32_t a, int32_t positive)
{
	return a <= INT32_MAX - positive;
}

static
int integerWontOverflowNegative(int32_t a, int32_t negative)
{
	return a >= INT32_MIN - negative;
}

// MARK: - Static Members

// MARK: - Methods

struct io_libecc_Op make (const io_libecc_native_io_libecc_Function native, eccvalue_t value, ecctextstring_t text)
{
	return (struct io_libecc_Op){ native, value, text };
}

const char * toChars (const io_libecc_native_io_libecc_Function native)
{
struct
{
    const char* name;
    const io_libecc_native_io_libecc_Function native;
} static const functionList[] = {
    { "noop", noop },
    { "value", value },
    { "valueConstRef", valueConstRef },
    { "text", text },
    { "function", function },
    { "object", object },
    { "array", array },
    { "regexp", regexp },
    { "this", this },
    { "createLocalRef", createLocalRef },
    { "getLocalRefOrNull", getLocalRefOrNull },
    { "getLocalRef", getLocalRef },
    { "getLocal", getLocal },
    { "setLocal", setLocal },
    { "deleteLocal", deleteLocal },
    { "getLocalSlotRef", getLocalSlotRef },
    { "getLocalSlot", getLocalSlot },
    { "setLocalSlot", setLocalSlot },
    { "deleteLocalSlot", deleteLocalSlot },
    { "getParentSlotRef", getParentSlotRef },
    { "getParentSlot", getParentSlot },
    { "setParentSlot", setParentSlot },
    { "deleteParentSlot", deleteParentSlot },
    { "getMemberRef", getMemberRef },
    { "getMember", getMember },
    { "setMember", setMember },
    { "callMember", callMember },
    { "deleteMember", deleteMember },
    { "getPropertyRef", getPropertyRef },
    { "getProperty", getProperty },
    { "setProperty", setProperty },
    { "callProperty", callProperty },
    { "deleteProperty", deleteProperty },
    { "pushEnvironment", pushEnvironment },
    { "popEnvironment", popEnvironment },
    { "exchange", exchange },
    { "typeOf", typeOf },
    { "equal", equal },
    { "notEqual", notEqual },
    { "identical", identical },
    { "notIdentical", notIdentical },
    { "less", less },
    { "lessOrEqual", lessOrEqual },
    { "more", more },
    { "moreOrEqual", moreOrEqual },
    { "instanceOf", instanceOf },
    { "in", in },
    { "add", add },
    { "minus", minus },
    { "multiply", multiply },
    { "divide", divide },
    { "modulo", modulo },
    { "leftShift", leftShift },
    { "rightShift", rightShift },
    { "unsignedRightShift", unsignedRightShift },
    { "bitwiseAnd", bitwiseAnd },
    { "bitwiseXor", bitwiseXor },
    { "bitwiseOr", bitwiseOr },
    { "logicalAnd", logicalAnd },
    { "logicalOr", logicalOr },
    { "positive", positive },
    { "negative", negative },
    { "invert", invert },
    { "not", not },
    { "construct", construct },
    { "call", call },
    { "eval", eval },
    { "incrementRef", incrementRef },
    { "decrementRef", decrementRef },
    { "postIncrementRef", postIncrementRef },
    { "postDecrementRef", postDecrementRef },
    { "addAssignRef", addAssignRef },
    { "minusAssignRef", minusAssignRef },
    { "multiplyAssignRef", multiplyAssignRef },
    { "divideAssignRef", divideAssignRef },
    { "moduloAssignRef", moduloAssignRef },
    { "leftShiftAssignRef", leftShiftAssignRef },
    { "rightShiftAssignRef", rightShiftAssignRef },
    { "unsignedRightShiftAssignRef", unsignedRightShiftAssignRef },
    { "bitAndAssignRef", bitAndAssignRef },
    { "bitXorAssignRef", bitXorAssignRef },
    { "bitOrAssignRef", bitOrAssignRef },
    { "debugger", debugger },
    { "try", try },
    { "throw", throw },
    { "with", with },
    { "next", next },
    { "nextIf", nextIf },
    { "autoreleaseExpression", autoreleaseExpression },
    { "autoreleaseDiscard", autoreleaseDiscard },
    { "expression", expression },
    { "discard", discard },
    { "discardN", discardN },
    { "jump", jump },
    { "jumpIf", jumpIf },
    { "jumpIfNot", jumpIfNot },
    { "repopulate", repopulate },
    { "result", result },
    { "resultVoid", resultVoid },
    { "switchOp", switchOp },
    { "breaker", breaker },
    { "iterate", iterate },
    { "iterateLessRef", iterateLessRef },
    { "iterateMoreRef", iterateMoreRef },
    { "iterateLessOrEqualRef", iterateLessOrEqualRef },
    { "iterateMoreOrEqualRef", iterateMoreOrEqualRef },
    { "iterateInRef", iterateInRef },
};

	int index;
	for (index = 0; index < sizeof(functionList); ++index)
		if (functionList[index].native == native)
			return functionList[index].name;
	
	assert(0);
	return "unknow";
}

// MARK: call

static
eccvalue_t nextOpValue (eccstate_t * const context)
{
	eccvalue_t value = nextOp();
	value.flags &= ~(io_libecc_value_readonly | io_libecc_value_hidden | io_libecc_value_sealed);
	return value;
}

#define nextOpValue() nextOpValue(context)

static
eccvalue_t replaceRefValue (eccvalue_t *ref, eccvalue_t value)
{
	ref->data = value.data;
	ref->type = value.type;
	return value;
}

static inline
eccvalue_t callOps (eccstate_t * const context, eccobject_t *environment)
{
	if (context->depth >= context->ecc->maximumCallDepth)
		ECCNSContext.rangeError(context, io_libecc_Chars.create("maximum depth exceeded"));
	
//	if (!context->parent->strictMode)
//		if (context->this.type == ECC_VALTYPE_UNDEFINED || context->this.type == ECC_VALTYPE_NULL)
//			context->this = ECCNSValue.object(&context->ecc->global->environment);
	
	context->environment = environment;
	return context->ops->native(context);
}

static inline
eccvalue_t callOpsRelease (eccstate_t * const context, eccobject_t *environment)
{
	eccvalue_t result;
	uint16_t index, count;
	
	result = callOps(context, environment);
	
	for (index = 2, count = environment->hashmapCount; index < count; ++index)
		release(environment->hashmap[index].value);
	
	return result;
}

static inline
void populateEnvironmentWithArguments (eccobject_t *environment, eccobject_t *arguments, int32_t parameterCount)
{
	int32_t index = 0;
	int argumentCount = arguments->elementCount;
	
	replaceRefValue(&environment->hashmap[2].value, retain(ECCNSValue.object(arguments)));
	
	if (argumentCount <= parameterCount)
		for (; index < argumentCount; ++index)
			environment->hashmap[index + 3].value = retain(arguments->element[index].value);
	else
	{
		for (; index < parameterCount; ++index)
			environment->hashmap[index + 3].value = retain(arguments->element[index].value);
	}
}

static inline
void populateEnvironmentAndArgumentsWithVA (eccobject_t *environment, int32_t parameterCount, int32_t argumentCount, va_list ap)
{
	int32_t index = 0;
	eccobject_t *arguments = ECCNSArguments.createSized(argumentCount);
	
	replaceRefValue(&environment->hashmap[2].value, retain(ECCNSValue.object(arguments)));
	
	if (argumentCount <= parameterCount)
		for (; index < argumentCount; ++index)
			environment->hashmap[index + 3].value = arguments->element[index].value = retain(va_arg(ap, eccvalue_t));
	else
	{
		for (; index < parameterCount; ++index)
			environment->hashmap[index + 3].value = arguments->element[index].value = retain(va_arg(ap, eccvalue_t));
		
		for (; index < argumentCount; ++index)
			arguments->element[index].value = retain(va_arg(ap, eccvalue_t));
	}
}

static inline
void populateEnvironmentWithVA (eccobject_t *environment, int32_t parameterCount, int32_t argumentCount, va_list ap)
{
	int32_t index = 0;
	if (argumentCount <= parameterCount)
		for (; index < argumentCount; ++index)
			environment->hashmap[index + 3].value = retain(va_arg(ap, eccvalue_t));
	else
		for (; index < parameterCount; ++index)
			environment->hashmap[index + 3].value = retain(va_arg(ap, eccvalue_t));
}

static inline
void populateStackEnvironmentAndArgumentsWithOps (eccstate_t * const context, eccobject_t *environment, eccobject_t *arguments, int32_t parameterCount, int32_t argumentCount)
{
	int32_t index = 0;
	
	replaceRefValue(&environment->hashmap[2].value, ECCNSValue.object(arguments));
	
	if (argumentCount <= parameterCount)
		for (; index < argumentCount; ++index)
			environment->hashmap[index + 3].value = arguments->element[index].value = retain(nextOpValue());
	else
	{
		for (; index < parameterCount; ++index)
			environment->hashmap[index + 3].value = arguments->element[index].value = retain(nextOpValue());
		
		for (; index < argumentCount; ++index)
			arguments->element[index].value = nextOpValue();
	}
}

static inline
void populateEnvironmentAndArgumentsWithOps (eccstate_t * const context, eccobject_t *environment, eccobject_t *arguments, int32_t parameterCount, int32_t argumentCount)
{
	populateStackEnvironmentAndArgumentsWithOps(context, environment, arguments, parameterCount, argumentCount);
	
	retain(ECCNSValue.object(arguments));
	
	if (argumentCount > parameterCount)
	{
		int32_t index = parameterCount;
		for (; index < argumentCount; ++index)
			retain(arguments->element[index].value);
	}
}

static inline
void populateEnvironmentWithOps (eccstate_t * const context, eccobject_t *environment, int32_t parameterCount, int32_t argumentCount)
{
	int32_t index = 0;
	if (argumentCount <= parameterCount)
		for (; index < argumentCount; ++index)
			environment->hashmap[index + 3].value = retain(nextOpValue());
	else
	{
		for (; index < parameterCount; ++index)
			environment->hashmap[index + 3].value = retain(nextOpValue());
		
		for (; index < argumentCount; ++index)
			nextOp();
	}
}

eccvalue_t callFunctionArguments (eccstate_t * const context, enum io_libecc_context_Offset offset, struct io_libecc_Function *function, eccvalue_t this, eccobject_t *arguments)
{
	eccstate_t subContext = {
		.ops = function->oplist->ops,
		.this = this,
		.parent = context,
		.ecc = context->ecc,
		.argumentOffset = offset,
		.depth = context->depth + 1,
		.strictMode = function->flags & io_libecc_function_strictMode,
		.refObject = function->refObject,
	};
	
	if (function->flags & io_libecc_function_needHeap)
	{
		eccobject_t *environment = ECCNSObject.copy(&function->environment);
		
		if (function->flags & io_libecc_function_needArguments)
		{
			eccobject_t *copy = ECCNSArguments.createSized(arguments->elementCount);
			memcpy(copy->element, arguments->element, sizeof(*copy->element) * copy->elementCount);
			arguments = copy;
		}
		populateEnvironmentWithArguments(environment, arguments, function->parameterCount);
		
		return callOps(&subContext, environment);
	}
	else
	{
		eccobject_t environment = function->environment;
		union io_libecc_object_Hashmap hashmap[function->environment.hashmapCapacity];
		
		memcpy(hashmap, function->environment.hashmap, sizeof(hashmap));
		environment.hashmap = hashmap;
		populateEnvironmentWithArguments(&environment, arguments, function->parameterCount);
		
		return callOpsRelease(&subContext, &environment);
	}
}

eccvalue_t callFunctionVA (eccstate_t * const context, enum io_libecc_context_Offset offset, struct io_libecc_Function *function, eccvalue_t this, int argumentCount, va_list ap)
{
	eccstate_t subContext = {
		.ops = function->oplist->ops,
		.this = this,
		.parent = context,
		.ecc = context->ecc,
		.argumentOffset = offset,
		.depth = context->depth + 1,
		.strictMode = function->flags & io_libecc_function_strictMode,
		.refObject = function->refObject,
	};
	
	if (function->flags & io_libecc_function_needHeap)
	{
		eccobject_t *environment = ECCNSObject.copy(&function->environment);
		
		if (function->flags & io_libecc_function_needArguments)
		{
			populateEnvironmentAndArgumentsWithVA(environment, function->parameterCount, argumentCount, ap);
			
			if (!context->strictMode)
			{
				ECCNSObject.addMember(environment->hashmap[2].value.data.object, io_libecc_key_callee, retain(ECCNSValue.function(function)), io_libecc_value_hidden);
				ECCNSObject.addMember(environment->hashmap[2].value.data.object, io_libecc_key_length, ECCNSValue.integer(argumentCount), io_libecc_value_hidden);
			}
		}
		else
			populateEnvironmentWithVA(environment, function->parameterCount, argumentCount, ap);
		
		return callOps(&subContext, environment);
	}
	else
	{
		eccobject_t environment = function->environment;
		union io_libecc_object_Hashmap hashmap[function->environment.hashmapCapacity];
		
		memcpy(hashmap, function->environment.hashmap, sizeof(hashmap));
		environment.hashmap = hashmap;
		
		populateEnvironmentWithVA(&environment, function->parameterCount, argumentCount, ap);
		
		return callOpsRelease(&subContext, &environment);
	}
}

static inline
eccvalue_t callFunction (eccstate_t * const context, struct io_libecc_Function * const function, eccvalue_t this, int32_t argumentCount, int construct)
{
	eccstate_t subContext = {
		.ops = function->oplist->ops,
		.this = this,
		.parent = context,
		.ecc = context->ecc,
		.construct = construct,
		.depth = context->depth + 1,
		.strictMode = function->flags & io_libecc_function_strictMode,
		.refObject = function->refObject,
	};
	
	if (function->flags & io_libecc_function_needHeap)
	{
		eccobject_t *environment = ECCNSObject.copy(&function->environment);
		
		if (function->flags & io_libecc_function_needArguments)
		{
			populateEnvironmentAndArgumentsWithOps(context, environment, ECCNSArguments.createSized(argumentCount), function->parameterCount, argumentCount);
			
			if (!context->strictMode)
			{
				ECCNSObject.addMember(environment->hashmap[2].value.data.object, io_libecc_key_callee, retain(ECCNSValue.function(function)), io_libecc_value_hidden);
				ECCNSObject.addMember(environment->hashmap[2].value.data.object, io_libecc_key_length, ECCNSValue.integer(argumentCount), io_libecc_value_hidden);
			}
		}
		else
			populateEnvironmentWithOps(context, environment, function->parameterCount, argumentCount);
		
		return callOps(&subContext, environment);
	}
	else if (function->flags & io_libecc_function_needArguments)
	{
		eccobject_t environment = function->environment;
		eccobject_t arguments = ECCNSObject.identity;
		union io_libecc_object_Hashmap hashmap[function->environment.hashmapCapacity];
		union io_libecc_object_Element element[argumentCount];
		
		memcpy(hashmap, function->environment.hashmap, sizeof(hashmap));
		environment.hashmap = hashmap;
		arguments.element = element;
		arguments.elementCount = argumentCount;
		populateStackEnvironmentAndArgumentsWithOps(context, &environment, &arguments, function->parameterCount, argumentCount);
		
		return callOpsRelease(&subContext, &environment);
	}
	else
	{
		eccobject_t environment = function->environment;
		union io_libecc_object_Hashmap hashmap[function->environment.hashmapCapacity];
		
		memcpy(hashmap, function->environment.hashmap, sizeof(hashmap));
		environment.hashmap = hashmap;
		populateEnvironmentWithOps(context, &environment, function->parameterCount, argumentCount);
		
		return callOpsRelease(&subContext, &environment);
	}
}

static inline
eccvalue_t callValue (eccstate_t * const context, eccvalue_t value, eccvalue_t this, int32_t argumentCount, int construct, const ecctextstring_t *textCall)
{
	eccvalue_t result;
	const ecctextstring_t *parentTextCall = context->textCall;
	
	if (value.type != ECC_VALTYPE_FUNCTION)
		ECCNSContext.typeError(context, io_libecc_Chars.create("'%.*s' is not a function", context->text->length, context->text->bytes));
	
	context->textCall = textCall;
	
	if (value.data.function->flags & io_libecc_function_useBoundThis)
		result = callFunction(context, value.data.function, value.data.function->boundThis, argumentCount, construct);
	else
		result = callFunction(context, value.data.function, this, argumentCount, construct);
	
	context->textCall = parentTextCall;
	return result;
}

eccvalue_t construct (eccstate_t * const context)
{
	const ecctextstring_t *textCall = opText(0);
	const ecctextstring_t *text = opText(1);
	int32_t argumentCount = opValue().data.integer;
	eccvalue_t value, *prototype, object, function = nextOp();
	
	if (function.type != ECC_VALTYPE_FUNCTION)
		goto error;
	
	prototype = ECCNSObject.member(&function.data.function->object, io_libecc_key_prototype, 0);
	if (!prototype)
		goto error;
	
	if (!ECCNSValue.isObject(*prototype))
		object = ECCNSValue.object(ECCNSObject.create(io_libecc_object_prototype));
	else if (prototype->type == ECC_VALTYPE_FUNCTION)
	{
		object = ECCNSValue.object(ECCNSObject.create(NULL));
		object.data.object->prototype = &prototype->data.function->object;
	}
	else if (prototype->type == ECC_VALTYPE_OBJECT)
		object = ECCNSValue.object(ECCNSObject.create(prototype->data.object));
	else
		object = ECCValConstUndefined;
	
	ECCNSContext.setText(context, text);
	value = callValue(context, function, object, argumentCount, 1, textCall);
	
	if (ECCNSValue.isObject(value))
		return value;
	else
		return object;
	
error:
	context->textCall = textCall;
	ECCNSContext.setTextIndex(context, io_libecc_context_funcIndex);
	ECCNSContext.typeError(context, io_libecc_Chars.create("'%.*s' is not a constructor", text->length, text->bytes));
}

eccvalue_t call (eccstate_t * const context)
{
	const ecctextstring_t *textCall = opText(0);
	const ecctextstring_t *text = opText(1);
	int32_t argumentCount = opValue().data.integer;
	eccvalue_t value;
	eccvalue_t this;
	
	context->inEnvironmentObject = 0;
	value = nextOp();
	
	if (context->inEnvironmentObject)
	{
		eccstate_t *seek = context;
		while (seek->parent && seek->parent->refObject == context->refObject)
			seek = seek->parent;
		
		++seek->environment->referenceCount;
		this = ECCNSValue.objectValue(seek->environment);
	}
	else
		this = ECCValConstUndefined;
	
	ECCNSContext.setText(context, text);
	return callValue(context, value, this, argumentCount, 0, textCall);
}

eccvalue_t eval (eccstate_t * const context)
{
	eccvalue_t value;
	eccioinput_t *input;
	int32_t argumentCount = opValue().data.integer;
	eccstate_t subContext = {
		.parent = context,
		.this = context->this,
		.environment = context->environment,
		.ecc = context->ecc,
		.depth = context->depth + 1,
		.strictMode = context->strictMode,
	};
	
	if (!argumentCount)
		return ECCValConstUndefined;
	
	value = nextOp();
	while (--argumentCount)
		nextOp();
	
	if (!ECCNSValue.isString(value) || !ECCNSValue.isPrimitive(value))
		return value;
	
	input = io_libecc_Input.createFromBytes(ECCNSValue.stringBytes(&value), ECCNSValue.stringLength(&value), "(eval)");
	ECCNSScript.evalInputWithContext(context->ecc, input, &subContext);
	
	value = context->ecc->result;
	context->ecc->result = ECCValConstUndefined;
	return value;
}


// Expression

eccvalue_t noop (eccstate_t * const context)
{
	return ECCValConstUndefined;
}

eccvalue_t value (eccstate_t * const context)
{
	return opValue();
}

eccvalue_t valueConstRef (eccstate_t * const context)
{
	return ECCNSValue.reference((eccvalue_t *)&opValue());
}

eccvalue_t text (eccstate_t * const context)
{
	return ECCNSValue.text(opText(0));
}

eccvalue_t regexp (eccstate_t * const context)
{
	const ecctextstring_t *text = opText(0);
	struct io_libecc_Error *error = NULL;
	struct io_libecc_Chars *chars = io_libecc_Chars.createWithBytes(text->length, text->bytes);
	struct io_libecc_RegExp *regexp = io_libecc_RegExp.create(chars, &error, context->ecc->sloppyMode? io_libecc_regexp_allowUnicodeFlags: 0);
	if (error)
	{
		error->text.bytes = text->bytes + (error->text.bytes - chars->bytes);
		ECCNSContext.throw(context, ECCNSValue.error(error));
	}
	return ECCNSValue.regexp(regexp);
}

eccvalue_t function (eccstate_t * const context)
{
	eccobject_t *prototype;
	eccvalue_t value = opValue(), result;
	
	struct io_libecc_Function *function = io_libecc_Function.copy(value.data.function);
	function->object.prototype = &value.data.function->object;
	function->environment.prototype = context->environment;
	if (context->refObject)
	{
		++context->refObject->referenceCount;
		function->refObject = context->refObject;
	}
	
	prototype = ECCNSObject.create(io_libecc_object_prototype);
	io_libecc_Function.linkPrototype(function, ECCNSValue.object(prototype), io_libecc_value_sealed);
	
	++prototype->referenceCount;
	++context->environment->referenceCount;
	++function->object.referenceCount;
	
	result = ECCNSValue.function(function);
	result.flags = value.flags;
	return result;
}

eccvalue_t object (eccstate_t * const context)
{
	eccobject_t *object = ECCNSObject.create(io_libecc_object_prototype);
	eccvalue_t property, value;
	uint32_t count;
	
	object->flags |= io_libecc_object_mark;
	
	for (count = opValue().data.integer; count--;)
	{
		property = nextOp();
		value = retain(nextOpValue());
		
		if (property.type == ECC_VALTYPE_KEY)
			ECCNSObject.addMember(object, property.data.key, value, 0);
		else if (property.type == ECC_VALTYPE_INTEGER)
			ECCNSObject.addElement(object, property.data.integer, value, 0);
	}
	return ECCNSValue.object(object);
}

eccvalue_t array (eccstate_t * const context)
{
	uint32_t length = opValue().data.integer;
	eccobject_t *object = io_libecc_Array.createSized(length);
	eccvalue_t value;
	uint32_t index;
	
	object->flags |= io_libecc_object_mark;
	
	for (index = 0; index < length; ++index)
	{
		value = retain(nextOpValue());
		object->element[index].value = value;
	}
	return ECCNSValue.object(object);
}

eccvalue_t this (eccstate_t * const context)
{
	return context->this;
}

static
eccvalue_t * localRef (eccstate_t * const context, struct io_libecc_Key key, const ecctextstring_t *text, int required)
{
	eccvalue_t *ref;
	
	ref = ECCNSObject.member(context->environment, key, 0);
	
	if (!context->strictMode)
	{
		context->inEnvironmentObject = context->refObject != NULL;
		if (!ref && context->refObject)
		{
			context->inEnvironmentObject = 0;
			ref = ECCNSObject.member(context->refObject, key, 0);
		}
	}
	
	if (!ref && required)
	{
		ECCNSContext.setText(context, text);
		ECCNSContext.referenceError(context, io_libecc_Chars.create("'%.*s' is not defined", io_libecc_Key.textOf(key)->length, io_libecc_Key.textOf(key)->bytes));
	}
	return ref;
}

eccvalue_t createLocalRef (eccstate_t * const context)
{
	struct io_libecc_Key key = opValue().data.key;
	eccvalue_t *ref = localRef(context, key, opText(0), context->strictMode);
	
	if (!ref)
		ref = ECCNSObject.addMember(&context->ecc->global->environment, key, ECCValConstUndefined, 0);
	
	return ECCNSValue.reference(ref);
}

eccvalue_t getLocalRefOrNull (eccstate_t * const context)
{
	return ECCNSValue.reference(localRef(context, opValue().data.key, opText(0), 0));
}

eccvalue_t getLocalRef (eccstate_t * const context)
{
	return ECCNSValue.reference(localRef(context, opValue().data.key, opText(0), 1));
}

eccvalue_t getLocal (eccstate_t * const context)
{
	return *localRef(context, opValue().data.key, opText(0), 1);
}

eccvalue_t setLocal (eccstate_t * const context)
{
	const ecctextstring_t *text = opText(0);
	struct io_libecc_Key key = opValue().data.key;
	eccvalue_t value = nextOp();
	
	eccvalue_t *ref = localRef(context, key, text, context->strictMode);
	
	if (!ref)
		ref = ECCNSObject.addMember(&context->ecc->global->environment, key, ECCValConstUndefined, 0);
	
	if (ref->flags & io_libecc_value_readonly)
		return value;
	
	retain(value);
	release(*ref);
	replaceRefValue(ref, value);
	return value;
}

eccvalue_t deleteLocal (eccstate_t * const context)
{
	eccvalue_t *ref = localRef(context, opValue().data.key, opText(0), 0);
	
	if (!ref)
		return ECCValConstTrue;
	
	if (ref->flags & io_libecc_value_sealed)
		return ECCValConstFalse;
	
	*ref = ECCValConstNone;
	return ECCValConstTrue;
}

eccvalue_t getLocalSlotRef (eccstate_t * const context)
{
	return ECCNSValue.reference(&context->environment->hashmap[opValue().data.integer].value);
}

eccvalue_t getLocalSlot (eccstate_t * const context)
{
	return context->environment->hashmap[opValue().data.integer].value;
}

eccvalue_t setLocalSlot (eccstate_t * const context)
{
	int32_t slot = opValue().data.integer;
	eccvalue_t value = nextOp();
	eccvalue_t *ref = &context->environment->hashmap[slot].value;
	
	if (ref->flags & io_libecc_value_readonly)
		return value;
	
	retain(value);
	release(*ref);
	replaceRefValue(ref, value);
	return value;
}

eccvalue_t deleteLocalSlot (eccstate_t * const context)
{
	return ECCValConstFalse;
}

eccvalue_t getParentSlotRef (eccstate_t * const context)
{
	int32_t slot = opValue().data.integer & 0xffff;
	int32_t count = opValue().data.integer >> 16;
	eccobject_t *object = context->environment;
	while (count--)
		object = object->prototype;
	
	return ECCNSValue.reference(&object->hashmap[slot].value);
}

eccvalue_t getParentSlot (eccstate_t * const context)
{
	return *getParentSlotRef(context).data.reference;
}

eccvalue_t setParentSlot (eccstate_t * const context)
{
	const ecctextstring_t *text = opText(0);
	eccvalue_t *ref = getParentSlotRef(context).data.reference;
	eccvalue_t value = nextOp();
	if (ref->flags & io_libecc_value_readonly)
	{
		if (context->strictMode)
		{
			ecctextstring_t property = *io_libecc_Key.textOf(ref->key);
			ECCNSContext.setText(context, text);
			ECCNSContext.typeError(context, io_libecc_Chars.create("'%.*s' is read-only", property.length, property.bytes));
		}
	}
	else
	{
		retain(value);
		release(*ref);
		replaceRefValue(ref, value);
	}
	return value;
}

eccvalue_t deleteParentSlot (eccstate_t * const context)
{
	return ECCValConstFalse;
}

static
void prepareObject (eccstate_t * const context, eccvalue_t *object)
{
	const ecctextstring_t *textObject = opText(1);
	*object = nextOp();
	
	if (ECCNSValue.isPrimitive(*object))
	{
		ECCNSContext.setText(context, textObject);
		*object = ECCNSValue.toObject(context, *object);
	}
}

eccvalue_t getMemberRef (eccstate_t * const context)
{
	const ecctextstring_t *text = opText(0);
	struct io_libecc_Key key = opValue().data.key;
	eccvalue_t object, *ref;
	
	prepareObject(context, &object);
	
	context->refObject = object.data.object;
	ref = ECCNSObject.member(object.data.object, key, io_libecc_value_asOwn);
	
	if (!ref)
	{
		if (object.data.object->flags & io_libecc_object_sealed)
		{
			ECCNSContext.setText(context, text);
			ECCNSContext.typeError(context, io_libecc_Chars.create("object is not extensible"));
		}
		ref = ECCNSObject.addMember(object.data.object, key, ECCValConstUndefined, 0);
	}
	
	return ECCNSValue.reference(ref);
}

eccvalue_t getMember (eccstate_t * const context)
{
	struct io_libecc_Key key = opValue().data.key;
	eccvalue_t object;
	
	prepareObject(context, &object);
	
	return ECCNSObject.getMember(context, object.data.object, key);
}

eccvalue_t setMember (eccstate_t * const context)
{
	const ecctextstring_t *text = opText(0);
	struct io_libecc_Key key = opValue().data.key;
	eccvalue_t object, value;
	
	prepareObject(context, &object);
	value = retain(nextOp());
	
	ECCNSContext.setText(context, text);
	ECCNSObject.putMember(context, object.data.object, key, value);
	
	return value;
}

eccvalue_t callMember (eccstate_t * const context)
{
	const ecctextstring_t *textCall = opText(0);
	int32_t argumentCount = opValue().data.integer;
	const ecctextstring_t *text = &(++context->ops)->text;
	struct io_libecc_Key key = opValue().data.key;
	eccvalue_t object;
	
	prepareObject(context, &object);
	
	ECCNSContext.setText(context, text);
	return callValue(context, ECCNSObject.getMember(context, object.data.object, key), object, argumentCount, 0, textCall);
}

eccvalue_t deleteMember (eccstate_t * const context)
{
	const ecctextstring_t *text = opText(0);
	struct io_libecc_Key key = opValue().data.key;
	eccvalue_t object;
	int result;
	
	prepareObject(context, &object);
	
	result = ECCNSObject.deleteMember(object.data.object, key);
	if (!result && context->strictMode)
	{
		ECCNSContext.setText(context, text);
		ECCNSContext.typeError(context, io_libecc_Chars.create("'%.*s' is non-configurable", io_libecc_Key.textOf(key)->length, io_libecc_Key.textOf(key)->bytes));
	}
	
	return ECCNSValue.truth(result);
}

static
void prepareObjectProperty (eccstate_t * const context, eccvalue_t *object, eccvalue_t *property)
{
	const ecctextstring_t *textProperty;
	
	prepareObject(context, object);
	
	textProperty = opText(1);
	*property = nextOp();
	
	if (ECCNSValue.isObject(*property))
	{
		ECCNSContext.setText(context, textProperty);
		*property = ECCNSValue.toPrimitive(context, *property, io_libecc_value_hintString);
	}
}

eccvalue_t getPropertyRef (eccstate_t * const context)
{
	const ecctextstring_t *text = opText(1);
	eccvalue_t object, property;
	eccvalue_t *ref;
	
	prepareObjectProperty(context, &object, &property);
	
	context->refObject = object.data.object;
	ref = ECCNSObject.property(object.data.object, property, io_libecc_value_asOwn);
	
	if (!ref)
	{
		if (object.data.object->flags & io_libecc_object_sealed)
		{
			ECCNSContext.setText(context, text);
			ECCNSContext.typeError(context, io_libecc_Chars.create("object is not extensible"));
		}
		ref = ECCNSObject.addProperty(object.data.object, property, ECCValConstUndefined, 0);
	}
	
	return ECCNSValue.reference(ref);
}

eccvalue_t getProperty (eccstate_t * const context)
{
	eccvalue_t object, property;
	
	prepareObjectProperty(context, &object, &property);
	
	return ECCNSObject.getProperty(context, object.data.object, property);
}

eccvalue_t setProperty (eccstate_t * const context)
{
	const ecctextstring_t *text = opText(0);
	eccvalue_t object, property, value;
	
	prepareObjectProperty(context, &object, &property);
	
	value = retain(nextOp());
	value.flags = 0;
	
	ECCNSContext.setText(context, text);
	ECCNSObject.putProperty(context, object.data.object, property, value);
	
	return value;
}

eccvalue_t callProperty (eccstate_t * const context)
{
	const ecctextstring_t *textCall = opText(0);
	int32_t argumentCount = opValue().data.integer;
	const ecctextstring_t *text = &(++context->ops)->text;
	eccvalue_t object, property;
	
	prepareObjectProperty(context, &object, &property);
	
	ECCNSContext.setText(context, text);
	return callValue(context, ECCNSObject.getProperty(context, object.data.object, property), object, argumentCount, 0, textCall);
}

eccvalue_t deleteProperty (eccstate_t * const context)
{
	const ecctextstring_t *text = opText(0);
	eccvalue_t object, property;
	int result;
	
	prepareObjectProperty(context, &object, &property);
	
	result = ECCNSObject.deleteProperty(object.data.object, property);
	if (!result && context->strictMode)
	{
		eccvalue_t string = ECCNSValue.toString(context, property);
		ECCNSContext.setText(context, text);
		ECCNSContext.typeError(context, io_libecc_Chars.create("'%.*s' is non-configurable", ECCNSValue.stringLength(&string), ECCNSValue.stringBytes(&string)));
	}
	return ECCNSValue.truth(result);
}

eccvalue_t pushEnvironment (eccstate_t * const context)
{
	if (context->refObject)
		context->refObject = ECCNSObject.create(context->refObject);
	else
		context->environment = ECCNSObject.create(context->environment);
	
	return opValue();
}

eccvalue_t popEnvironment (eccstate_t * const context)
{
	if (context->refObject)
	{
		eccobject_t *refObject = context->refObject;
		context->refObject = context->refObject->prototype;
		refObject->prototype = NULL;
	}
	else
	{
		eccobject_t *environment = context->environment;
		context->environment = context->environment->prototype;
		environment->prototype = NULL;
	}
	return opValue();
}

eccvalue_t exchange (eccstate_t * const context)
{
	eccvalue_t value = opValue();
	nextOp();
	return value;
}

eccvalue_t typeOf (eccstate_t * const context)
{
	eccvalue_t value = nextOp();
	if (value.type == ECC_VALTYPE_REFERENCE)
	{
		eccvalue_t *ref = value.data.reference;
		if (!ref)
			return ECCNSValue.text(&ECC_ConstString_Undefined);
		else
			value = *ref;
	}
	return ECCNSValue.toType(value);
}

#define prepareAB \
	const ecctextstring_t *text = opText(1);\
	eccvalue_t a = nextOp();\
	const ecctextstring_t *textAlt = opText(1);\
	eccvalue_t b = nextOp();\

eccvalue_t equal (eccstate_t * const context)
{
	prepareAB
	
	if (a.type == ECC_VALTYPE_BINARY && b.type == ECC_VALTYPE_BINARY)
		return ECCNSValue.truth(a.data.binary == b.data.binary);
	else
	{
		ECCNSContext.setTexts(context, text, textAlt);
		return ECCNSValue.equals(context, a, b);
	}
}

eccvalue_t notEqual (eccstate_t * const context)
{
	prepareAB
	
	if (a.type == ECC_VALTYPE_BINARY && b.type == ECC_VALTYPE_BINARY)
		return ECCNSValue.truth(a.data.binary != b.data.binary);
	else
	{
		ECCNSContext.setTexts(context, text, textAlt);
		return ECCNSValue.truth(!ECCNSValue.isTrue(ECCNSValue.equals(context, a, b)));
	}
}

eccvalue_t identical (eccstate_t * const context)
{
	prepareAB
	
	if (a.type == ECC_VALTYPE_BINARY && b.type == ECC_VALTYPE_BINARY)
		return ECCNSValue.truth(a.data.binary == b.data.binary);
	else
	{
		ECCNSContext.setTexts(context, text, textAlt);
		return ECCNSValue.same(context, a, b);
	}
}

eccvalue_t notIdentical (eccstate_t * const context)
{
	prepareAB
	
	if (a.type == ECC_VALTYPE_BINARY && b.type == ECC_VALTYPE_BINARY)
		return ECCNSValue.truth(a.data.binary != b.data.binary);
	else
	{
		ECCNSContext.setTexts(context, text, textAlt);
		return ECCNSValue.truth(!ECCNSValue.isTrue(ECCNSValue.same(context, a, b)));
	}
}

eccvalue_t less (eccstate_t * const context)
{
	prepareAB
	
	if (a.type == ECC_VALTYPE_BINARY && b.type == ECC_VALTYPE_BINARY)
		return ECCNSValue.truth(a.data.binary < b.data.binary);
	else
	{
		ECCNSContext.setTexts(context, text, textAlt);
		return ECCNSValue.less(context, a, b);
	}
}

eccvalue_t lessOrEqual (eccstate_t * const context)
{
	prepareAB
	
	if (a.type == ECC_VALTYPE_BINARY && b.type == ECC_VALTYPE_BINARY)
		return ECCNSValue.truth(a.data.binary <= b.data.binary);
	else
	{
		ECCNSContext.setTexts(context, text, textAlt);
		return ECCNSValue.lessOrEqual(context, a, b);
	}
}

eccvalue_t more (eccstate_t * const context)
{
	prepareAB
	
	if (a.type == ECC_VALTYPE_BINARY && b.type == ECC_VALTYPE_BINARY)
		return ECCNSValue.truth(a.data.binary > b.data.binary);
	else
	{
		ECCNSContext.setTexts(context, text, textAlt);
		return ECCNSValue.more(context, a, b);
	}
}

eccvalue_t moreOrEqual (eccstate_t * const context)
{
	prepareAB
	
	if (a.type == ECC_VALTYPE_BINARY && b.type == ECC_VALTYPE_BINARY)
		return ECCNSValue.truth(a.data.binary >= b.data.binary);
	else
	{
		ECCNSContext.setTexts(context, text, textAlt);
		return ECCNSValue.moreOrEqual(context, a, b);
	}
}

eccvalue_t instanceOf (eccstate_t * const context)
{
	eccvalue_t a = nextOp();
	const ecctextstring_t *textAlt = opText(1);
	eccvalue_t b = nextOp();
	
	if (b.type != ECC_VALTYPE_FUNCTION)
	{
		ECCNSContext.setText(context, textAlt);
		ECCNSContext.typeError(context, io_libecc_Chars.create("'%.*s' is not a function", textAlt->length, textAlt->bytes));
	}
	
	b = ECCNSObject.getMember(context, b.data.object, io_libecc_key_prototype);
	if (!ECCNSValue.isObject(b))
	{
		ECCNSContext.setText(context, textAlt);
		ECCNSContext.typeError(context, io_libecc_Chars.create("'%.*s'.prototype not an object", textAlt->length, textAlt->bytes));
	}
	
	if (ECCNSValue.isObject(a))
	{
		eccobject_t *object;
		
		object = a.data.object;
		while (( object = object->prototype ))
			if (object == b.data.object)
				return ECCValConstTrue;
	}
	return ECCValConstFalse;
}

eccvalue_t in (eccstate_t * const context)
{
	eccvalue_t property = nextOp();
	eccvalue_t object = nextOp();
	eccvalue_t *ref;
	
	if (!ECCNSValue.isObject(object))
		ECCNSContext.typeError(context, io_libecc_Chars.create("'%.*s' not an object", context->ops->text.length, context->ops->text.bytes));
	
	ref = ECCNSObject.property(object.data.object, ECCNSValue.toString(context, property), 0);
	
	return ECCNSValue.truth(ref != NULL);
}

eccvalue_t add (eccstate_t * const context)
{
	prepareAB
	
	if (a.type == ECC_VALTYPE_BINARY && b.type == ECC_VALTYPE_BINARY)
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

eccvalue_t minus (eccstate_t * const context)
{
	eccvalue_t a = nextOp();
	eccvalue_t b = nextOp();
	if (a.type == ECC_VALTYPE_BINARY && b.type == ECC_VALTYPE_BINARY)
	{
		a.data.binary -= b.data.binary;
		return a;
	}
	else
		return ECCNSValue.binary(ECCNSValue.toBinary(context, a).data.binary - ECCNSValue.toBinary(context, b).data.binary);
}

eccvalue_t multiply (eccstate_t * const context)
{
	eccvalue_t a = nextOp();
	eccvalue_t b = nextOp();
	if (a.type == ECC_VALTYPE_BINARY && b.type == ECC_VALTYPE_BINARY)
	{
		a.data.binary *= b.data.binary;
		return a;
	}
	else
		return ECCNSValue.binary(ECCNSValue.toBinary(context, a).data.binary * ECCNSValue.toBinary(context, b).data.binary);
}

eccvalue_t divide (eccstate_t * const context)
{
	eccvalue_t a = nextOp();
	eccvalue_t b = nextOp();
	if (a.type == ECC_VALTYPE_BINARY && b.type == ECC_VALTYPE_BINARY)
	{
		a.data.binary /= b.data.binary;
		return a;
	}
	else
		return ECCNSValue.binary(ECCNSValue.toBinary(context, a).data.binary / ECCNSValue.toBinary(context, b).data.binary);
}

eccvalue_t modulo (eccstate_t * const context)
{
	eccvalue_t a = nextOp();
	eccvalue_t b = nextOp();
	if (a.type == ECC_VALTYPE_BINARY && b.type == ECC_VALTYPE_BINARY)
	{
		a.data.binary = fmod(a.data.binary, b.data.binary);
		return a;
	}
	else
		return ECCNSValue.binary(fmod(ECCNSValue.toBinary(context, a).data.binary, ECCNSValue.toBinary(context, b).data.binary));
}

eccvalue_t leftShift (eccstate_t * const context)
{
	eccvalue_t a = nextOp();
	eccvalue_t b = nextOp();
	return ECCNSValue.binary(ECCNSValue.toInteger(context, a).data.integer << (uint32_t)ECCNSValue.toInteger(context, b).data.integer);
}

eccvalue_t rightShift (eccstate_t * const context)
{
	eccvalue_t a = nextOp();
	eccvalue_t b = nextOp();
	return ECCNSValue.binary(ECCNSValue.toInteger(context, a).data.integer >> (uint32_t)ECCNSValue.toInteger(context, b).data.integer);
}

eccvalue_t unsignedRightShift (eccstate_t * const context)
{
	eccvalue_t a = nextOp();
	eccvalue_t b = nextOp();
	return ECCNSValue.binary((uint32_t)ECCNSValue.toInteger(context, a).data.integer >> (uint32_t)ECCNSValue.toInteger(context, b).data.integer);
}

eccvalue_t bitwiseAnd (eccstate_t * const context)
{
	eccvalue_t a = nextOp();
	eccvalue_t b = nextOp();
	return ECCNSValue.binary(ECCNSValue.toInteger(context, a).data.integer & ECCNSValue.toInteger(context, b).data.integer);
}

eccvalue_t bitwiseXor (eccstate_t * const context)
{
	eccvalue_t a = nextOp();
	eccvalue_t b = nextOp();
	return ECCNSValue.binary(ECCNSValue.toInteger(context, a).data.integer ^ ECCNSValue.toInteger(context, b).data.integer);
}

eccvalue_t bitwiseOr (eccstate_t * const context)
{
	eccvalue_t a = nextOp();
	eccvalue_t b = nextOp();
	return ECCNSValue.binary(ECCNSValue.toInteger(context, a).data.integer | ECCNSValue.toInteger(context, b).data.integer);
}

eccvalue_t logicalAnd (eccstate_t * const context)
{
	const int32_t opCount = opValue().data.integer;
	eccvalue_t value = nextOp();
	
	if (!ECCNSValue.isTrue(value))
	{
		context->ops += opCount;
		return value;
	}
	else
		return nextOp();
}

eccvalue_t logicalOr (eccstate_t * const context)
{
	const int32_t opCount = opValue().data.integer;
	eccvalue_t value = nextOp();
	
	if (ECCNSValue.isTrue(value))
	{
		context->ops += opCount;
		return value;
	}
	else
		return nextOp();
}

eccvalue_t positive (eccstate_t * const context)
{
	eccvalue_t a = nextOp();
	if (a.type == ECC_VALTYPE_BINARY)
		return a;
	else
		return ECCNSValue.toBinary(context, a);
}

eccvalue_t negative (eccstate_t * const context)
{
	eccvalue_t a = nextOp();
	if (a.type == ECC_VALTYPE_BINARY)
		return ECCNSValue.binary(-a.data.binary);
	else
		return ECCNSValue.binary(-ECCNSValue.toBinary(context, a).data.binary);
}

eccvalue_t invert (eccstate_t * const context)
{
	eccvalue_t a = nextOp();
	return ECCNSValue.binary(~ECCNSValue.toInteger(context, a).data.integer);
}

eccvalue_t not (eccstate_t * const context)
{
	eccvalue_t a = nextOp();
	return ECCNSValue.truth(!ECCNSValue.isTrue(a));
}

// MARK: assignement

#define unaryBinaryOpRef(OP) \
	eccobject_t *refObject = context->refObject; \
	const ecctextstring_t *text = opText(0); \
	eccvalue_t *ref = nextOp().data.reference; \
	eccvalue_t a; \
	double result; \
	 \
	a = *ref; \
	if (a.flags & (io_libecc_value_readonly | io_libecc_value_accessor)) \
	{ \
		ECCNSContext.setText(context, text); \
		a = ECCNSValue.toBinary(context, release(ECCNSObject.getValue(context, context->refObject, ref))); \
		result = OP; \
		ECCNSObject.putValue(context, context->refObject, ref, a); \
		return ECCNSValue.binary(result); \
	} \
	else if (a.type != ECC_VALTYPE_BINARY) \
		a = ECCNSValue.toBinary(context, release(a)); \
	 \
	result = OP; \
	replaceRefValue(ref, a); \
	context->refObject = refObject; \
	return ECCNSValue.binary(result); \

eccvalue_t incrementRef (eccstate_t * const context)
{
	unaryBinaryOpRef(++a.data.binary)
}

eccvalue_t decrementRef (eccstate_t * const context)
{
	unaryBinaryOpRef(--a.data.binary)
}

eccvalue_t postIncrementRef (eccstate_t * const context)
{
	unaryBinaryOpRef(a.data.binary++)
}

eccvalue_t postDecrementRef (eccstate_t * const context)
{
	unaryBinaryOpRef(a.data.binary--)
}

#define assignOpRef(OP, TYPE, CONV) \
	eccobject_t *refObject = context->refObject; \
	const ecctextstring_t *text = opText(0); \
	eccvalue_t *ref = nextOp().data.reference; \
	eccvalue_t a, b = nextOp(); \
	 \
	if (b.type != TYPE) \
		b = CONV(context, b); \
	 \
	a = *ref; \
	if (a.flags & (io_libecc_value_readonly | io_libecc_value_accessor)) \
	{ \
		ECCNSContext.setText(context, text); \
		a = CONV(context, ECCNSObject.getValue(context, context->refObject, ref)); \
		OP; \
		return ECCNSObject.putValue(context, context->refObject, ref, a); \
	} \
	else if (a.type != TYPE) \
		a = CONV(context, release(a)); \
	\
	OP; \
	replaceRefValue(ref, a); \
	context->refObject = refObject; \
	return a; \

#define assignBinaryOpRef(OP) assignOpRef(OP, ECC_VALTYPE_BINARY, ECCNSValue.toBinary)
#define assignIntegerOpRef(OP) assignOpRef(OP, ECC_VALTYPE_INTEGER, ECCNSValue.toInteger)

eccvalue_t addAssignRef (eccstate_t * const context)
{
	eccobject_t *refObject = context->refObject;
	const ecctextstring_t *text = opText(1);
	eccvalue_t *ref = nextOp().data.reference;
	const ecctextstring_t *textAlt = opText(1);
	eccvalue_t a, b = nextOp();
	
	ECCNSContext.setTexts(context, text, textAlt);

	a = *ref;
	if (a.flags & (io_libecc_value_readonly | io_libecc_value_accessor))
	{
		a = ECCNSObject.getValue(context, context->refObject, ref);
		a = retain(ECCNSValue.add(context, a, b));
		return ECCNSObject.putValue(context, context->refObject, ref, a);
	}
	
	if (a.type == ECC_VALTYPE_BINARY && b.type == ECC_VALTYPE_BINARY)
	{
		a.data.binary += b.data.binary;
		return *ref = a;
	}
	
	a = retain(ECCNSValue.add(context, release(a), b));
	replaceRefValue(ref, a);
	context->refObject = refObject;
	return a;
}

eccvalue_t minusAssignRef (eccstate_t * const context)
{
	assignBinaryOpRef(a.data.binary -= b.data.binary);
}

eccvalue_t multiplyAssignRef (eccstate_t * const context)
{
	assignBinaryOpRef(a.data.binary *= b.data.binary);
}

eccvalue_t divideAssignRef (eccstate_t * const context)
{
	assignBinaryOpRef(a.data.binary /= b.data.binary);
}

eccvalue_t moduloAssignRef (eccstate_t * const context)
{
	assignBinaryOpRef(a.data.binary = fmod(a.data.binary, b.data.binary));
}

eccvalue_t leftShiftAssignRef (eccstate_t * const context)
{
	assignIntegerOpRef(a.data.integer <<= (uint32_t)b.data.integer);
}

eccvalue_t rightShiftAssignRef (eccstate_t * const context)
{
	assignIntegerOpRef(a.data.integer >>= (uint32_t)b.data.integer);
}

eccvalue_t unsignedRightShiftAssignRef (eccstate_t * const context)
{
	assignIntegerOpRef(a.data.integer = (uint32_t)a.data.integer >> (uint32_t)b.data.integer);
}

eccvalue_t bitAndAssignRef (eccstate_t * const context)
{
	assignIntegerOpRef(a.data.integer &= (uint32_t)b.data.integer);
}

eccvalue_t bitXorAssignRef (eccstate_t * const context)
{
	assignIntegerOpRef(a.data.integer ^= (uint32_t)b.data.integer);
}

eccvalue_t bitOrAssignRef (eccstate_t * const context)
{
	assignIntegerOpRef(a.data.integer |= (uint32_t)b.data.integer);
}


// MARK: Statement

eccvalue_t debugger (eccstate_t * const context)
{
	#if DEBUG
	debug = 1;
	#endif
	return trapOp(context, 0);
}

io_libecc_ecc_useframe
eccvalue_t try (eccstate_t * const context)
{
	eccobject_t *environment = context->environment;
	eccobject_t *refObject = context->refObject;
	const struct io_libecc_Op *end = context->ops + opValue().data.integer;
	struct io_libecc_Key key;
	
	const struct io_libecc_Op * volatile rethrowOps = NULL;
	volatile int rethrow = 0, breaker = 0;
	volatile eccvalue_t value = ECCValConstUndefined;
	eccvalue_t finallyValue;
	uint32_t indices[3];
	
	io_libecc_Pool.getIndices(indices);
	
	if (!setjmp(*ECCNSScript.pushEnv(context->ecc))) // try
		value = nextOp();
	else
	{
		value = context->ecc->result;
		context->ecc->result = ECCValConstUndefined;
		rethrowOps = context->ops;
		
		if (!rethrow) // catch
		{
			io_libecc_Pool.unreferenceFromIndices(indices);
			
			rethrow = 1;
			context->ops = end + 1; // bypass catch jump
			context->environment = environment;
			context->refObject = refObject;
			key = nextOp().data.key;
			
			if (!io_libecc_Key.isEqual(key, io_libecc_key_none))
			{
				if (value.type == ECC_VALTYPE_FUNCTION)
				{
					value.data.function->flags |= io_libecc_function_useBoundThis;
					value.data.function->boundThis = ECCNSValue.object(context->environment);
				}
				ECCNSObject.addMember(context->environment, key, value, io_libecc_value_sealed);
				value = nextOp(); // execute until noop
				rethrow = 0;
				if (context->breaker)
					popEnvironment(context);
			}
		}
		else // rethrow
			popEnvironment(context);
	}
	
	ECCNSScript.popEnv(context->ecc);
	
	breaker = context->breaker;
	context->breaker = 0;
	context->ops = end; // op[end] = io_libecc_Op.jump, to after catch
	finallyValue = nextOp(); // jump to after catch, and execute until noop
	
	if (context->breaker) /* return breaker */
		return finallyValue;
	else if (rethrow)
	{
		context->ops = rethrowOps;
		ECCNSContext.throw(context, retain(value));
	}
	else if (breaker)
	{
		context->breaker = breaker;
		return value;
	}
	else
		return nextOp();
}

io_libecc_ecc_noreturn
eccvalue_t throw (eccstate_t * const context)
{
	context->ecc->text = *opText(1);
	ECCNSContext.throw(context, retain(trapOp(context, 0)));
}

eccvalue_t with (eccstate_t * const context)
{
	eccobject_t *environment = context->environment;
	eccobject_t *refObject = context->refObject;
	eccobject_t *object = ECCNSValue.toObject(context, nextOp()).data.object;
	eccvalue_t value;
	
	if (!refObject)
		context->refObject = context->environment;
	
	context->environment = object;
	value = nextOp();
	context->environment = environment;
	context->refObject = refObject;
	
	if (context->breaker)
		return value;
	else
		return nextOp();
}

eccvalue_t next (eccstate_t * const context)
{
	return nextOp();
}

eccvalue_t nextIf (eccstate_t * const context)
{
	eccvalue_t value = opValue();
	
	if (!ECCNSValue.isTrue(trapOp(context, 1)))
		return value;
	
	return nextOp();
}

eccvalue_t autoreleaseExpression (eccstate_t * const context)
{
	uint32_t indices[3];
	
	io_libecc_Pool.getIndices(indices);
	release(context->ecc->result);
	context->ecc->result = retain(trapOp(context, 1));
	io_libecc_Pool.collectUnreferencedFromIndices(indices);
	return nextOp();
}

eccvalue_t autoreleaseDiscard (eccstate_t * const context)
{
	uint32_t indices[3];
	
	io_libecc_Pool.getIndices(indices);
	trapOp(context, 1);
	io_libecc_Pool.collectUnreferencedFromIndices(indices);
	return nextOp();
}

eccvalue_t expression (eccstate_t * const context)
{
	release(context->ecc->result);
	context->ecc->result = retain(trapOp(context, 1));
	return nextOp();
}

eccvalue_t discard (eccstate_t * const context)
{
	trapOp(context, 1);
	return nextOp();
}

eccvalue_t discardN (eccstate_t * const context)
{
	switch (opValue().data.integer)
	{
		default:
			ECCNSScript.fatal("Invalid discardN : %d", opValue().data.integer);
		
		case 16:
			trapOp(context, 1);
		case 15:
			trapOp(context, 1);
		case 14:
			trapOp(context, 1);
		case 13:
			trapOp(context, 1);
		case 12:
			trapOp(context, 1);
		case 11:
			trapOp(context, 1);
		case 10:
			trapOp(context, 1);
		case 9:
			trapOp(context, 1);
		case 8:
			trapOp(context, 1);
		case 7:
			trapOp(context, 1);
		case 6:
			trapOp(context, 1);
		case 5:
			trapOp(context, 1);
		case 4:
			trapOp(context, 1);
		case 3:
			trapOp(context, 1);
		case 2:
			trapOp(context, 1);
		case 1:
			trapOp(context, 1);
	}
	return nextOp();
}

eccvalue_t jump (eccstate_t * const context)
{
	int32_t offset = opValue().data.integer;
	context->ops += offset;
	return nextOp();
}

eccvalue_t jumpIf (eccstate_t * const context)
{
	int32_t offset = opValue().data.integer;
	eccvalue_t value;
	
	value = trapOp(context, 1);
	if (ECCNSValue.isTrue(value))
		context->ops += offset;
	
	return nextOp();
}

eccvalue_t jumpIfNot (eccstate_t * const context)
{
	int32_t offset = opValue().data.integer;
	eccvalue_t value;
	
	value = trapOp(context, 1);
	if (!ECCNSValue.isTrue(value))
		context->ops += offset;
	
	return nextOp();
}

eccvalue_t result (eccstate_t * const context)
{
	eccvalue_t result = trapOp(context, 0);
	context->breaker = -1;
	return result;
}

eccvalue_t repopulate (eccstate_t * const context)
{
	uint32_t index, count, arguments = opValue().data.integer + 3;
	int32_t offset = nextOp().data.integer;
	const struct io_libecc_Op *nextOps = context->ops + offset;
	
	{
		union io_libecc_object_Hashmap hashmap[context->environment->hashmapCapacity];
		count = arguments <= context->environment->hashmapCapacity? arguments: context->environment->hashmapCapacity;
		
		for (index = 0; index < 3; ++index)
			hashmap[index].value = context->environment->hashmap[index].value;
		
		for (; index < count; ++index)
		{
			release(context->environment->hashmap[index].value);
			hashmap[index].value = retain(nextOp());
		}
		
		if (index < context->environment->hashmapCapacity)
		{
			for (; index < context->environment->hashmapCapacity; ++index)
			{
				release(context->environment->hashmap[index].value);
				hashmap[index].value = ECCValConstNone;
			}
		}
		else
			for (; index < arguments; ++index)
				nextOp();
		
		if (context->environment->hashmap[2].value.type == ECC_VALTYPE_OBJECT) {
			eccobject_t *arguments = context->environment->hashmap[2].value.data.object;
			
			for (index = 3; index < context->environment->hashmapCapacity; ++index)
				arguments->element[index - 3].value = hashmap[index].value;
		}
		
		memcpy(context->environment->hashmap, hashmap, sizeof(hashmap));
	}
	
	context->ops = nextOps;
	return nextOp();
}

eccvalue_t resultVoid (eccstate_t * const context)
{
	eccvalue_t result = ECCValConstUndefined;
	context->breaker = -1;
	return result;
}

eccvalue_t switchOp (eccstate_t * const context)
{
	int32_t offset = opValue().data.integer;
	const struct io_libecc_Op *nextOps = context->ops + offset;
	eccvalue_t value, caseValue;
	const ecctextstring_t *text = opText(1);
	
	value = trapOp(context, 1);
	
	while (context->ops < nextOps)
	{
		const ecctextstring_t *textAlt = opText(1);
		caseValue = nextOp();
		
		ECCNSContext.setTexts(context, text, textAlt);
		if (ECCNSValue.isTrue(ECCNSValue.same(context, value, caseValue)))
		{
			offset = nextOp().data.integer;
			context->ops = nextOps + offset;
			break;
		}
		else
			++context->ops;
	}
	
	value = nextOp();
	if (context->breaker && --context->breaker)
		return value;
	else
	{
		context->ops = nextOps + 2 + nextOps[2].value.data.integer;
		return nextOp();
	}
}

// MARK: Iteration

#define stepIteration(value, nextOps, then) \
	{ \
		uint32_t indices[3]; \
		io_libecc_Pool.getIndices(indices); \
		value = nextOp(); \
		if (context->breaker && --context->breaker) \
		{ \
			if (--context->breaker) \
				return value; /* return breaker */\
			else \
				then; \
		} \
		else \
		{ \
			io_libecc_Pool.collectUnreferencedFromIndices(indices); \
			context->ops = nextOps; \
		} \
	}

eccvalue_t breaker (eccstate_t * const context)
{
	context->breaker = opValue().data.integer;
	return ECCValConstUndefined;
}

eccvalue_t iterate (eccstate_t * const context)
{
	const struct io_libecc_Op *startOps = context->ops;
	const struct io_libecc_Op *endOps = startOps;
	const struct io_libecc_Op *nextOps = startOps + 1;
	eccvalue_t value;
	int32_t skipOp = opValue().data.integer;
	
	context->ops = nextOps + skipOp;
	
	while (ECCNSValue.isTrue(nextOp()))
		stepIteration(value, nextOps, break);
	
	context->ops = endOps;
	
	return nextOp();
}

static
eccvalue_t iterateIntegerRef (
	eccstate_t * const context,
	int (*compareInteger) (int32_t, int32_t),
	int (*wontOverflow) (int32_t, int32_t),
	eccvalue_t (*compareValue) (eccstate_t * const, eccvalue_t, eccvalue_t),
	eccvalue_t (*valueStep) (eccstate_t * const, eccvalue_t, eccvalue_t))
{
	eccobject_t *refObject = context->refObject;
	const struct io_libecc_Op *endOps = context->ops + opValue().data.integer;
	eccvalue_t stepValue = nextOp();
	eccvalue_t *indexRef = nextOp().data.reference;
	eccvalue_t *countRef = nextOp().data.reference;
	const struct io_libecc_Op *nextOps = context->ops;
	eccvalue_t value;
	
	if (indexRef->type == ECC_VALTYPE_BINARY && indexRef->data.binary >= INT32_MIN && indexRef->data.binary <= INT32_MAX)
	{
		eccvalue_t integerValue = ECCNSValue.toInteger(context, *indexRef);
		if (indexRef->data.binary == integerValue.data.integer)
			*indexRef = integerValue;
	}
	
	if (countRef->type == ECC_VALTYPE_BINARY && countRef->data.binary >= INT32_MIN && countRef->data.binary <= INT32_MAX)
	{
		eccvalue_t integerValue = ECCNSValue.toInteger(context, *countRef);
		if (countRef->data.binary == integerValue.data.integer)
			*countRef = integerValue;
	}
	
	if (indexRef->type == ECC_VALTYPE_INTEGER && countRef->type == ECC_VALTYPE_INTEGER)
	{
		int32_t step = valueStep == ECCNSValue.add? stepValue.data.integer: -stepValue.data.integer;
		if (!wontOverflow(countRef->data.integer, step - 1))
			goto deoptimize;
		
		for (; compareInteger(indexRef->data.integer, countRef->data.integer); indexRef->data.integer += step)
			stepIteration(value, nextOps, goto done);
	}
	
deoptimize:
	for (; ECCNSValue.isTrue(compareValue(context, *indexRef, *countRef))
		 ; *indexRef = valueStep(context, *indexRef, stepValue)
		 )
		stepIteration(value, nextOps, break);
	
done:
	context->refObject = refObject;
	context->ops = endOps;
	return nextOp();
}

eccvalue_t iterateLessRef (eccstate_t * const context)
{
	return iterateIntegerRef(context, integerLess, integerWontOverflowPositive, ECCNSValue.less, ECCNSValue.add);
}

eccvalue_t iterateLessOrEqualRef (eccstate_t * const context)
{
	return iterateIntegerRef(context, integerLessOrEqual, integerWontOverflowPositive, ECCNSValue.lessOrEqual, ECCNSValue.add);
}

eccvalue_t iterateMoreRef (eccstate_t * const context)
{
	return iterateIntegerRef(context, integerMore, integerWontOverflowNegative, ECCNSValue.more, ECCNSValue.subtract);
}

eccvalue_t iterateMoreOrEqualRef (eccstate_t * const context)
{
	return iterateIntegerRef(context, integerMoreOrEqual, integerWontOverflowNegative, ECCNSValue.moreOrEqual, ECCNSValue.subtract);
}

eccvalue_t iterateInRef (eccstate_t * const context)
{
	eccobject_t *refObject = context->refObject;
	eccvalue_t *ref = nextOp().data.reference;
	eccvalue_t target = nextOp();
	eccvalue_t value = nextOp(), key;
	struct io_libecc_chars_Append chars;
	eccobject_t *object;
	const struct io_libecc_Op *startOps = context->ops;
	const struct io_libecc_Op *endOps = startOps + value.data.integer;
	uint32_t index, count;
	
	if (ECCNSValue.isObject(target))
	{
		object = target.data.object;
		do
		{
			count = object->elementCount < object->elementCapacity? object->elementCount : object->elementCapacity;
			for (index = 0; index < count; ++index)
			{
				union io_libecc_object_Element *element = object->element + index;
				
				if (element->value.check != 1 || (element->value.flags & io_libecc_value_hidden))
					continue;
				
				if (object != target.data.object && &element->value != ECCNSObject.element(target.data.object, index, 0))
					continue;
				
				io_libecc_Chars.beginAppend(&chars);
				io_libecc_Chars.append(&chars, "%d", index);
				key = io_libecc_Chars.endAppend(&chars);
				replaceRefValue(ref, key);
				
				stepIteration(value, startOps, break);
			}
		}
		while (( object = object->prototype ));
		
		object = target.data.object;
		do
		{
			count = object->hashmapCount;
			for (index = 2; index < count; ++index)
			{
				union io_libecc_object_Hashmap *hashmap = object->hashmap + index;
				
				if (hashmap->value.check != 1 || (hashmap->value.flags & io_libecc_value_hidden))
					continue;
				
				if (object != target.data.object && &hashmap->value != ECCNSObject.member(target.data.object, hashmap->value.key, 0))
					continue;
				
				key = ECCNSValue.text(io_libecc_Key.textOf(hashmap->value.key));
				replaceRefValue(ref, key);
				
				stepIteration(value, startOps, break);
			}
		}
		while (( object = object->prototype ));
	}
	
	context->refObject = refObject;
	context->ops = endOps;
	return nextOp();
}
