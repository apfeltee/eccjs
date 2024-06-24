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
	io_libecc_Env.printColor(0, io_libecc_env_bold, "\n\t-- libecc: basic gdb/lldb commands --\n");
	io_libecc_Env.printColor(io_libecc_env_green, io_libecc_env_bold, "\tstep-in\n");
	fprintf(stderr, "\t  c\n");
	io_libecc_Env.printColor(io_libecc_env_green, io_libecc_env_bold, "\tcontinue\n");
	fprintf(stderr, "\t  p debug=0\n");
	fprintf(stderr, "\t  c\n\n");
}

static
struct eccvalue_t trapOp_(struct eccstate_t *context, int offset)
{
	const struct io_libecc_Text *text = opText(offset);
	if (debug && text->bytes && text->length)
	{
		io_libecc_Env.newline();
		io_libecc_Context.printBacktrace(context);
		io_libecc_Ecc.printTextInput(context->ecc, *text, 1);
		trap();
	}
	return nextOp();
}
#define _ return trapOp_(context, offset);

static
struct eccvalue_t trapOp(struct eccstate_t *context, int offset)
{
_/*     gdb/lldb infos: p usage()     */
}


#undef _

#else
	#define trapOp(context, offset) nextOp()
#endif

//

static struct io_libecc_Op make(const io_libecc_native_io_libecc_Function native, struct eccvalue_t value, struct io_libecc_Text text);
static const char* toChars(const io_libecc_native_io_libecc_Function native);
static struct eccvalue_t
callFunctionArguments(struct eccstate_t* const, enum io_libecc_context_Offset, struct io_libecc_Function* function, struct eccvalue_t this, struct eccobject_t* arguments);
static struct eccvalue_t
callFunctionVA(struct eccstate_t* const, enum io_libecc_context_Offset, struct io_libecc_Function* function, struct eccvalue_t this, int argumentCount, va_list ap);
static struct eccvalue_t noop(struct eccstate_t* const);
static struct eccvalue_t value(struct eccstate_t* const);
static struct eccvalue_t valueConstRef(struct eccstate_t* const);
static struct eccvalue_t text(struct eccstate_t* const);
static struct eccvalue_t function(struct eccstate_t* const);
static struct eccvalue_t object(struct eccstate_t* const);
static struct eccvalue_t array(struct eccstate_t* const);
static struct eccvalue_t regexp(struct eccstate_t* const);
static struct eccvalue_t this(struct eccstate_t* const);
static struct eccvalue_t createLocalRef(struct eccstate_t* const);
static struct eccvalue_t getLocalRefOrNull(struct eccstate_t* const);
static struct eccvalue_t getLocalRef(struct eccstate_t* const);
static struct eccvalue_t getLocal(struct eccstate_t* const);
static struct eccvalue_t setLocal(struct eccstate_t* const);
static struct eccvalue_t deleteLocal(struct eccstate_t* const);
static struct eccvalue_t getLocalSlotRef(struct eccstate_t* const);
static struct eccvalue_t getLocalSlot(struct eccstate_t* const);
static struct eccvalue_t setLocalSlot(struct eccstate_t* const);
static struct eccvalue_t deleteLocalSlot(struct eccstate_t* const);
static struct eccvalue_t getParentSlotRef(struct eccstate_t* const);
static struct eccvalue_t getParentSlot(struct eccstate_t* const);
static struct eccvalue_t setParentSlot(struct eccstate_t* const);
static struct eccvalue_t deleteParentSlot(struct eccstate_t* const);
static struct eccvalue_t getMemberRef(struct eccstate_t* const);
static struct eccvalue_t getMember(struct eccstate_t* const);
static struct eccvalue_t setMember(struct eccstate_t* const);
static struct eccvalue_t callMember(struct eccstate_t* const);
static struct eccvalue_t deleteMember(struct eccstate_t* const);
static struct eccvalue_t getPropertyRef(struct eccstate_t* const);
static struct eccvalue_t getProperty(struct eccstate_t* const);
static struct eccvalue_t setProperty(struct eccstate_t* const);
static struct eccvalue_t callProperty(struct eccstate_t* const);
static struct eccvalue_t deleteProperty(struct eccstate_t* const);
static struct eccvalue_t pushEnvironment(struct eccstate_t* const);
static struct eccvalue_t popEnvironment(struct eccstate_t* const);
static struct eccvalue_t exchange(struct eccstate_t* const);
static struct eccvalue_t typeOf(struct eccstate_t* const);
static struct eccvalue_t equal(struct eccstate_t* const);
static struct eccvalue_t notEqual(struct eccstate_t* const);
static struct eccvalue_t identical(struct eccstate_t* const);
static struct eccvalue_t notIdentical(struct eccstate_t* const);
static struct eccvalue_t less(struct eccstate_t* const);
static struct eccvalue_t lessOrEqual(struct eccstate_t* const);
static struct eccvalue_t more(struct eccstate_t* const);
static struct eccvalue_t moreOrEqual(struct eccstate_t* const);
static struct eccvalue_t instanceOf(struct eccstate_t* const);
static struct eccvalue_t in(struct eccstate_t* const);
static struct eccvalue_t add(struct eccstate_t* const);
static struct eccvalue_t minus(struct eccstate_t* const);
static struct eccvalue_t multiply(struct eccstate_t* const);
static struct eccvalue_t divide(struct eccstate_t* const);
static struct eccvalue_t modulo(struct eccstate_t* const);
static struct eccvalue_t leftShift(struct eccstate_t* const);
static struct eccvalue_t rightShift(struct eccstate_t* const);
static struct eccvalue_t unsignedRightShift(struct eccstate_t* const);
static struct eccvalue_t bitwiseAnd(struct eccstate_t* const);
static struct eccvalue_t bitwiseXor(struct eccstate_t* const);
static struct eccvalue_t bitwiseOr(struct eccstate_t* const);
static struct eccvalue_t logicalAnd(struct eccstate_t* const);
static struct eccvalue_t logicalOr(struct eccstate_t* const);
static struct eccvalue_t positive(struct eccstate_t* const);
static struct eccvalue_t negative(struct eccstate_t* const);
static struct eccvalue_t invert(struct eccstate_t* const);
static struct eccvalue_t not(struct eccstate_t* const);
static struct eccvalue_t construct(struct eccstate_t* const);
static struct eccvalue_t call(struct eccstate_t* const);
static struct eccvalue_t eval(struct eccstate_t* const);
static struct eccvalue_t incrementRef(struct eccstate_t* const);
static struct eccvalue_t decrementRef(struct eccstate_t* const);
static struct eccvalue_t postIncrementRef(struct eccstate_t* const);
static struct eccvalue_t postDecrementRef(struct eccstate_t* const);
static struct eccvalue_t addAssignRef(struct eccstate_t* const);
static struct eccvalue_t minusAssignRef(struct eccstate_t* const);
static struct eccvalue_t multiplyAssignRef(struct eccstate_t* const);
static struct eccvalue_t divideAssignRef(struct eccstate_t* const);
static struct eccvalue_t moduloAssignRef(struct eccstate_t* const);
static struct eccvalue_t leftShiftAssignRef(struct eccstate_t* const);
static struct eccvalue_t rightShiftAssignRef(struct eccstate_t* const);
static struct eccvalue_t unsignedRightShiftAssignRef(struct eccstate_t* const);
static struct eccvalue_t bitAndAssignRef(struct eccstate_t* const);
static struct eccvalue_t bitXorAssignRef(struct eccstate_t* const);
static struct eccvalue_t bitOrAssignRef(struct eccstate_t* const);
static struct eccvalue_t debugger(struct eccstate_t* const);
static struct eccvalue_t try(struct eccstate_t* const);
static struct eccvalue_t throw(struct eccstate_t* const);
static struct eccvalue_t with(struct eccstate_t* const);
static struct eccvalue_t next(struct eccstate_t* const);
static struct eccvalue_t nextIf(struct eccstate_t* const);
static struct eccvalue_t autoreleaseExpression(struct eccstate_t* const);
static struct eccvalue_t autoreleaseDiscard(struct eccstate_t* const);
static struct eccvalue_t expression(struct eccstate_t* const);
static struct eccvalue_t discard(struct eccstate_t* const);
static struct eccvalue_t discardN(struct eccstate_t* const);
static struct eccvalue_t jump(struct eccstate_t* const);
static struct eccvalue_t jumpIf(struct eccstate_t* const);
static struct eccvalue_t jumpIfNot(struct eccstate_t* const);
static struct eccvalue_t repopulate(struct eccstate_t* const);
static struct eccvalue_t result(struct eccstate_t* const);
static struct eccvalue_t resultVoid(struct eccstate_t* const);
static struct eccvalue_t switchOp(struct eccstate_t* const);
static struct eccvalue_t breaker(struct eccstate_t* const);
static struct eccvalue_t iterate(struct eccstate_t* const);
static struct eccvalue_t iterateLessRef(struct eccstate_t* const);
static struct eccvalue_t iterateMoreRef(struct eccstate_t* const);
static struct eccvalue_t iterateLessOrEqualRef(struct eccstate_t* const);
static struct eccvalue_t iterateMoreOrEqualRef(struct eccstate_t* const);
static struct eccvalue_t iterateInRef(struct eccstate_t* const);
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
struct eccvalue_t retain (struct eccvalue_t value)
{
	if (value.type == io_libecc_value_charsType)
		++value.data.chars->referenceCount;
	if (value.type >= io_libecc_value_objectType)
		++value.data.object->referenceCount;
	
