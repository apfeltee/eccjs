//
//  error.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"

// MARK: - Private

struct eccobject_t * io_libecc_error_prototype = NULL;
struct eccobject_t * io_libecc_error_rangePrototype = NULL;
struct eccobject_t * io_libecc_error_referencePrototype = NULL;
struct eccobject_t * io_libecc_error_syntaxPrototype = NULL;
struct eccobject_t * io_libecc_error_typePrototype = NULL;
struct eccobject_t * io_libecc_error_uriPrototype = NULL;
struct eccobject_t * io_libecc_error_evalPrototype = NULL;

struct io_libecc_Function * io_libecc_error_constructor = NULL;
struct io_libecc_Function * io_libecc_error_rangeConstructor = NULL;
struct io_libecc_Function * io_libecc_error_referenceConstructor = NULL;
struct io_libecc_Function * io_libecc_error_syntaxConstructor = NULL;
struct io_libecc_Function * io_libecc_error_typeConstructor = NULL;
struct io_libecc_Function * io_libecc_error_uriConstructor = NULL;
struct io_libecc_Function * io_libecc_error_evalConstructor = NULL;

const struct io_libecc_object_Type io_libecc_error_type = {
	.text = &io_libecc_text_errorType,
};

// MARK: - Static Members

static void setup(void);
static void teardown(void);
static struct io_libecc_Error* error(struct io_libecc_Text, struct io_libecc_Chars* message);
static struct io_libecc_Error* rangeError(struct io_libecc_Text, struct io_libecc_Chars* message);
static struct io_libecc_Error* referenceError(struct io_libecc_Text, struct io_libecc_Chars* message);
static struct io_libecc_Error* syntaxError(struct io_libecc_Text, struct io_libecc_Chars* message);
static struct io_libecc_Error* typeError(struct io_libecc_Text, struct io_libecc_Chars* message);
static struct io_libecc_Error* uriError(struct io_libecc_Text, struct io_libecc_Chars* message);
static void destroy(struct io_libecc_Error*);
const struct type_io_libecc_Error io_libecc_Error = {
    setup, teardown, error, rangeError, referenceError, syntaxError, typeError, uriError, destroy,
};

static struct io_libecc_Error * evalError (struct io_libecc_Text text, struct io_libecc_Chars *message);

static
struct io_libecc_Chars *messageValue (struct eccstate_t * const context, struct eccvalue_t value)
{
	if (value.type == io_libecc_value_undefinedType)
		return NULL;
	else if (value.type == io_libecc_value_charsType)
		return value.data.chars;
	else
	{
		value = io_libecc_Value.toString(context, value);
		return io_libecc_Chars.create("%.*s", io_libecc_Value.stringLength(&value), io_libecc_Value.stringBytes(&value));
	}
}

static
struct eccvalue_t toChars (struct eccstate_t * const context, struct eccvalue_t value)
{
	struct eccvalue_t name, message;
	struct eccobject_t *self;
	struct io_libecc_chars_Append chars;
	
	assert(value.type == io_libecc_value_errorType);
	assert(value.data.error);
	
	self = value.data.object;
	
	name = io_libecc_Object.getMember(context, self, io_libecc_key_name);
	if (name.type == io_libecc_value_undefinedType)
		name = io_libecc_Value.text(&io_libecc_text_errorName);
	else
		name = io_libecc_Value.toString(context, name);
	
	message = io_libecc_Object.getMember(context, self, io_libecc_key_message);
	if (message.type == io_libecc_value_undefinedType)
		message = io_libecc_Value.text(&io_libecc_text_empty);
	else
		message = io_libecc_Value.toString(context, message);
	
	io_libecc_Chars.beginAppend(&chars);
	io_libecc_Chars.appendValue(&chars, context, name);
	
	if (io_libecc_Value.stringLength(&name) && io_libecc_Value.stringLength(&message))
		io_libecc_Chars.append(&chars, ": ");
	
	io_libecc_Chars.appendValue(&chars, context, message);
	
