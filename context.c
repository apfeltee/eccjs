//
//  context.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//

#define Implementation
#include "context.h"

#include "ecc.h"
#include "op.h"

// MARK: - Private

// MARK: - Static Members

// MARK: - Methods

static void rangeError(struct io_libecc_Context* const, struct io_libecc_Chars*) __attribute__((noreturn));
static void referenceError(struct io_libecc_Context* const, struct io_libecc_Chars*) __attribute__((noreturn));
static void syntaxError(struct io_libecc_Context* const, struct io_libecc_Chars*) __attribute__((noreturn));
static void typeError(struct io_libecc_Context* const, struct io_libecc_Chars*) __attribute__((noreturn));
static void uriError(struct io_libecc_Context* const, struct io_libecc_Chars*) __attribute__((noreturn));
static void throw(struct io_libecc_Context* const, struct io_libecc_Value) __attribute__((noreturn));
static struct io_libecc_Value callFunction(struct io_libecc_Context* const, struct io_libecc_Function* function, struct io_libecc_Value this, int argumentCount, ...);
static int argumentCount(struct io_libecc_Context* const);
static struct io_libecc_Value argument(struct io_libecc_Context* const, int argumentIndex);
static void replaceArgument(struct io_libecc_Context* const, int argumentIndex, struct io_libecc_Value value);
static struct io_libecc_Value this(struct io_libecc_Context* const);
static void assertThisType(struct io_libecc_Context* const, enum io_libecc_value_Type);
static void assertThisMask(struct io_libecc_Context* const, enum io_libecc_value_Mask);
static void assertThisCoerciblePrimitive(struct io_libecc_Context* const);
static void setText(struct io_libecc_Context* const, const struct io_libecc_Text* text);
static void setTexts(struct io_libecc_Context* const, const struct io_libecc_Text* text, const struct io_libecc_Text* textAlt);
static void setTextIndex(struct io_libecc_Context* const, enum io_libecc_context_Index index);
static void setTextIndexArgument(struct io_libecc_Context* const, int argument);
static struct io_libecc_Text textSeek(struct io_libecc_Context* const);
static void rewindStatement(struct io_libecc_Context* const);
static void printBacktrace(struct io_libecc_Context* const context);
static struct io_libecc_Object* environmentRoot(struct io_libecc_Context* const context);
const struct type_io_libecc_Context io_libecc_Context = {
    rangeError,     referenceError,
    syntaxError,    typeError,
    uriError,       throw,
    callFunction,   argumentCount,
    argument,       replaceArgument,
    this,           assertThisType,
    assertThisMask, assertThisCoerciblePrimitive,
    setText,        setTexts,
    setTextIndex,   setTextIndexArgument,
    textSeek,       rewindStatement,
    printBacktrace, environmentRoot,
};

void rangeError (struct io_libecc_Context * const self, struct io_libecc_Chars *chars)
{
	throw(self, io_libecc_Value.error(io_libecc_Error.rangeError(textSeek(self), chars)));
}

void referenceError (struct io_libecc_Context * const self, struct io_libecc_Chars *chars)
{
	throw(self, io_libecc_Value.error(io_libecc_Error.referenceError(textSeek(self), chars)));
}

void syntaxError (struct io_libecc_Context * const self, struct io_libecc_Chars *chars)
{
	throw(self, io_libecc_Value.error(io_libecc_Error.syntaxError(textSeek(self), chars)));
}

void typeError (struct io_libecc_Context * const self, struct io_libecc_Chars *chars)
{
	throw(self, io_libecc_Value.error(io_libecc_Error.typeError(textSeek(self), chars)));
}

void uriError (struct io_libecc_Context * const self, struct io_libecc_Chars *chars)
{
	throw(self, io_libecc_Value.error(io_libecc_Error.uriError(textSeek(self), chars)));
}