	return value;
}

static
struct eccvalue_t release (struct eccvalue_t value)
{
	if (value.type == io_libecc_value_charsType)
		--value.data.chars->referenceCount;
	if (value.type >= io_libecc_value_objectType)
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

struct io_libecc_Op make (const io_libecc_native_io_libecc_Function native, struct eccvalue_t value, struct io_libecc_Text text)
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
struct eccvalue_t nextOpValue (struct eccstate_t * const context)
{
	struct eccvalue_t value = nextOp();
	value.flags &= ~(io_libecc_value_readonly | io_libecc_value_hidden | io_libecc_value_sealed);
	return value;
}

#define nextOpValue() nextOpValue(context)

static
struct eccvalue_t replaceRefValue (struct eccvalue_t *ref, struct eccvalue_t value)
{
	ref->data = value.data;
	ref->type = value.type;
	return value;
}

static inline
struct eccvalue_t callOps (struct eccstate_t * const context, struct eccobject_t *environment)
{
	if (context->depth >= context->ecc->maximumCallDepth)
		io_libecc_Context.rangeError(context, io_libecc_Chars.create("maximum depth exceeded"));
	
//	if (!context->parent->strictMode)
//		if (context->this.type == io_libecc_value_undefinedType || context->this.type == io_libecc_value_nullType)
//			context->this = io_libecc_Value.object(&context->ecc->global->environment);
	
	context->environment = environment;
	return context->ops->native(context);
}

static inline
struct eccvalue_t callOpsRelease (struct eccstate_t * const context, struct eccobject_t *environment)
{
	struct eccvalue_t result;
	uint16_t index, count;
	
	result = callOps(context, environment);
	
	for (index = 2, count = environment->hashmapCount; index < count; ++index)
		release(environment->hashmap[index].value);
	
