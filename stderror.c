//
//  error.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"

// MARK: - Private

eccobject_t* io_libecc_error_prototype = NULL;
eccobject_t* io_libecc_error_rangePrototype = NULL;
eccobject_t* io_libecc_error_referencePrototype = NULL;
eccobject_t* io_libecc_error_syntaxPrototype = NULL;
eccobject_t* io_libecc_error_typePrototype = NULL;
eccobject_t* io_libecc_error_uriPrototype = NULL;
eccobject_t* io_libecc_error_evalPrototype = NULL;

eccobjscriptfunction_t* io_libecc_error_constructor = NULL;
eccobjscriptfunction_t* io_libecc_error_rangeConstructor = NULL;
eccobjscriptfunction_t* io_libecc_error_referenceConstructor = NULL;
eccobjscriptfunction_t* io_libecc_error_syntaxConstructor = NULL;
eccobjscriptfunction_t* io_libecc_error_typeConstructor = NULL;
eccobjscriptfunction_t* io_libecc_error_uriConstructor = NULL;
eccobjscriptfunction_t* io_libecc_error_evalConstructor = NULL;

const eccobjinterntype_t io_libecc_error_type = {
    .text = &ECC_ConstString_ErrorType,
};

// MARK: - Static Members

static void nserrorfn_setup(void);
static void nserrorfn_teardown(void);
static eccobjerror_t* nserrorfn_error(ecctextstring_t, ecccharbuffer_t* message);
static eccobjerror_t* nserrorfn_rangeError(ecctextstring_t, ecccharbuffer_t* message);
static eccobjerror_t* nserrorfn_referenceError(ecctextstring_t, ecccharbuffer_t* message);
static eccobjerror_t* nserrorfn_syntaxError(ecctextstring_t, ecccharbuffer_t* message);
static eccobjerror_t* nserrorfn_typeError(ecctextstring_t, ecccharbuffer_t* message);
static eccobjerror_t* nserrorfn_uriError(ecctextstring_t, ecccharbuffer_t* message);
static void nserrorfn_destroy(eccobjerror_t*);
const struct eccpseudonserror_t io_libecc_Error = {
    nserrorfn_setup, nserrorfn_teardown, nserrorfn_error, nserrorfn_rangeError, nserrorfn_referenceError, nserrorfn_syntaxError, nserrorfn_typeError, nserrorfn_uriError, nserrorfn_destroy,
    {}
};

static eccobjerror_t* evalError(ecctextstring_t text, ecccharbuffer_t* message);

static ecccharbuffer_t* messageValue(eccstate_t* context, eccvalue_t value)
{
    if(value.type == ECC_VALTYPE_UNDEFINED)
        return NULL;
    else if(value.type == ECC_VALTYPE_CHARS)
        return value.data.chars;
    else
    {
        value = ECCNSValue.toString(context, value);
        return io_libecc_Chars.create("%.*s", ECCNSValue.stringLength(&value), ECCNSValue.stringBytes(&value));
    }
}

static eccvalue_t toChars(eccstate_t* context, eccvalue_t value)
{
    eccvalue_t name, message;
    eccobject_t* self;
    eccappendbuffer_t chars;

    assert(value.type == ECC_VALTYPE_ERROR);
    assert(value.data.error);

    self = value.data.object;

    name = ECCNSObject.getMember(context, self, io_libecc_key_name);
    if(name.type == ECC_VALTYPE_UNDEFINED)
        name = ECCNSValue.text(&ECC_ConstString_ErrorName);
    else
        name = ECCNSValue.toString(context, name);

    message = ECCNSObject.getMember(context, self, io_libecc_key_message);
    if(message.type == ECC_VALTYPE_UNDEFINED)
        message = ECCNSValue.text(&ECC_ConstString_Empty);
    else
        message = ECCNSValue.toString(context, message);

    io_libecc_Chars.beginAppend(&chars);
    io_libecc_Chars.appendValue(&chars, context, name);

    if(ECCNSValue.stringLength(&name) && ECCNSValue.stringLength(&message))
        io_libecc_Chars.append(&chars, ": ");

    io_libecc_Chars.appendValue(&chars, context, message);

    return io_libecc_Chars.endAppend(&chars);
}

static eccobjerror_t* create(eccobject_t* errorPrototype, ecctextstring_t text, ecccharbuffer_t* message)
{
    eccobjerror_t* self = malloc(sizeof(*self));
    io_libecc_Pool.addObject(&self->object);

    *self = io_libecc_Error.identity;

    ECCNSObject.initialize(&self->object, errorPrototype);

    self->text = text;

    if(message)
    {
        ECCNSObject.addMember(&self->object, io_libecc_key_message, ECCNSValue.chars(message), ECC_VALFLAG_HIDDEN);
        ++message->referenceCount;
    }

    return self;
}

static eccvalue_t toString(eccstate_t* context)
{
    ECCNSContext.assertThisMask(context, ECC_VALMASK_OBJECT);

    return toChars(context, context->thisvalue);
}

static eccvalue_t errorConstructor(eccstate_t* context)
{
    ecccharbuffer_t* message;

    message = messageValue(context, ECCNSContext.argument(context, 0));
    ECCNSContext.setTextIndex(context, ECC_CTXINDEXTYPE_CALL);
    return ECCNSValue.error(nserrorfn_error(ECCNSContext.textSeek(context), message));
}

static eccvalue_t rangeErrorConstructor(eccstate_t* context)
{
    ecccharbuffer_t* message;

    message = messageValue(context, ECCNSContext.argument(context, 0));
    ECCNSContext.setTextIndex(context, ECC_CTXINDEXTYPE_CALL);
    return ECCNSValue.error(nserrorfn_rangeError(ECCNSContext.textSeek(context), message));
}