void throw (struct io_libecc_Context * const self, struct io_libecc_Value value)
{
	if (value.type == io_libecc_value_errorType)
		self->ecc->text = value.data.error->text;
	
	if (self->ecc->printLastThrow && self->ecc->envCount == 1)
	{
		struct io_libecc_Value name, message;
		name = io_libecc_value_undefined;
		
		if (value.type == io_libecc_value_errorType)
		{
			name = io_libecc_Value.toString(self, io_libecc_Object.getMember(self, value.data.object, io_libecc_key_name));
			message = io_libecc_Value.toString(self, io_libecc_Object.getMember(self, value.data.object, io_libecc_key_message));
		}
		else
			message = io_libecc_Value.toString(self, value);
		
		if (name.type == io_libecc_value_undefinedType)
			name = io_libecc_Value.text(&io_libecc_text_errorName);
		
		io_libecc_Env.newline();
		io_libecc_Env.printError(io_libecc_Value.stringLength(&name), io_libecc_Value.stringBytes(&name), "%.*s" , io_libecc_Value.stringLength(&message), io_libecc_Value.stringBytes(&message));
		printBacktrace(self);
		io_libecc_Ecc.printTextInput(self->ecc, self->ecc->text, 1);
	}
	
	io_libecc_Ecc.jmpEnv(self->ecc, value);
}

struct io_libecc_Value callFunction (struct io_libecc_Context * const self, struct io_libecc_Function *function, struct io_libecc_Value this, int argumentCount, ... )
{
	struct io_libecc_Value result;
	va_list ap;
	int offset = 0;
	
	if (argumentCount & io_libecc_context_asAccessor) {
		offset = io_libecc_context_accessorOffset;
	}
	
	va_start(ap, argumentCount);
	result = io_libecc_Op.callFunctionVA(self, offset, function, this, argumentCount & io_libecc_context_countMask, ap);
	va_end(ap);
	
	return result;
}

int argumentCount (struct io_libecc_Context * const self)
{
	if (self->environment->hashmap[2].value.type == io_libecc_value_objectType)
		return self->environment->hashmap[2].value.data.object->elementCount;
	else
		return self->environment->hashmapCount - 3;
}

struct io_libecc_Value argument (struct io_libecc_Context * const self, int argumentIndex)
{
	self->textIndex = argumentIndex + 4;
	
	if (self->environment->hashmap[2].value.type == io_libecc_value_objectType)
	{
		if (argumentIndex < self->environment->hashmap[2].value.data.object->elementCount)
			return self->environment->hashmap[2].value.data.object->element[argumentIndex].value;
	}
	else if (argumentIndex < self->environment->hashmapCount - 3)
		return self->environment->hashmap[argumentIndex + 3].value;
	
	return io_libecc_value_none;
}

void replaceArgument (struct io_libecc_Context * const self, int argumentIndex, struct io_libecc_Value value)
{
	if (self->environment->hashmap[2].value.type == io_libecc_value_objectType)
	{
		if (argumentIndex < self->environment->hashmap[2].value.data.object->elementCount)
			self->environment->hashmap[2].value.data.object->element[argumentIndex].value = value;
	}
	else if (argumentIndex < self->environment->hashmapCount - 3)
		self->environment->hashmap[argumentIndex + 3].value = value;
}

struct io_libecc_Value this (struct io_libecc_Context * const self)
{
	self->textIndex = io_libecc_context_thisIndex;
	return self->this;
}

void assertThisType (struct io_libecc_Context * const self, enum io_libecc_value_Type type)
{
	if (self->this.type != type)
	{
		setTextIndex(self, io_libecc_context_thisIndex);
		typeError(self, io_libecc_Chars.create("'this' is not a %s", io_libecc_Value.typeName(type)));
	}
}

void assertThisMask (struct io_libecc_Context * const self, enum io_libecc_value_Mask mask)
{
	if (!(self->this.type & mask))
	{
		setTextIndex(self, io_libecc_context_thisIndex);
		typeError(self, io_libecc_Chars.create("'this' is not a %s", io_libecc_Value.maskName(mask)));
	}
}

void assertThisCoerciblePrimitive (struct io_libecc_Context * const self)
{
	if (self->this.type == io_libecc_value_undefinedType || self->this.type == io_libecc_value_nullType)
	{
		setTextIndex(self, io_libecc_context_thisIndex);
		typeError(self, io_libecc_Chars.create("'this' cannot be null or undefined"));
	}
}

void setText (struct io_libecc_Context * const self, const struct io_libecc_Text *text)
{
	self->textIndex = io_libecc_context_savedIndex;
	self->text = text;
}

void setTexts (struct io_libecc_Context * const self, const struct io_libecc_Text *text, const struct io_libecc_Text *textAlt)
{
	self->textIndex = io_libecc_context_savedIndex;
	self->text = text;
	self->textAlt = textAlt;
}