	return result;
}

static inline
void populateEnvironmentWithArguments (struct eccobject_t *environment, struct eccobject_t *arguments, int32_t parameterCount)
{
	int32_t index = 0;
	int argumentCount = arguments->elementCount;
	
	replaceRefValue(&environment->hashmap[2].value, retain(io_libecc_Value.object(arguments)));
	
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
void populateEnvironmentAndArgumentsWithVA (struct eccobject_t *environment, int32_t parameterCount, int32_t argumentCount, va_list ap)
{
	int32_t index = 0;
	struct eccobject_t *arguments = io_libecc_Arguments.createSized(argumentCount);
	
	replaceRefValue(&environment->hashmap[2].value, retain(io_libecc_Value.object(arguments)));
	
	if (argumentCount <= parameterCount)
		for (; index < argumentCount; ++index)
			environment->hashmap[index + 3].value = arguments->element[index].value = retain(va_arg(ap, struct eccvalue_t));
	else
	{
		for (; index < parameterCount; ++index)
			environment->hashmap[index + 3].value = arguments->element[index].value = retain(va_arg(ap, struct eccvalue_t));
		
		for (; index < argumentCount; ++index)
			arguments->element[index].value = retain(va_arg(ap, struct eccvalue_t));
	}
}

static inline
void populateEnvironmentWithVA (struct eccobject_t *environment, int32_t parameterCount, int32_t argumentCount, va_list ap)
{
	int32_t index = 0;
	if (argumentCount <= parameterCount)
		for (; index < argumentCount; ++index)
			environment->hashmap[index + 3].value = retain(va_arg(ap, struct eccvalue_t));
	else
		for (; index < parameterCount; ++index)
			environment->hashmap[index + 3].value = retain(va_arg(ap, struct eccvalue_t));
}

static inline
void populateStackEnvironmentAndArgumentsWithOps (struct eccstate_t * const context, struct eccobject_t *environment, struct eccobject_t *arguments, int32_t parameterCount, int32_t argumentCount)
{
	int32_t index = 0;
	
	replaceRefValue(&environment->hashmap[2].value, io_libecc_Value.object(arguments));
	
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
void populateEnvironmentAndArgumentsWithOps (struct eccstate_t * const context, struct eccobject_t *environment, struct eccobject_t *arguments, int32_t parameterCount, int32_t argumentCount)
{
	populateStackEnvironmentAndArgumentsWithOps(context, environment, arguments, parameterCount, argumentCount);
	
	retain(io_libecc_Value.object(arguments));
	
	if (argumentCount > parameterCount)
	{
		int32_t index = parameterCount;
		for (; index < argumentCount; ++index)
			retain(arguments->element[index].value);
	}
}

static inline
void populateEnvironmentWithOps (struct eccstate_t * const context, struct eccobject_t *environment, int32_t parameterCount, int32_t argumentCount)
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

struct eccvalue_t callFunctionArguments (struct eccstate_t * const context, enum io_libecc_context_Offset offset, struct io_libecc_Function *function, struct eccvalue_t this, struct eccobject_t *arguments)
{
	struct eccstate_t subContext = {
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
		struct eccobject_t *environment = io_libecc_Object.copy(&function->environment);
		
		if (function->flags & io_libecc_function_needArguments)
		{
			struct eccobject_t *copy = io_libecc_Arguments.createSized(arguments->elementCount);
			memcpy(copy->element, arguments->element, sizeof(*copy->element) * copy->elementCount);
			arguments = copy;
		}
		populateEnvironmentWithArguments(environment, arguments, function->parameterCount);
		
		return callOps(&subContext, environment);
	}
	else
	{
		struct eccobject_t environment = function->environment;
		union io_libecc_object_Hashmap hashmap[function->environment.hashmapCapacity];
		
		memcpy(hashmap, function->environment.hashmap, sizeof(hashmap));
		environment.hashmap = hashmap;
		populateEnvironmentWithArguments(&environment, arguments, function->parameterCount);
		
		return callOpsRelease(&subContext, &environment);
	}
}

struct eccvalue_t callFunctionVA (struct eccstate_t * const context, enum io_libecc_context_Offset offset, struct io_libecc_Function *function, struct eccvalue_t this, int argumentCount, va_list ap)
{
	struct eccstate_t subContext = {
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
		struct eccobject_t *environment = io_libecc_Object.copy(&function->environment);
		
		if (function->flags & io_libecc_function_needArguments)
		{
			populateEnvironmentAndArgumentsWithVA(environment, function->parameterCount, argumentCount, ap);
			
			if (!context->strictMode)
			{
				io_libecc_Object.addMember(environment->hashmap[2].value.data.object, io_libecc_key_callee, retain(io_libecc_Value.function(function)), io_libecc_value_hidden);
				io_libecc_Object.addMember(environment->hashmap[2].value.data.object, io_libecc_key_length, io_libecc_Value.integer(argumentCount), io_libecc_value_hidden);
			}
		}
		else
			populateEnvironmentWithVA(environment, function->parameterCount, argumentCount, ap);
		
		return callOps(&subContext, environment);
	}
	else
	{
		struct eccobject_t environment = function->environment;
		union io_libecc_object_Hashmap hashmap[function->environment.hashmapCapacity];
		
		memcpy(hashmap, function->environment.hashmap, sizeof(hashmap));
		environment.hashmap = hashmap;
		
		populateEnvironmentWithVA(&environment, function->parameterCount, argumentCount, ap);
		
		return callOpsRelease(&subContext, &environment);
	}
}

static inline
struct eccvalue_t callFunction (struct eccstate_t * const context, struct io_libecc_Function * const function, struct eccvalue_t this, int32_t argumentCount, int construct)
{
	struct eccstate_t subContext = {
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
		struct eccobject_t *environment = io_libecc_Object.copy(&function->environment);
		
		if (function->flags & io_libecc_function_needArguments)
		{
			populateEnvironmentAndArgumentsWithOps(context, environment, io_libecc_Arguments.createSized(argumentCount), function->parameterCount, argumentCount);
			
			if (!context->strictMode)
			{
				io_libecc_Object.addMember(environment->hashmap[2].value.data.object, io_libecc_key_callee, retain(io_libecc_Value.function(function)), io_libecc_value_hidden);
				io_libecc_Object.addMember(environment->hashmap[2].value.data.object, io_libecc_key_length, io_libecc_Value.integer(argumentCount), io_libecc_value_hidden);
			}
		}
		else
			populateEnvironmentWithOps(context, environment, function->parameterCount, argumentCount);
		
		return callOps(&subContext, environment);
	}
	else if (function->flags & io_libecc_function_needArguments)
	{
		struct eccobject_t environment = function->environment;
		struct eccobject_t arguments = io_libecc_Object.identity;
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
		struct eccobject_t environment = function->environment;
		union io_libecc_object_Hashmap hashmap[function->environment.hashmapCapacity];
		
		memcpy(hashmap, function->environment.hashmap, sizeof(hashmap));
		environment.hashmap = hashmap;
		populateEnvironmentWithOps(context, &environment, function->parameterCount, argumentCount);
		
		return callOpsRelease(&subContext, &environment);
	}
}

static inline
struct eccvalue_t callValue (struct eccstate_t * const context, struct eccvalue_t value, struct eccvalue_t this, int32_t argumentCount, int construct, const struct io_libecc_Text *textCall)
{
	struct eccvalue_t result;
	const struct io_libecc_Text *parentTextCall = context->textCall;
	
	if (value.type != io_libecc_value_functionType)
		io_libecc_Context.typeError(context, io_libecc_Chars.create("'%.*s' is not a function", context->text->length, context->text->bytes));
	
	context->textCall = textCall;
	
	if (value.data.function->flags & io_libecc_function_useBoundThis)
		result = callFunction(context, value.data.function, value.data.function->boundThis, argumentCount, construct);
	else
		result = callFunction(context, value.data.function, this, argumentCount, construct);
	
	context->textCall = parentTextCall;
	return result;
}

struct eccvalue_t construct (struct eccstate_t * const context)
{
	const struct io_libecc_Text *textCall = opText(0);
	const struct io_libecc_Text *text = opText(1);
	int32_t argumentCount = opValue().data.integer;
	struct eccvalue_t value, *prototype, object, function = nextOp();
	
	if (function.type != io_libecc_value_functionType)
		goto error;
	
	prototype = io_libecc_Object.member(&function.data.function->object, io_libecc_key_prototype, 0);
	if (!prototype)
		goto error;
	
	if (!io_libecc_Value.isObject(*prototype))
		object = io_libecc_Value.object(io_libecc_Object.create(io_libecc_object_prototype));
	else if (prototype->type == io_libecc_value_functionType)
	{
		object = io_libecc_Value.object(io_libecc_Object.create(NULL));
		object.data.object->prototype = &prototype->data.function->object;
	}
	else if (prototype->type == io_libecc_value_objectType)
		object = io_libecc_Value.object(io_libecc_Object.create(prototype->data.object));
	else
		object = io_libecc_value_undefined;
	
	io_libecc_Context.setText(context, text);
	value = callValue(context, function, object, argumentCount, 1, textCall);
	
	if (io_libecc_Value.isObject(value))
		return value;
	else
		return object;
	
error:
	context->textCall = textCall;
	io_libecc_Context.setTextIndex(context, io_libecc_context_funcIndex);
	io_libecc_Context.typeError(context, io_libecc_Chars.create("'%.*s' is not a constructor", text->length, text->bytes));
}

struct eccvalue_t call (struct eccstate_t * const context)
{
	const struct io_libecc_Text *textCall = opText(0);
	const struct io_libecc_Text *text = opText(1);
	int32_t argumentCount = opValue().data.integer;
	struct eccvalue_t value;
	struct eccvalue_t this;
	
	context->inEnvironmentObject = 0;
	value = nextOp();
	
	if (context->inEnvironmentObject)
	{
		struct eccstate_t *seek = context;
		while (seek->parent && seek->parent->refObject == context->refObject)
			seek = seek->parent;
		
		++seek->environment->referenceCount;
		this = io_libecc_Value.objectValue(seek->environment);
	}
	else
		this = io_libecc_value_undefined;
	
	io_libecc_Context.setText(context, text);
	return callValue(context, value, this, argumentCount, 0, textCall);
}

struct eccvalue_t eval (struct eccstate_t * const context)
{
	struct eccvalue_t value;
	struct io_libecc_Input *input;
	int32_t argumentCount = opValue().data.integer;
	struct eccstate_t subContext = {
		.parent = context,
		.this = context->this,
		.environment = context->environment,
		.ecc = context->ecc,
		.depth = context->depth + 1,
		.strictMode = context->strictMode,
	};
	
	if (!argumentCount)
		return io_libecc_value_undefined;
	
	value = nextOp();
	while (--argumentCount)
		nextOp();
	
	if (!io_libecc_Value.isString(value) || !io_libecc_Value.isPrimitive(value))
		return value;
	
	input = io_libecc_Input.createFromBytes(io_libecc_Value.stringBytes(&value), io_libecc_Value.stringLength(&value), "(eval)");
	io_libecc_Ecc.evalInputWithContext(context->ecc, input, &subContext);
	
	value = context->ecc->result;
	context->ecc->result = io_libecc_value_undefined;
	return value;
}


// Expression

struct eccvalue_t noop (struct eccstate_t * const context)
{
	return io_libecc_value_undefined;
}

struct eccvalue_t value (struct eccstate_t * const context)
{
	return opValue();
}

struct eccvalue_t valueConstRef (struct eccstate_t * const context)
{
	return io_libecc_Value.reference((struct eccvalue_t *)&opValue());
}

struct eccvalue_t text (struct eccstate_t * const context)
{
	return io_libecc_Value.text(opText(0));
}

struct eccvalue_t regexp (struct eccstate_t * const context)
{
	const struct io_libecc_Text *text = opText(0);
	struct io_libecc_Error *error = NULL;
	struct io_libecc_Chars *chars = io_libecc_Chars.createWithBytes(text->length, text->bytes);
	struct io_libecc_RegExp *regexp = io_libecc_RegExp.create(chars, &error, context->ecc->sloppyMode? io_libecc_regexp_allowUnicodeFlags: 0);
	if (error)
	{
		error->text.bytes = text->bytes + (error->text.bytes - chars->bytes);
		io_libecc_Context.throw(context, io_libecc_Value.error(error));
	}
	return io_libecc_Value.regexp(regexp);
}

struct eccvalue_t function (struct eccstate_t * const context)
{
	struct eccobject_t *prototype;
	struct eccvalue_t value = opValue(), result;
	
	struct io_libecc_Function *function = io_libecc_Function.copy(value.data.function);
	function->object.prototype = &value.data.function->object;
	function->environment.prototype = context->environment;
	if (context->refObject)
	{
		++context->refObject->referenceCount;
		function->refObject = context->refObject;
	}
	
	prototype = io_libecc_Object.create(io_libecc_object_prototype);
	io_libecc_Function.linkPrototype(function, io_libecc_Value.object(prototype), io_libecc_value_sealed);
	
	++prototype->referenceCount;
	++context->environment->referenceCount;
	++function->object.referenceCount;
	
	result = io_libecc_Value.function(function);
	result.flags = value.flags;
	return result;
}

struct eccvalue_t object (struct eccstate_t * const context)
{
	struct eccobject_t *object = io_libecc_Object.create(io_libecc_object_prototype);
	struct eccvalue_t property, value;
	uint32_t count;
	
	object->flags |= io_libecc_object_mark;
	
	for (count = opValue().data.integer; count--;)
	{
		property = nextOp();
		value = retain(nextOpValue());
		
		if (property.type == io_libecc_value_keyType)
			io_libecc_Object.addMember(object, property.data.key, value, 0);
		else if (property.type == io_libecc_value_integerType)
			io_libecc_Object.addElement(object, property.data.integer, value, 0);
	}
	return io_libecc_Value.object(object);
}

struct eccvalue_t array (struct eccstate_t * const context)
{
	uint32_t length = opValue().data.integer;
	struct eccobject_t *object = io_libecc_Array.createSized(length);
	struct eccvalue_t value;
	uint32_t index;
	
	object->flags |= io_libecc_object_mark;
	
	for (index = 0; index < length; ++index)
	{
		value = retain(nextOpValue());
		object->element[index].value = value;
	}
	return io_libecc_Value.object(object);
}

struct eccvalue_t this (struct eccstate_t * const context)
{
	return context->this;
}

static
struct eccvalue_t * localRef (struct eccstate_t * const context, struct io_libecc_Key key, const struct io_libecc_Text *text, int required)
{
	struct eccvalue_t *ref;
	
	ref = io_libecc_Object.member(context->environment, key, 0);
	
	if (!context->strictMode)
	{
		context->inEnvironmentObject = context->refObject != NULL;
		if (!ref && context->refObject)
		{
			context->inEnvironmentObject = 0;
			ref = io_libecc_Object.member(context->refObject, key, 0);
		}
	}
	
	if (!ref && required)
	{
		io_libecc_Context.setText(context, text);
		io_libecc_Context.referenceError(context, io_libecc_Chars.create("'%.*s' is not defined", io_libecc_Key.textOf(key)->length, io_libecc_Key.textOf(key)->bytes));
	}
	return ref;
}

struct eccvalue_t createLocalRef (struct eccstate_t * const context)
{
	struct io_libecc_Key key = opValue().data.key;
	struct eccvalue_t *ref = localRef(context, key, opText(0), context->strictMode);
	
	if (!ref)
		ref = io_libecc_Object.addMember(&context->ecc->global->environment, key, io_libecc_value_undefined, 0);
	
	return io_libecc_Value.reference(ref);
}

struct eccvalue_t getLocalRefOrNull (struct eccstate_t * const context)
{
	return io_libecc_Value.reference(localRef(context, opValue().data.key, opText(0), 0));
}

struct eccvalue_t getLocalRef (struct eccstate_t * const context)
{
	return io_libecc_Value.reference(localRef(context, opValue().data.key, opText(0), 1));
}

struct eccvalue_t getLocal (struct eccstate_t * const context)
{
	return *localRef(context, opValue().data.key, opText(0), 1);
}

struct eccvalue_t setLocal (struct eccstate_t * const context)
{
	const struct io_libecc_Text *text = opText(0);
	struct io_libecc_Key key = opValue().data.key;
	struct eccvalue_t value = nextOp();
	
	struct eccvalue_t *ref = localRef(context, key, text, context->strictMode);
	
	if (!ref)
		ref = io_libecc_Object.addMember(&context->ecc->global->environment, key, io_libecc_value_undefined, 0);
	
	if (ref->flags & io_libecc_value_readonly)
		return value;
	
	retain(value);
	release(*ref);
	replaceRefValue(ref, value);
	return value;
}

struct eccvalue_t deleteLocal (struct eccstate_t * const context)
{
	struct eccvalue_t *ref = localRef(context, opValue().data.key, opText(0), 0);
	
	if (!ref)
		return io_libecc_value_true;
	
	if (ref->flags & io_libecc_value_sealed)
		return io_libecc_value_false;
	
	*ref = io_libecc_value_none;
	return io_libecc_value_true;
}

struct eccvalue_t getLocalSlotRef (struct eccstate_t * const context)
{
	return io_libecc_Value.reference(&context->environment->hashmap[opValue().data.integer].value);
}

struct eccvalue_t getLocalSlot (struct eccstate_t * const context)
{
	return context->environment->hashmap[opValue().data.integer].value;
}

struct eccvalue_t setLocalSlot (struct eccstate_t * const context)
{
	int32_t slot = opValue().data.integer;
	struct eccvalue_t value = nextOp();
	struct eccvalue_t *ref = &context->environment->hashmap[slot].value;
	
	if (ref->flags & io_libecc_value_readonly)
		return value;
	
	retain(value);
	release(*ref);
	replaceRefValue(ref, value);
	return value;
}

struct eccvalue_t deleteLocalSlot (struct eccstate_t * const context)
{
	return io_libecc_value_false;
}

struct eccvalue_t getParentSlotRef (struct eccstate_t * const context)
{
	int32_t slot = opValue().data.integer & 0xffff;
	int32_t count = opValue().data.integer >> 16;
	struct eccobject_t *object = context->environment;
	while (count--)
		object = object->prototype;
	
	return io_libecc_Value.reference(&object->hashmap[slot].value);
}

struct eccvalue_t getParentSlot (struct eccstate_t * const context)
{
	return *getParentSlotRef(context).data.reference;
}

struct eccvalue_t setParentSlot (struct eccstate_t * const context)
{
	const struct io_libecc_Text *text = opText(0);
	struct eccvalue_t *ref = getParentSlotRef(context).data.reference;
	struct eccvalue_t value = nextOp();
	if (ref->flags & io_libecc_value_readonly)
	{
		if (context->strictMode)
		{
			struct io_libecc_Text property = *io_libecc_Key.textOf(ref->key);
			io_libecc_Context.setText(context, text);
			io_libecc_Context.typeError(context, io_libecc_Chars.create("'%.*s' is read-only", property.length, property.bytes));
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

struct eccvalue_t deleteParentSlot (struct eccstate_t * const context)
{
	return io_libecc_value_false;
}

static
void prepareObject (struct eccstate_t * const context, struct eccvalue_t *object)
{
	const struct io_libecc_Text *textObject = opText(1);
	*object = nextOp();
	
	if (io_libecc_Value.isPrimitive(*object))
	{
		io_libecc_Context.setText(context, textObject);
		*object = io_libecc_Value.toObject(context, *object);
	}
}

struct eccvalue_t getMemberRef (struct eccstate_t * const context)
{
	const struct io_libecc_Text *text = opText(0);
	struct io_libecc_Key key = opValue().data.key;
	struct eccvalue_t object, *ref;
	
	prepareObject(context, &object);
	
	context->refObject = object.data.object;
	ref = io_libecc_Object.member(object.data.object, key, io_libecc_value_asOwn);
	
	if (!ref)
	{
		if (object.data.object->flags & io_libecc_object_sealed)
		{
			io_libecc_Context.setText(context, text);
			io_libecc_Context.typeError(context, io_libecc_Chars.create("object is not extensible"));
		}
		ref = io_libecc_Object.addMember(object.data.object, key, io_libecc_value_undefined, 0);
	}
	
	return io_libecc_Value.reference(ref);
}

struct eccvalue_t getMember (struct eccstate_t * const context)
{
	struct io_libecc_Key key = opValue().data.key;
	struct eccvalue_t object;
	
	prepareObject(context, &object);
	
	return io_libecc_Object.getMember(context, object.data.object, key);
}

struct eccvalue_t setMember (struct eccstate_t * const context)
{
	const struct io_libecc_Text *text = opText(0);
	struct io_libecc_Key key = opValue().data.key;
	struct eccvalue_t object, value;
	
	prepareObject(context, &object);
	value = retain(nextOp());
	
	io_libecc_Context.setText(context, text);
	io_libecc_Object.putMember(context, object.data.object, key, value);
	
	return value;
}

struct eccvalue_t callMember (struct eccstate_t * const context)
{
	const struct io_libecc_Text *textCall = opText(0);
	int32_t argumentCount = opValue().data.integer;
	const struct io_libecc_Text *text = &(++context->ops)->text;
	struct io_libecc_Key key = opValue().data.key;
	struct eccvalue_t object;
	
	prepareObject(context, &object);
	
	io_libecc_Context.setText(context, text);
	return callValue(context, io_libecc_Object.getMember(context, object.data.object, key), object, argumentCount, 0, textCall);
}

struct eccvalue_t deleteMember (struct eccstate_t * const context)
{
	const struct io_libecc_Text *text = opText(0);
	struct io_libecc_Key key = opValue().data.key;
	struct eccvalue_t object;
	int result;
	
	prepareObject(context, &object);
	
	result = io_libecc_Object.deleteMember(object.data.object, key);
	if (!result && context->strictMode)
	{
		io_libecc_Context.setText(context, text);
		io_libecc_Context.typeError(context, io_libecc_Chars.create("'%.*s' is non-configurable", io_libecc_Key.textOf(key)->length, io_libecc_Key.textOf(key)->bytes));
	}
	
	return io_libecc_Value.truth(result);
}

static
void prepareObjectProperty (struct eccstate_t * const context, struct eccvalue_t *object, struct eccvalue_t *property)
{
	const struct io_libecc_Text *textProperty;
	
	prepareObject(context, object);
	
	textProperty = opText(1);
	*property = nextOp();
	
	if (io_libecc_Value.isObject(*property))
	{
		io_libecc_Context.setText(context, textProperty);
		*property = io_libecc_Value.toPrimitive(context, *property, io_libecc_value_hintString);
	}
}

struct eccvalue_t getPropertyRef (struct eccstate_t * const context)
{
	const struct io_libecc_Text *text = opText(1);
	struct eccvalue_t object, property;
	struct eccvalue_t *ref;
	
	prepareObjectProperty(context, &object, &property);
	
	context->refObject = object.data.object;
	ref = io_libecc_Object.property(object.data.object, property, io_libecc_value_asOwn);
	
	if (!ref)
	{
		if (object.data.object->flags & io_libecc_object_sealed)
		{
			io_libecc_Context.setText(context, text);
			io_libecc_Context.typeError(context, io_libecc_Chars.create("object is not extensible"));
		}
		ref = io_libecc_Object.addProperty(object.data.object, property, io_libecc_value_undefined, 0);
	}
	
	return io_libecc_Value.reference(ref);
}

struct eccvalue_t getProperty (struct eccstate_t * const context)
{
	struct eccvalue_t object, property;
	
	prepareObjectProperty(context, &object, &property);
	
	return io_libecc_Object.getProperty(context, object.data.object, property);
}

struct eccvalue_t setProperty (struct eccstate_t * const context)
{
	const struct io_libecc_Text *text = opText(0);
	struct eccvalue_t object, property, value;
	
	prepareObjectProperty(context, &object, &property);
	
	value = retain(nextOp());
	value.flags = 0;
	
	io_libecc_Context.setText(context, text);
	io_libecc_Object.putProperty(context, object.data.object, property, value);
	
	return value;
}

struct eccvalue_t callProperty (struct eccstate_t * const context)
{
	const struct io_libecc_Text *textCall = opText(0);
	int32_t argumentCount = opValue().data.integer;
	const struct io_libecc_Text *text = &(++context->ops)->text;
	struct eccvalue_t object, property;
	
	prepareObjectProperty(context, &object, &property);
	
	io_libecc_Context.setText(context, text);
	return callValue(context, io_libecc_Object.getProperty(context, object.data.object, property), object, argumentCount, 0, textCall);
}

struct eccvalue_t deleteProperty (struct eccstate_t * const context)
{
	const struct io_libecc_Text *text = opText(0);
	struct eccvalue_t object, property;
	int result;
	
	prepareObjectProperty(context, &object, &property);
	
	result = io_libecc_Object.deleteProperty(object.data.object, property);
	if (!result && context->strictMode)
	{
		struct eccvalue_t string = io_libecc_Value.toString(context, property);
		io_libecc_Context.setText(context, text);
		io_libecc_Context.typeError(context, io_libecc_Chars.create("'%.*s' is non-configurable", io_libecc_Value.stringLength(&string), io_libecc_Value.stringBytes(&string)));
	}
	return io_libecc_Value.truth(result);
}

struct eccvalue_t pushEnvironment (struct eccstate_t * const context)
{
	if (context->refObject)
		context->refObject = io_libecc_Object.create(context->refObject);
	else
		context->environment = io_libecc_Object.create(context->environment);
	
	return opValue();
}

struct eccvalue_t popEnvironment (struct eccstate_t * const context)
{
	if (context->refObject)
	{
		struct eccobject_t *refObject = context->refObject;
		context->refObject = context->refObject->prototype;
		refObject->prototype = NULL;
	}
	else
	{
		struct eccobject_t *environment = context->environment;
		context->environment = context->environment->prototype;
		environment->prototype = NULL;
	}
	return opValue();
}

struct eccvalue_t exchange (struct eccstate_t * const context)
{
	struct eccvalue_t value = opValue();
	nextOp();
	return value;
}

struct eccvalue_t typeOf (struct eccstate_t * const context)
{
	struct eccvalue_t value = nextOp();
	if (value.type == io_libecc_value_referenceType)
	{
		struct eccvalue_t *ref = value.data.reference;
		if (!ref)
			return io_libecc_Value.text(&io_libecc_text_undefined);
		else
			value = *ref;
	}
	return io_libecc_Value.toType(value);
}

#define prepareAB \
	const struct io_libecc_Text *text = opText(1);\
	struct eccvalue_t a = nextOp();\
	const struct io_libecc_Text *textAlt = opText(1);\
	struct eccvalue_t b = nextOp();\

struct eccvalue_t equal (struct eccstate_t * const context)
{
	prepareAB
	
	if (a.type == io_libecc_value_binaryType && b.type == io_libecc_value_binaryType)
		return io_libecc_Value.truth(a.data.binary == b.data.binary);
	else
	{
		io_libecc_Context.setTexts(context, text, textAlt);
		return io_libecc_Value.equals(context, a, b);
	}
}

struct eccvalue_t notEqual (struct eccstate_t * const context)
{
	prepareAB
	
	if (a.type == io_libecc_value_binaryType && b.type == io_libecc_value_binaryType)
		return io_libecc_Value.truth(a.data.binary != b.data.binary);
	else
	{
		io_libecc_Context.setTexts(context, text, textAlt);
		return io_libecc_Value.truth(!io_libecc_Value.isTrue(io_libecc_Value.equals(context, a, b)));
	}
}

struct eccvalue_t identical (struct eccstate_t * const context)
{
	prepareAB
	
	if (a.type == io_libecc_value_binaryType && b.type == io_libecc_value_binaryType)
		return io_libecc_Value.truth(a.data.binary == b.data.binary);
	else
	{
		io_libecc_Context.setTexts(context, text, textAlt);
		return io_libecc_Value.same(context, a, b);
	}
}

struct eccvalue_t notIdentical (struct eccstate_t * const context)
{
	prepareAB
	
	if (a.type == io_libecc_value_binaryType && b.type == io_libecc_value_binaryType)
		return io_libecc_Value.truth(a.data.binary != b.data.binary);
	else
	{
		io_libecc_Context.setTexts(context, text, textAlt);
		return io_libecc_Value.truth(!io_libecc_Value.isTrue(io_libecc_Value.same(context, a, b)));
	}
}

struct eccvalue_t less (struct eccstate_t * const context)
{
	prepareAB
	
	if (a.type == io_libecc_value_binaryType && b.type == io_libecc_value_binaryType)
		return io_libecc_Value.truth(a.data.binary < b.data.binary);
	else
	{
		io_libecc_Context.setTexts(context, text, textAlt);
		return io_libecc_Value.less(context, a, b);
	}
}

struct eccvalue_t lessOrEqual (struct eccstate_t * const context)
{
	prepareAB
	
	if (a.type == io_libecc_value_binaryType && b.type == io_libecc_value_binaryType)
		return io_libecc_Value.truth(a.data.binary <= b.data.binary);
	else
	{
		io_libecc_Context.setTexts(context, text, textAlt);
		return io_libecc_Value.lessOrEqual(context, a, b);
	}
}

struct eccvalue_t more (struct eccstate_t * const context)
{
	prepareAB
	
	if (a.type == io_libecc_value_binaryType && b.type == io_libecc_value_binaryType)
		return io_libecc_Value.truth(a.data.binary > b.data.binary);
	else
	{
		io_libecc_Context.setTexts(context, text, textAlt);
		return io_libecc_Value.more(context, a, b);
	}
}

struct eccvalue_t moreOrEqual (struct eccstate_t * const context)
{
	prepareAB
	
	if (a.type == io_libecc_value_binaryType && b.type == io_libecc_value_binaryType)
		return io_libecc_Value.truth(a.data.binary >= b.data.binary);
	else
	{
		io_libecc_Context.setTexts(context, text, textAlt);
		return io_libecc_Value.moreOrEqual(context, a, b);
	}
}

struct eccvalue_t instanceOf (struct eccstate_t * const context)
{
	struct eccvalue_t a = nextOp();
	const struct io_libecc_Text *textAlt = opText(1);
	struct eccvalue_t b = nextOp();
	
	if (b.type != io_libecc_value_functionType)
	{
		io_libecc_Context.setText(context, textAlt);
		io_libecc_Context.typeError(context, io_libecc_Chars.create("'%.*s' is not a function", textAlt->length, textAlt->bytes));
	}
	
	b = io_libecc_Object.getMember(context, b.data.object, io_libecc_key_prototype);
	if (!io_libecc_Value.isObject(b))
	{
		io_libecc_Context.setText(context, textAlt);
		io_libecc_Context.typeError(context, io_libecc_Chars.create("'%.*s'.prototype not an object", textAlt->length, textAlt->bytes));
	}
	
	if (io_libecc_Value.isObject(a))
	{
		struct eccobject_t *object;
		
		object = a.data.object;
		while (( object = object->prototype ))
			if (object == b.data.object)
				return io_libecc_value_true;
	}
	return io_libecc_value_false;
}

struct eccvalue_t in (struct eccstate_t * const context)
{
	struct eccvalue_t property = nextOp();
	struct eccvalue_t object = nextOp();
	struct eccvalue_t *ref;
	
	if (!io_libecc_Value.isObject(object))
		io_libecc_Context.typeError(context, io_libecc_Chars.create("'%.*s' not an object", context->ops->text.length, context->ops->text.bytes));
	
	ref = io_libecc_Object.property(object.data.object, io_libecc_Value.toString(context, property), 0);
	
	return io_libecc_Value.truth(ref != NULL);
}

struct eccvalue_t add (struct eccstate_t * const context)
{
	prepareAB
	
	if (a.type == io_libecc_value_binaryType && b.type == io_libecc_value_binaryType)
	{
		a.data.binary += b.data.binary;
		return a;
	}
	else
	{
		io_libecc_Context.setTexts(context, text, textAlt);
		return io_libecc_Value.add(context, a, b);
	}
}

struct eccvalue_t minus (struct eccstate_t * const context)
{
	struct eccvalue_t a = nextOp();
	struct eccvalue_t b = nextOp();
	if (a.type == io_libecc_value_binaryType && b.type == io_libecc_value_binaryType)
	{
		a.data.binary -= b.data.binary;
		return a;
	}
	else
		return io_libecc_Value.binary(io_libecc_Value.toBinary(context, a).data.binary - io_libecc_Value.toBinary(context, b).data.binary);
}

struct eccvalue_t multiply (struct eccstate_t * const context)
{
	struct eccvalue_t a = nextOp();
	struct eccvalue_t b = nextOp();
	if (a.type == io_libecc_value_binaryType && b.type == io_libecc_value_binaryType)
	{
		a.data.binary *= b.data.binary;
		return a;
	}
	else
		return io_libecc_Value.binary(io_libecc_Value.toBinary(context, a).data.binary * io_libecc_Value.toBinary(context, b).data.binary);
}

struct eccvalue_t divide (struct eccstate_t * const context)
{
	struct eccvalue_t a = nextOp();
	struct eccvalue_t b = nextOp();
	if (a.type == io_libecc_value_binaryType && b.type == io_libecc_value_binaryType)
	{
		a.data.binary /= b.data.binary;
		return a;
	}
	else
		return io_libecc_Value.binary(io_libecc_Value.toBinary(context, a).data.binary / io_libecc_Value.toBinary(context, b).data.binary);
}

struct eccvalue_t modulo (struct eccstate_t * const context)
{
	struct eccvalue_t a = nextOp();
	struct eccvalue_t b = nextOp();
	if (a.type == io_libecc_value_binaryType && b.type == io_libecc_value_binaryType)
	{
		a.data.binary = fmod(a.data.binary, b.data.binary);
		return a;
	}
	else
		return io_libecc_Value.binary(fmod(io_libecc_Value.toBinary(context, a).data.binary, io_libecc_Value.toBinary(context, b).data.binary));
}

struct eccvalue_t leftShift (struct eccstate_t * const context)
{
	struct eccvalue_t a = nextOp();
	struct eccvalue_t b = nextOp();
	return io_libecc_Value.binary(io_libecc_Value.toInteger(context, a).data.integer << (uint32_t)io_libecc_Value.toInteger(context, b).data.integer);
}

struct eccvalue_t rightShift (struct eccstate_t * const context)
{
	struct eccvalue_t a = nextOp();
	struct eccvalue_t b = nextOp();
	return io_libecc_Value.binary(io_libecc_Value.toInteger(context, a).data.integer >> (uint32_t)io_libecc_Value.toInteger(context, b).data.integer);
}

struct eccvalue_t unsignedRightShift (struct eccstate_t * const context)
{
	struct eccvalue_t a = nextOp();
	struct eccvalue_t b = nextOp();
	return io_libecc_Value.binary((uint32_t)io_libecc_Value.toInteger(context, a).data.integer >> (uint32_t)io_libecc_Value.toInteger(context, b).data.integer);
}

struct eccvalue_t bitwiseAnd (struct eccstate_t * const context)
{
	struct eccvalue_t a = nextOp();
	struct eccvalue_t b = nextOp();
	return io_libecc_Value.binary(io_libecc_Value.toInteger(context, a).data.integer & io_libecc_Value.toInteger(context, b).data.integer);
}

struct eccvalue_t bitwiseXor (struct eccstate_t * const context)
{
	struct eccvalue_t a = nextOp();
	struct eccvalue_t b = nextOp();
	return io_libecc_Value.binary(io_libecc_Value.toInteger(context, a).data.integer ^ io_libecc_Value.toInteger(context, b).data.integer);
}

struct eccvalue_t bitwiseOr (struct eccstate_t * const context)
{
	struct eccvalue_t a = nextOp();
	struct eccvalue_t b = nextOp();
	return io_libecc_Value.binary(io_libecc_Value.toInteger(context, a).data.integer | io_libecc_Value.toInteger(context, b).data.integer);
}

struct eccvalue_t logicalAnd (struct eccstate_t * const context)
{
	const int32_t opCount = opValue().data.integer;
	struct eccvalue_t value = nextOp();
	
	if (!io_libecc_Value.isTrue(value))
	{
		context->ops += opCount;
		return value;
	}
	else
		return nextOp();
}

struct eccvalue_t logicalOr (struct eccstate_t * const context)
{
	const int32_t opCount = opValue().data.integer;
	struct eccvalue_t value = nextOp();
	
	if (io_libecc_Value.isTrue(value))
	{
		context->ops += opCount;
		return value;
	}
	else
		return nextOp();
}

struct eccvalue_t positive (struct eccstate_t * const context)
{
	struct eccvalue_t a = nextOp();
	if (a.type == io_libecc_value_binaryType)
		return a;
	else
		return io_libecc_Value.toBinary(context, a);
}

struct eccvalue_t negative (struct eccstate_t * const context)
{
	struct eccvalue_t a = nextOp();
	if (a.type == io_libecc_value_binaryType)
		return io_libecc_Value.binary(-a.data.binary);
	else
		return io_libecc_Value.binary(-io_libecc_Value.toBinary(context, a).data.binary);
}

struct eccvalue_t invert (struct eccstate_t * const context)
{
	struct eccvalue_t a = nextOp();
	return io_libecc_Value.binary(~io_libecc_Value.toInteger(context, a).data.integer);
}

struct eccvalue_t not (struct eccstate_t * const context)
{
	struct eccvalue_t a = nextOp();
	return io_libecc_Value.truth(!io_libecc_Value.isTrue(a));
}

// MARK: assignement

#define unaryBinaryOpRef(OP) \
	struct eccobject_t *refObject = context->refObject; \
	const struct io_libecc_Text *text = opText(0); \
	struct eccvalue_t *ref = nextOp().data.reference; \
	struct eccvalue_t a; \
	double result; \
	 \
	a = *ref; \
	if (a.flags & (io_libecc_value_readonly | io_libecc_value_accessor)) \
	{ \
		io_libecc_Context.setText(context, text); \
		a = io_libecc_Value.toBinary(context, release(io_libecc_Object.getValue(context, context->refObject, ref))); \
		result = OP; \
		io_libecc_Object.putValue(context, context->refObject, ref, a); \
		return io_libecc_Value.binary(result); \
	} \
	else if (a.type != io_libecc_value_binaryType) \
		a = io_libecc_Value.toBinary(context, release(a)); \
	 \
	result = OP; \
	replaceRefValue(ref, a); \
	context->refObject = refObject; \
	return io_libecc_Value.binary(result); \

struct eccvalue_t incrementRef (struct eccstate_t * const context)
{
	unaryBinaryOpRef(++a.data.binary)
}

struct eccvalue_t decrementRef (struct eccstate_t * const context)
{
	unaryBinaryOpRef(--a.data.binary)
}

struct eccvalue_t postIncrementRef (struct eccstate_t * const context)
{
	unaryBinaryOpRef(a.data.binary++)
}

struct eccvalue_t postDecrementRef (struct eccstate_t * const context)
{
	unaryBinaryOpRef(a.data.binary--)
}

#define assignOpRef(OP, TYPE, CONV) \
	struct eccobject_t *refObject = context->refObject; \
	const struct io_libecc_Text *text = opText(0); \
	struct eccvalue_t *ref = nextOp().data.reference; \
	struct eccvalue_t a, b = nextOp(); \
	 \
	if (b.type != TYPE) \
		b = CONV(context, b); \
	 \
	a = *ref; \
	if (a.flags & (io_libecc_value_readonly | io_libecc_value_accessor)) \
	{ \
		io_libecc_Context.setText(context, text); \
		a = CONV(context, io_libecc_Object.getValue(context, context->refObject, ref)); \
		OP; \
		return io_libecc_Object.putValue(context, context->refObject, ref, a); \
	} \
	else if (a.type != TYPE) \
		a = CONV(context, release(a)); \
	\
	OP; \
	replaceRefValue(ref, a); \
	context->refObject = refObject; \
	return a; \

#define assignBinaryOpRef(OP) assignOpRef(OP, io_libecc_value_binaryType, io_libecc_Value.toBinary)
#define assignIntegerOpRef(OP) assignOpRef(OP, io_libecc_value_integerType, io_libecc_Value.toInteger)

struct eccvalue_t addAssignRef (struct eccstate_t * const context)
{
	struct eccobject_t *refObject = context->refObject;
	const struct io_libecc_Text *text = opText(1);
	struct eccvalue_t *ref = nextOp().data.reference;
	const struct io_libecc_Text *textAlt = opText(1);
	struct eccvalue_t a, b = nextOp();
	
	io_libecc_Context.setTexts(context, text, textAlt);

	a = *ref;
	if (a.flags & (io_libecc_value_readonly | io_libecc_value_accessor))
	{
		a = io_libecc_Object.getValue(context, context->refObject, ref);
		a = retain(io_libecc_Value.add(context, a, b));
		return io_libecc_Object.putValue(context, context->refObject, ref, a);
	}
	
	if (a.type == io_libecc_value_binaryType && b.type == io_libecc_value_binaryType)
	{
		a.data.binary += b.data.binary;
		return *ref = a;
	}
	
	a = retain(io_libecc_Value.add(context, release(a), b));
	replaceRefValue(ref, a);
	context->refObject = refObject;
	return a;
}

struct eccvalue_t minusAssignRef (struct eccstate_t * const context)
{
	assignBinaryOpRef(a.data.binary -= b.data.binary);
}

struct eccvalue_t multiplyAssignRef (struct eccstate_t * const context)
{
	assignBinaryOpRef(a.data.binary *= b.data.binary);
}

struct eccvalue_t divideAssignRef (struct eccstate_t * const context)
{
	assignBinaryOpRef(a.data.binary /= b.data.binary);
}

struct eccvalue_t moduloAssignRef (struct eccstate_t * const context)
{
	assignBinaryOpRef(a.data.binary = fmod(a.data.binary, b.data.binary));
}

struct eccvalue_t leftShiftAssignRef (struct eccstate_t * const context)
{
	assignIntegerOpRef(a.data.integer <<= (uint32_t)b.data.integer);
}

struct eccvalue_t rightShiftAssignRef (struct eccstate_t * const context)
{
	assignIntegerOpRef(a.data.integer >>= (uint32_t)b.data.integer);
}

struct eccvalue_t unsignedRightShiftAssignRef (struct eccstate_t * const context)
{
	assignIntegerOpRef(a.data.integer = (uint32_t)a.data.integer >> (uint32_t)b.data.integer);
}

struct eccvalue_t bitAndAssignRef (struct eccstate_t * const context)
{
	assignIntegerOpRef(a.data.integer &= (uint32_t)b.data.integer);
}

struct eccvalue_t bitXorAssignRef (struct eccstate_t * const context)
{
	assignIntegerOpRef(a.data.integer ^= (uint32_t)b.data.integer);
}

struct eccvalue_t bitOrAssignRef (struct eccstate_t * const context)
{
	assignIntegerOpRef(a.data.integer |= (uint32_t)b.data.integer);
}


// MARK: Statement

struct eccvalue_t debugger (struct eccstate_t * const context)
{
	#if DEBUG
	debug = 1;
	#endif
	return trapOp(context, 0);
}

io_libecc_ecc_useframe
struct eccvalue_t try (struct eccstate_t * const context)
{
	struct eccobject_t *environment = context->environment;
	struct eccobject_t *refObject = context->refObject;
	const struct io_libecc_Op *end = context->ops + opValue().data.integer;
	struct io_libecc_Key key;
	
	const struct io_libecc_Op * volatile rethrowOps = NULL;
	volatile int rethrow = 0, breaker = 0;
	volatile struct eccvalue_t value = io_libecc_value_undefined;
	struct eccvalue_t finallyValue;
	uint32_t indices[3];
	
	io_libecc_Pool.getIndices(indices);
	
	if (!setjmp(*io_libecc_Ecc.pushEnv(context->ecc))) // try
		value = nextOp();
	else
	{
		value = context->ecc->result;
		context->ecc->result = io_libecc_value_undefined;
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
				if (value.type == io_libecc_value_functionType)
				{
					value.data.function->flags |= io_libecc_function_useBoundThis;
					value.data.function->boundThis = io_libecc_Value.object(context->environment);
				}
				io_libecc_Object.addMember(context->environment, key, value, io_libecc_value_sealed);
				value = nextOp(); // execute until noop
				rethrow = 0;
				if (context->breaker)
					popEnvironment(context);
			}
		}
		else // rethrow
			popEnvironment(context);
	}
	
	io_libecc_Ecc.popEnv(context->ecc);
	
	breaker = context->breaker;
	context->breaker = 0;
	context->ops = end; // op[end] = io_libecc_Op.jump, to after catch
	finallyValue = nextOp(); // jump to after catch, and execute until noop
	
	if (context->breaker) /* return breaker */
		return finallyValue;
	else if (rethrow)
	{
		context->ops = rethrowOps;
		io_libecc_Context.throw(context, retain(value));
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
struct eccvalue_t throw (struct eccstate_t * const context)
{
	context->ecc->text = *opText(1);
	io_libecc_Context.throw(context, retain(trapOp(context, 0)));
}

struct eccvalue_t with (struct eccstate_t * const context)
{
	struct eccobject_t *environment = context->environment;
	struct eccobject_t *refObject = context->refObject;
	struct eccobject_t *object = io_libecc_Value.toObject(context, nextOp()).data.object;
	struct eccvalue_t value;
	
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

struct eccvalue_t next (struct eccstate_t * const context)
{
	return nextOp();
}

struct eccvalue_t nextIf (struct eccstate_t * const context)
{
	struct eccvalue_t value = opValue();
	
	if (!io_libecc_Value.isTrue(trapOp(context, 1)))
		return value;
	
	return nextOp();
}

struct eccvalue_t autoreleaseExpression (struct eccstate_t * const context)
{
	uint32_t indices[3];
	
	io_libecc_Pool.getIndices(indices);
	release(context->ecc->result);
	context->ecc->result = retain(trapOp(context, 1));
	io_libecc_Pool.collectUnreferencedFromIndices(indices);
	return nextOp();
}

struct eccvalue_t autoreleaseDiscard (struct eccstate_t * const context)
{
	uint32_t indices[3];
	
	io_libecc_Pool.getIndices(indices);
	trapOp(context, 1);
	io_libecc_Pool.collectUnreferencedFromIndices(indices);
	return nextOp();
}

struct eccvalue_t expression (struct eccstate_t * const context)
{
	release(context->ecc->result);
	context->ecc->result = retain(trapOp(context, 1));
	return nextOp();
}

struct eccvalue_t discard (struct eccstate_t * const context)
{
	trapOp(context, 1);
	return nextOp();
}

struct eccvalue_t discardN (struct eccstate_t * const context)
{
	switch (opValue().data.integer)
	{
		default:
			io_libecc_Ecc.fatal("Invalid discardN : %d", opValue().data.integer);
		
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

struct eccvalue_t jump (struct eccstate_t * const context)
{
	int32_t offset = opValue().data.integer;
	context->ops += offset;
	return nextOp();
}

struct eccvalue_t jumpIf (struct eccstate_t * const context)
{
	int32_t offset = opValue().data.integer;
	struct eccvalue_t value;
	
	value = trapOp(context, 1);
	if (io_libecc_Value.isTrue(value))
		context->ops += offset;
	
	return nextOp();
}

struct eccvalue_t jumpIfNot (struct eccstate_t * const context)
{
	int32_t offset = opValue().data.integer;
	struct eccvalue_t value;
	
	value = trapOp(context, 1);
	if (!io_libecc_Value.isTrue(value))
		context->ops += offset;
	
	return nextOp();
}

struct eccvalue_t result (struct eccstate_t * const context)
{
	struct eccvalue_t result = trapOp(context, 0);
	context->breaker = -1;
	return result;
}

struct eccvalue_t repopulate (struct eccstate_t * const context)
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
				hashmap[index].value = io_libecc_value_none;
			}
		}
		else
			for (; index < arguments; ++index)
				nextOp();
		
		if (context->environment->hashmap[2].value.type == io_libecc_value_objectType) {
			struct eccobject_t *arguments = context->environment->hashmap[2].value.data.object;
			
			for (index = 3; index < context->environment->hashmapCapacity; ++index)
				arguments->element[index - 3].value = hashmap[index].value;
		}
		
		memcpy(context->environment->hashmap, hashmap, sizeof(hashmap));
	}
	
	context->ops = nextOps;
	return nextOp();
}

struct eccvalue_t resultVoid (struct eccstate_t * const context)
{
	struct eccvalue_t result = io_libecc_value_undefined;
	context->breaker = -1;
	return result;
}

struct eccvalue_t switchOp (struct eccstate_t * const context)
{
	int32_t offset = opValue().data.integer;
	const struct io_libecc_Op *nextOps = context->ops + offset;
	struct eccvalue_t value, caseValue;
	const struct io_libecc_Text *text = opText(1);
	
	value = trapOp(context, 1);
	
	while (context->ops < nextOps)
	{
		const struct io_libecc_Text *textAlt = opText(1);
		caseValue = nextOp();
		
		io_libecc_Context.setTexts(context, text, textAlt);
		if (io_libecc_Value.isTrue(io_libecc_Value.same(context, value, caseValue)))
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

struct eccvalue_t breaker (struct eccstate_t * const context)
{
	context->breaker = opValue().data.integer;
	return io_libecc_value_undefined;
}

struct eccvalue_t iterate (struct eccstate_t * const context)
{
	const struct io_libecc_Op *startOps = context->ops;
	const struct io_libecc_Op *endOps = startOps;
	const struct io_libecc_Op *nextOps = startOps + 1;
	struct eccvalue_t value;
	int32_t skipOp = opValue().data.integer;
	
	context->ops = nextOps + skipOp;
	
	while (io_libecc_Value.isTrue(nextOp()))
		stepIteration(value, nextOps, break);
	
	context->ops = endOps;
	
	return nextOp();
}

static
struct eccvalue_t iterateIntegerRef (
	struct eccstate_t * const context,
	int (*compareInteger) (int32_t, int32_t),
	int (*wontOverflow) (int32_t, int32_t),
	struct eccvalue_t (*compareValue) (struct eccstate_t * const, struct eccvalue_t, struct eccvalue_t),
	struct eccvalue_t (*valueStep) (struct eccstate_t * const, struct eccvalue_t, struct eccvalue_t))
{
	struct eccobject_t *refObject = context->refObject;
	const struct io_libecc_Op *endOps = context->ops + opValue().data.integer;
	struct eccvalue_t stepValue = nextOp();
	struct eccvalue_t *indexRef = nextOp().data.reference;
	struct eccvalue_t *countRef = nextOp().data.reference;
	const struct io_libecc_Op *nextOps = context->ops;
	struct eccvalue_t value;
	
	if (indexRef->type == io_libecc_value_binaryType && indexRef->data.binary >= INT32_MIN && indexRef->data.binary <= INT32_MAX)
	{
		struct eccvalue_t integerValue = io_libecc_Value.toInteger(context, *indexRef);
		if (indexRef->data.binary == integerValue.data.integer)
			*indexRef = integerValue;
	}
	
	if (countRef->type == io_libecc_value_binaryType && countRef->data.binary >= INT32_MIN && countRef->data.binary <= INT32_MAX)
	{
		struct eccvalue_t integerValue = io_libecc_Value.toInteger(context, *countRef);
		if (countRef->data.binary == integerValue.data.integer)
			*countRef = integerValue;
	}
	
	if (indexRef->type == io_libecc_value_integerType && countRef->type == io_libecc_value_integerType)
	{
		int32_t step = valueStep == io_libecc_Value.add? stepValue.data.integer: -stepValue.data.integer;
		if (!wontOverflow(countRef->data.integer, step - 1))
			goto deoptimize;
		
		for (; compareInteger(indexRef->data.integer, countRef->data.integer); indexRef->data.integer += step)
			stepIteration(value, nextOps, goto done);
	}
	
deoptimize:
	for (; io_libecc_Value.isTrue(compareValue(context, *indexRef, *countRef))
		 ; *indexRef = valueStep(context, *indexRef, stepValue)
		 )
		stepIteration(value, nextOps, break);
	
done:
	context->refObject = refObject;
	context->ops = endOps;
	return nextOp();
}

struct eccvalue_t iterateLessRef (struct eccstate_t * const context)
{
	return iterateIntegerRef(context, integerLess, integerWontOverflowPositive, io_libecc_Value.less, io_libecc_Value.add);
}

struct eccvalue_t iterateLessOrEqualRef (struct eccstate_t * const context)
{
	return iterateIntegerRef(context, integerLessOrEqual, integerWontOverflowPositive, io_libecc_Value.lessOrEqual, io_libecc_Value.add);
}

struct eccvalue_t iterateMoreRef (struct eccstate_t * const context)
{
	return iterateIntegerRef(context, integerMore, integerWontOverflowNegative, io_libecc_Value.more, io_libecc_Value.subtract);
}

struct eccvalue_t iterateMoreOrEqualRef (struct eccstate_t * const context)
{
	return iterateIntegerRef(context, integerMoreOrEqual, integerWontOverflowNegative, io_libecc_Value.moreOrEqual, io_libecc_Value.subtract);
}

struct eccvalue_t iterateInRef (struct eccstate_t * const context)
{
	struct eccobject_t *refObject = context->refObject;
	struct eccvalue_t *ref = nextOp().data.reference;
	struct eccvalue_t target = nextOp();
	struct eccvalue_t value = nextOp(), key;
	struct io_libecc_chars_Append chars;
	struct eccobject_t *object;
	const struct io_libecc_Op *startOps = context->ops;
	const struct io_libecc_Op *endOps = startOps + value.data.integer;
	uint32_t index, count;
	
	if (io_libecc_Value.isObject(target))
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
				
				if (object != target.data.object && &element->value != io_libecc_Object.element(target.data.object, index, 0))
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
				
				if (object != target.data.object && &hashmap->value != io_libecc_Object.member(target.data.object, hashmap->value.key, 0))
					continue;
				
				key = io_libecc_Value.text(io_libecc_Key.textOf(hashmap->value.key));
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