	return io_libecc_Chars.endAppend(&chars);
}

static
struct io_libecc_Error * create (struct eccobject_t *errorPrototype, struct io_libecc_Text text, struct io_libecc_Chars *message)
{
	struct io_libecc_Error *self = malloc(sizeof(*self));
	io_libecc_Pool.addObject(&self->object);
	
	*self = io_libecc_Error.identity;
	
	io_libecc_Object.initialize(&self->object, errorPrototype);
	
	self->text = text;
	
	if (message)
	{
		io_libecc_Object.addMember(&self->object, io_libecc_key_message, io_libecc_Value.chars(message), io_libecc_value_hidden);
		++message->referenceCount;
	}
	
	return self;
}

static
struct eccvalue_t toString (struct eccstate_t * const context)
{
	io_libecc_Context.assertThisMask(context, io_libecc_value_objectMask);
	
	return toChars(context, context->this);
}

static
struct eccvalue_t errorConstructor (struct eccstate_t * const context)
{
	struct io_libecc_Chars *message;
	
	message = messageValue(context, io_libecc_Context.argument(context, 0));
	io_libecc_Context.setTextIndex(context, io_libecc_context_callIndex);
	return io_libecc_Value.error(error(io_libecc_Context.textSeek(context), message));
}

static
struct eccvalue_t rangeErrorConstructor (struct eccstate_t * const context)
{
	struct io_libecc_Chars *message;
	
	message = messageValue(context, io_libecc_Context.argument(context, 0));
	io_libecc_Context.setTextIndex(context, io_libecc_context_callIndex);
	return io_libecc_Value.error(rangeError(io_libecc_Context.textSeek(context), message));
}

static
struct eccvalue_t referenceErrorConstructor (struct eccstate_t * const context)
{
	struct io_libecc_Chars *message;
	
	message = messageValue(context, io_libecc_Context.argument(context, 0));
	io_libecc_Context.setTextIndex(context, io_libecc_context_callIndex);
	return io_libecc_Value.error(referenceError(io_libecc_Context.textSeek(context), message));
}

static
struct eccvalue_t syntaxErrorConstructor (struct eccstate_t * const context)
{
	struct io_libecc_Chars *message;
	
	message = messageValue(context, io_libecc_Context.argument(context, 0));
	io_libecc_Context.setTextIndex(context, io_libecc_context_callIndex);
	return io_libecc_Value.error(syntaxError(io_libecc_Context.textSeek(context), message));
}

static
struct eccvalue_t typeErrorConstructor (struct eccstate_t * const context)
{
	struct io_libecc_Chars *message;
	
	message = messageValue(context, io_libecc_Context.argument(context, 0));
	io_libecc_Context.setTextIndex(context, io_libecc_context_callIndex);
	return io_libecc_Value.error(typeError(io_libecc_Context.textSeek(context), message));
}

static
struct eccvalue_t uriErrorConstructor (struct eccstate_t * const context)
{
	struct io_libecc_Chars *message;
	
	message = messageValue(context, io_libecc_Context.argument(context, 0));
	io_libecc_Context.setTextIndex(context, io_libecc_context_callIndex);
	return io_libecc_Value.error(uriError(io_libecc_Context.textSeek(context), message));
}

static
struct eccvalue_t evalErrorConstructor (struct eccstate_t * const context)
{
	struct io_libecc_Chars *message;
	
	message = messageValue(context, io_libecc_Context.argument(context, 0));
	io_libecc_Context.setTextIndex(context, io_libecc_context_callIndex);
	return io_libecc_Value.error(evalError(io_libecc_Context.textSeek(context), message));
}

static
void setupBuiltinObject (struct io_libecc_Function **constructor, const io_libecc_native_io_libecc_Function native, int parameterCount, struct eccobject_t **prototype, const struct io_libecc_Text *name)
{
	io_libecc_Function.setupBuiltinObject(
		constructor, native, 1,
		prototype, io_libecc_Value.error(error(*name, NULL)),
		&io_libecc_error_type);
	
	io_libecc_Object.addMember(*prototype, io_libecc_key_name, io_libecc_Value.text(name), io_libecc_value_hidden);
}