void setTextIndex (struct io_libecc_Context * const self, enum io_libecc_context_Index index)
{
	self->textIndex = index;
}

void setTextIndexArgument (struct io_libecc_Context * const self, int argument)
{
	self->textIndex = argument + 4;
}

struct io_libecc_Text textSeek (struct io_libecc_Context * const self)
{
	const char *bytes;
	struct io_libecc_Context seek = *self;
	uint32_t breakArray = 0, argumentCount = 0;
	struct io_libecc_Text callText;
	enum io_libecc_context_Index index;
	int isAccessor = 0;
	
	assert(self);
	assert(self->ecc);
	
	index = self->textIndex;
	
	if (index == io_libecc_context_savedIndex)
		return *self->text;
	
	if (index == io_libecc_context_savedIndexAlt)
		return *self->textAlt;
	
	while (seek.ops->text.bytes == io_libecc_text_nativeCode.bytes)
	{
		if (!seek.parent)
			return seek.ops->text;
		
		isAccessor = seek.argumentOffset == io_libecc_context_accessorOffset;
		
		if (seek.argumentOffset > 0 && index >= io_libecc_context_thisIndex)
		{
			++index;
			++argumentCount;
			breakArray <<= 1;
			
			if (seek.argumentOffset == io_libecc_context_applyOffset)
				breakArray |= 2;
		}
		seek = *seek.parent;
	}
	
	if (seek.ops->native == io_libecc_Op.noop)
		--seek.ops;
	
	if (isAccessor)
	{
		if (index > io_libecc_context_thisIndex)
			io_libecc_Context.rewindStatement(&seek);
	}
	else if (index > io_libecc_context_noIndex)
	{
		while (seek.ops->text.bytes != seek.textCall->bytes
			|| seek.ops->text.length != seek.textCall->length
			)
			--seek.ops;
		
		argumentCount += seek.ops->value.data.integer;
		callText = seek.ops->text;
		
		// func
		if (index-- > io_libecc_context_callIndex)
			++seek.ops;
		
		// this
		if (index-- > io_libecc_context_callIndex && (seek.ops + 1)->text.bytes <= seek.ops->text.bytes)
			++seek.ops;
		
		// arguments
		while (index-- > io_libecc_context_callIndex)
		{
			if (!argumentCount--)
				return io_libecc_Text.make(callText.bytes + callText.length - 1, 0);
			
			bytes = seek.ops->text.bytes + seek.ops->text.length;
			while (bytes > seek.ops->text.bytes && seek.ops->text.bytes)
				++seek.ops;
			
			if (breakArray & 0x1 && seek.ops->native == io_libecc_Op.array)
				++seek.ops;
			
			breakArray >>= 1;
		}
	}
	
	return seek.ops->text;
}

void rewindStatement(struct io_libecc_Context * const context)
{
	while (!(context->ops->text.flags & io_libecc_text_breakFlag))
		--context->ops;
}

void printBacktrace (struct io_libecc_Context * const context)
{
	int depth = context->depth, count, skip;
	struct io_libecc_Context frame;
	
	if (depth > 12)
	{
		io_libecc_Env.printColor(0, io_libecc_env_bold, "...");
		fprintf(stderr, " (%d more)\n", depth - 12);
		depth = 12;
	}
	
	while (depth > 0)
	{
		count = depth--;
		frame = *context;
		skip = 0;
		
		while (count--)
		{
			--skip;
			
			if (frame.argumentOffset == io_libecc_context_callOffset || frame.argumentOffset == io_libecc_context_applyOffset)
				skip = 2;
			else if (frame.textIndex > io_libecc_context_noIndex && frame.ops->text.bytes == io_libecc_text_nativeCode.bytes)
				skip = 1;
			
			frame = *frame.parent;
		}
		
		if (skip <= 0 && frame.ops->text.bytes != io_libecc_text_nativeCode.bytes)
		{
			io_libecc_Context.rewindStatement(&frame);
			if (frame.ops->text.length)
				io_libecc_Ecc.printTextInput(frame.ecc, frame.ops->text, 0);
		}
	}
}

struct io_libecc_Object *environmentRoot (struct io_libecc_Context * const context)
{
	struct io_libecc_Object *environment = context->strictMode? context->environment: &context->ecc->global->environment;
	
	if (context->strictMode)
		while (environment->prototype && environment->prototype != &context->ecc->global->environment)
			environment = environment->prototype;
	
	return environment;
}
