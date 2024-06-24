//
//  error.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"

// MARK: - Private

eccobject_t * io_libecc_error_prototype = NULL;
eccobject_t * io_libecc_error_rangePrototype = NULL;
eccobject_t * io_libecc_error_referencePrototype = NULL;
eccobject_t * io_libecc_error_syntaxPrototype = NULL;
eccobject_t * io_libecc_error_typePrototype = NULL;
eccobject_t * io_libecc_error_uriPrototype = NULL;
eccobject_t * io_libecc_error_evalPrototype = NULL;

struct io_libecc_Function * io_libecc_error_constructor = NULL;
struct io_libecc_Function * io_libecc_error_rangeConstructor = NULL;
struct io_libecc_Function * io_libecc_error_referenceConstructor = NULL;
struct io_libecc_Function * io_libecc_error_syntaxConstructor = NULL;
struct io_libecc_Function * io_libecc_error_typeConstructor = NULL;
struct io_libecc_Function * io_libecc_error_uriConstructor = NULL;
struct io_libecc_Function * io_libecc_error_evalConstructor = NULL;

const struct io_libecc_object_Type io_libecc_error_type = {
	.text = &ECC_ConstString_ErrorType,
};

// MARK: - Static Members

static void setup(void);
static void teardown(void);
static struct io_libecc_Error* error(ecctextstring_t, struct io_libecc_Chars* message);
static struct io_libecc_Error* rangeError(ecctextstring_t, struct io_libecc_Chars* message);
static struct io_libecc_Error* referenceError(ecctextstring_t, struct io_libecc_Chars* message);
static struct io_libecc_Error* syntaxError(ecctextstring_t, struct io_libecc_Chars* message);
static struct io_libecc_Error* typeError(ecctextstring_t, struct io_libecc_Chars* message);
static struct io_libecc_Error* uriError(ecctextstring_t, struct io_libecc_Chars* message);
static void destroy(struct io_libecc_Error*);
const struct type_io_libecc_Error io_libecc_Error = {
    setup, teardown, error, rangeError, referenceError, syntaxError, typeError, uriError, destroy,
};

static struct io_libecc_Error * evalError (ecctextstring_t text, struct io_libecc_Chars *message);

static
struct io_libecc_Chars *messageValue (eccstate_t * const context, eccvalue_t value)
{
	if (value.type == ECC_VALTYPE_UNDEFINED)
		return NULL;
	else if (value.type == ECC_VALTYPE_CHARS)
		return value.data.chars;
	else
	{
		value = ECCNSValue.toString(context, value);
		return io_libecc_Chars.create("%.*s", ECCNSValue.stringLength(&value), ECCNSValue.stringBytes(&value));
	}
}

static
eccvalue_t toChars (eccstate_t * const context, eccvalue_t value)
{
	eccvalue_t name, message;
	eccobject_t *self;
	struct io_libecc_chars_Append chars;
	
	assert(value.type == ECC_VALTYPE_ERROR);
	assert(value.data.error);
	
	self = value.data.object;
	
	name = io_libecc_Object.getMember(context, self, io_libecc_key_name);
	if (name.type == ECC_VALTYPE_UNDEFINED)
		name = ECCNSValue.text(&ECC_ConstString_ErrorName);
	else
		name = ECCNSValue.toString(context, name);
	
	message = io_libecc_Object.getMember(context, self, io_libecc_key_message);
	if (message.type == ECC_VALTYPE_UNDEFINED)
		message = ECCNSValue.text(&ECC_ConstString_Empty);
	else
		message = ECCNSValue.toString(context, message);
	
	io_libecc_Chars.beginAppend(&chars);
	io_libecc_Chars.appendValue(&chars, context, name);
	
	if (ECCNSValue.stringLength(&name) && ECCNSValue.stringLength(&message))
		io_libecc_Chars.append(&chars, ": ");
	
	io_libecc_Chars.appendValue(&chars, context, message);
	
	return io_libecc_Chars.endAppend(&chars);
}

static
struct io_libecc_Error * create (eccobject_t *errorPrototype, ecctextstring_t text, struct io_libecc_Chars *message)
{
	struct io_libecc_Error *self = malloc(sizeof(*self));
	io_libecc_Pool.addObject(&self->object);
	
	*self = io_libecc_Error.identity;
	
	io_libecc_Object.initialize(&self->object, errorPrototype);
	
	self->text = text;
	
	if (message)
	{
		io_libecc_Object.addMember(&self->object, io_libecc_key_message, ECCNSValue.chars(message), io_libecc_value_hidden);
		++message->referenceCount;
	}
	
	return self;
}

static
eccvalue_t toString (eccstate_t * const context)
{
	io_libecc_Context.assertThisMask(context, ECC_VALMASK_OBJECT);
	
	return toChars(context, context->this);
}

static
eccvalue_t errorConstructor (eccstate_t * const context)
{
	struct io_libecc_Chars *message;
	
	message = messageValue(context, io_libecc_Context.argument(context, 0));
	io_libecc_Context.setTextIndex(context, io_libecc_context_callIndex);
	return ECCNSValue.error(error(io_libecc_Context.textSeek(context), message));
}

static
eccvalue_t rangeErrorConstructor (eccstate_t * const context)
{
	struct io_libecc_Chars *message;
	
	message = messageValue(context, io_libecc_Context.argument(context, 0));
	io_libecc_Context.setTextIndex(context, io_libecc_context_callIndex);
	return ECCNSValue.error(rangeError(io_libecc_Context.textSeek(context), message));
}