// MARK: - Methods

void setup (void)
{
	const enum io_libecc_value_Flags h = io_libecc_value_hidden;
	
	setupBuiltinObject(&io_libecc_error_constructor, errorConstructor, 1, &io_libecc_error_prototype, &io_libecc_text_errorName);
	setupBuiltinObject(&io_libecc_error_rangeConstructor, rangeErrorConstructor, 1, &io_libecc_error_rangePrototype, &io_libecc_text_rangeErrorName);
	setupBuiltinObject(&io_libecc_error_referenceConstructor, referenceErrorConstructor, 1, &io_libecc_error_referencePrototype, &io_libecc_text_referenceErrorName);
	setupBuiltinObject(&io_libecc_error_syntaxConstructor, syntaxErrorConstructor, 1, &io_libecc_error_syntaxPrototype, &io_libecc_text_syntaxErrorName);
	setupBuiltinObject(&io_libecc_error_typeConstructor, typeErrorConstructor, 1, &io_libecc_error_typePrototype, &io_libecc_text_typeErrorName);
	setupBuiltinObject(&io_libecc_error_uriConstructor, uriErrorConstructor, 1, &io_libecc_error_uriPrototype, &io_libecc_text_uriErrorName);
	setupBuiltinObject(&io_libecc_error_evalConstructor, evalErrorConstructor, 1, &io_libecc_error_evalPrototype, &io_libecc_text_evalErrorName);
	
	io_libecc_Function.addToObject(io_libecc_error_prototype, "toString", toString, 0, h);
	
	io_libecc_Object.addMember(io_libecc_error_prototype, io_libecc_key_message, io_libecc_Value.text(&io_libecc_text_empty), h);
}

void teardown (void)
{
	io_libecc_error_prototype = NULL;
	io_libecc_error_constructor = NULL;
	
	io_libecc_error_rangePrototype = NULL;
	io_libecc_error_rangeConstructor = NULL;
	
	io_libecc_error_referencePrototype = NULL;
	io_libecc_error_referenceConstructor = NULL;
	
	io_libecc_error_syntaxPrototype = NULL;
	io_libecc_error_syntaxConstructor = NULL;
	
	io_libecc_error_typePrototype = NULL;
	io_libecc_error_typeConstructor = NULL;
	
	io_libecc_error_uriPrototype = NULL;
	io_libecc_error_uriConstructor = NULL;
}

struct io_libecc_Error * error (struct io_libecc_Text text, struct io_libecc_Chars *message)
{
	return create(io_libecc_error_prototype, text, message);
}

struct io_libecc_Error * rangeError (struct io_libecc_Text text, struct io_libecc_Chars *message)
{
	return create(io_libecc_error_rangePrototype, text, message);
}

struct io_libecc_Error * referenceError (struct io_libecc_Text text, struct io_libecc_Chars *message)
{
	return create(io_libecc_error_referencePrototype, text, message);
}

struct io_libecc_Error * syntaxError (struct io_libecc_Text text, struct io_libecc_Chars *message)
{
	return create(io_libecc_error_syntaxPrototype, text, message);
}

struct io_libecc_Error * typeError (struct io_libecc_Text text, struct io_libecc_Chars *message)
{
	return create(io_libecc_error_typePrototype, text, message);
}

struct io_libecc_Error * uriError (struct io_libecc_Text text, struct io_libecc_Chars *message)
{
	return create(io_libecc_error_uriPrototype, text, message);
}

struct io_libecc_Error * evalError (struct io_libecc_Text text, struct io_libecc_Chars *message)
{
	return create(io_libecc_error_evalPrototype, text, message);
}

void destroy (struct io_libecc_Error *self)
{
	assert(self);
	if (!self)
		return;
	
	io_libecc_Object.finalize(&self->object);
	
	free(self), self = NULL;
}