static eccvalue_t referenceErrorConstructor(eccstate_t* context)
{
    ecccharbuffer_t* message;

    message = messageValue(context, ECCNSContext.argument(context, 0));
    ECCNSContext.setTextIndex(context, ECC_CTXINDEXTYPE_CALL);
    return ECCNSValue.error(nserrorfn_referenceError(ECCNSContext.textSeek(context), message));
}

static eccvalue_t syntaxErrorConstructor(eccstate_t* context)
{
    ecccharbuffer_t* message;

    message = messageValue(context, ECCNSContext.argument(context, 0));
    ECCNSContext.setTextIndex(context, ECC_CTXINDEXTYPE_CALL);
    return ECCNSValue.error(nserrorfn_syntaxError(ECCNSContext.textSeek(context), message));
}

static eccvalue_t typeErrorConstructor(eccstate_t* context)
{
    ecccharbuffer_t* message;

    message = messageValue(context, ECCNSContext.argument(context, 0));
    ECCNSContext.setTextIndex(context, ECC_CTXINDEXTYPE_CALL);
    return ECCNSValue.error(nserrorfn_typeError(ECCNSContext.textSeek(context), message));
}

static eccvalue_t uriErrorConstructor(eccstate_t* context)
{
    ecccharbuffer_t* message;

    message = messageValue(context, ECCNSContext.argument(context, 0));
    ECCNSContext.setTextIndex(context, ECC_CTXINDEXTYPE_CALL);
    return ECCNSValue.error(nserrorfn_uriError(ECCNSContext.textSeek(context), message));
}

static eccvalue_t evalErrorConstructor(eccstate_t* context)
{
    ecccharbuffer_t* message;

    message = messageValue(context, ECCNSContext.argument(context, 0));
    ECCNSContext.setTextIndex(context, ECC_CTXINDEXTYPE_CALL);
    return ECCNSValue.error(evalError(ECCNSContext.textSeek(context), message));
}

static void
setupBuiltinObject(eccobjscriptfunction_t** constructor, const eccnativefuncptr_t native, int parameterCount, eccobject_t** prototype, const ecctextstring_t* name)
{
    (void)parameterCount;
    io_libecc_Function.setupBuiltinObject(constructor, native, 1, prototype, ECCNSValue.error(nserrorfn_error(*name, NULL)), &io_libecc_error_type);

    ECCNSObject.addMember(*prototype, io_libecc_key_name, ECCNSValue.text(name), ECC_VALFLAG_HIDDEN);
}

// MARK: - Methods

void nserrorfn_setup(void)
{
    const eccvalflag_t h = ECC_VALFLAG_HIDDEN;

    setupBuiltinObject(&io_libecc_error_constructor, errorConstructor, 1, &io_libecc_error_prototype, &ECC_ConstString_ErrorName);
    setupBuiltinObject(&io_libecc_error_rangeConstructor, rangeErrorConstructor, 1, &io_libecc_error_rangePrototype, &ECC_ConstString_RangeErrorName);
    setupBuiltinObject(&io_libecc_error_referenceConstructor, referenceErrorConstructor, 1, &io_libecc_error_referencePrototype, &ECC_ConstString_ReferenceErrorName);
    setupBuiltinObject(&io_libecc_error_syntaxConstructor, syntaxErrorConstructor, 1, &io_libecc_error_syntaxPrototype, &ECC_ConstString_SyntaxErrorName);
    setupBuiltinObject(&io_libecc_error_typeConstructor, typeErrorConstructor, 1, &io_libecc_error_typePrototype, &ECC_ConstString_TypeErrorName);
    setupBuiltinObject(&io_libecc_error_uriConstructor, uriErrorConstructor, 1, &io_libecc_error_uriPrototype, &ECC_ConstString_UriErrorName);
    setupBuiltinObject(&io_libecc_error_evalConstructor, evalErrorConstructor, 1, &io_libecc_error_evalPrototype, &ECC_ConstString_EvalErrorName);

    io_libecc_Function.addToObject(io_libecc_error_prototype, "toString", toString, 0, h);

    ECCNSObject.addMember(io_libecc_error_prototype, io_libecc_key_message, ECCNSValue.text(&ECC_ConstString_Empty), h);
}

void nserrorfn_teardown(void)
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

eccobjerror_t* nserrorfn_error(ecctextstring_t text, ecccharbuffer_t* message)
{
    return create(io_libecc_error_prototype, text, message);
}

eccobjerror_t* nserrorfn_rangeError(ecctextstring_t text, ecccharbuffer_t* message)
{
    return create(io_libecc_error_rangePrototype, text, message);
}

eccobjerror_t* nserrorfn_referenceError(ecctextstring_t text, ecccharbuffer_t* message)
{
    return create(io_libecc_error_referencePrototype, text, message);
}

eccobjerror_t* nserrorfn_syntaxError(ecctextstring_t text, ecccharbuffer_t* message)
{
    return create(io_libecc_error_syntaxPrototype, text, message);
}

eccobjerror_t* nserrorfn_typeError(ecctextstring_t text, ecccharbuffer_t* message)
{
    return create(io_libecc_error_typePrototype, text, message);
}

eccobjerror_t* nserrorfn_uriError(ecctextstring_t text, ecccharbuffer_t* message)
{
    return create(io_libecc_error_uriPrototype, text, message);
}

eccobjerror_t* evalError(ecctextstring_t text, ecccharbuffer_t* message)
{
    return create(io_libecc_error_evalPrototype, text, message);
}

void nserrorfn_destroy(eccobjerror_t* self)
{
    assert(self);
    if(!self)
        return;

    ECCNSObject.finalize(&self->object);

    free(self), self = NULL;
}