static
eccvalue_t referenceErrorConstructor (eccstate_t * const context)
{
	struct io_libecc_Chars *message;
	
	message = messageValue(context, io_libecc_Context.argument(context, 0));
	io_libecc_Context.setTextIndex(context, io_libecc_context_callIndex);
	return ECCNSValue.error(referenceError(io_libecc_Context.textSeek(context), message));
}

static
eccvalue_t syntaxErrorConstructor (eccstate_t * const context)
{
	struct io_libecc_Chars *message;
	
	message = messageValue(context, io_libecc_Context.argument(context, 0));
	io_libecc_Context.setTextIndex(context, io_libecc_context_callIndex);
	return ECCNSValue.error(syntaxError(io_libecc_Context.textSeek(context), message));
}

static
eccvalue_t typeErrorConstructor (eccstate_t * const context)
{
	struct io_libecc_Chars *message;
	
	message = messageValue(context, io_libecc_Context.argument(context, 0));
	io_libecc_Context.setTextIndex(context, io_libecc_context_callIndex);
	return ECCNSValue.error(typeError(io_libecc_Context.textSeek(context), message));
}

static
eccvalue_t uriErrorConstructor (eccstate_t * const context)
{
	struct io_libecc_Chars *message;
	
	message = messageValue(context, io_libecc_Context.argument(context, 0));
	io_libecc_Context.setTextIndex(context, io_libecc_context_callIndex);
	return ECCNSValue.error(uriError(io_libecc_Context.textSeek(context), message));
}

static
eccvalue_t evalErrorConstructor (eccstate_t * const context)
{
	struct io_libecc_Chars *message;
	
	message = messageValue(context, io_libecc_Context.argument(context, 0));
	io_libecc_Context.setTextIndex(context, io_libecc_context_callIndex);
	return ECCNSValue.error(evalError(io_libecc_Context.textSeek(context), message));
}

static
void setupBuiltinObject (struct io_libecc_Function **constructor, const io_libecc_native_io_libecc_Function native, int parameterCount, eccobject_t **prototype, const ecctextstring_t *name)
{
	io_libecc_Function.setupBuiltinObject(
		constructor, native, 1,
		prototype, ECCNSValue.error(error(*name, NULL)),
		&io_libecc_error_type);
	
	io_libecc_Object.addMember(*prototype, io_libecc_key_name, ECCNSValue.text(name), io_libecc_value_hidden);
}

// MARK: - Methods

void setup (void)
{
	const enum io_libecc_value_Flags h = io_libecc_value_hidden;
	
	setupBuiltinObject(&io_libecc_error_constructor, errorConstructor, 1, &io_libecc_error_prototype, &ECC_ConstString_ErrorName);
	setupBuiltinObject(&io_libecc_error_rangeConstructor, rangeErrorConstructor, 1, &io_libecc_error_rangePrototype, &ECC_ConstString_RangeErrorName);
	setupBuiltinObject(&io_libecc_error_referenceConstructor, referenceErrorConstructor, 1, &io_libecc_error_referencePrototype, &ECC_ConstString_ReferenceErrorName);
	setupBuiltinObject(&io_libecc_error_syntaxConstructor, syntaxErrorConstructor, 1, &io_libecc_error_syntaxPrototype, &ECC_ConstString_SyntaxErrorName);
	setupBuiltinObject(&io_libecc_error_typeConstructor, typeErrorConstructor, 1, &io_libecc_error_typePrototype, &ECC_ConstString_TypeErrorName);
	setupBuiltinObject(&io_libecc_error_uriConstructor, uriErrorConstructor, 1, &io_libecc_error_uriPrototype, &ECC_ConstString_UriErrorName);
	setupBuiltinObject(&io_libecc_error_evalConstructor, evalErrorConstructor, 1, &io_libecc_error_evalPrototype, &ECC_ConstString_EvalErrorName);
	
	io_libecc_Function.addToObject(io_libecc_error_prototype, "toString", toString, 0, h);
	
	io_libecc_Object.addMember(io_libecc_error_prototype, io_libecc_key_message, ECCNSValue.text(&ECC_ConstString_Empty), h);
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

struct io_libecc_Error * error (ecctextstring_t text, struct io_libecc_Chars *message)
{
	return create(io_libecc_error_prototype, text, message);
}

struct io_libecc_Error * rangeError (ecctextstring_t text, struct io_libecc_Chars *message)
{
	return create(io_libecc_error_rangePrototype, text, message);
}

struct io_libecc_Error * referenceError (ecctextstring_t text, struct io_libecc_Chars *message)
{
	return create(io_libecc_error_referencePrototype, text, message);
}

struct io_libecc_Error * syntaxError (ecctextstring_t text, struct io_libecc_Chars *message)
{
	return create(io_libecc_error_syntaxPrototype, text, message);
}

struct io_libecc_Error * typeError (ecctextstring_t text, struct io_libecc_Chars *message)
{
	return create(io_libecc_error_typePrototype, text, message);
}

struct io_libecc_Error * uriError (ecctextstring_t text, struct io_libecc_Chars *message)
{
	return create(io_libecc_error_uriPrototype, text, message);
}

struct io_libecc_Error * evalError (ecctextstring_t text, struct io_libecc_Chars *message)
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
