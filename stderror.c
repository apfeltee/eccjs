//
//  error.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"


static ecccharbuffer_t *messageValue(eccstate_t *context, eccvalue_t value);
static eccvalue_t toChars(eccstate_t *context, eccvalue_t value);
static eccobjerror_t *create(eccobject_t *errorPrototype, ecctextstring_t text, ecccharbuffer_t *message);
static eccvalue_t toString(eccstate_t *context);
static eccvalue_t errorConstructor(eccstate_t *context);
static eccvalue_t rangeErrorConstructor(eccstate_t *context);
static eccvalue_t referenceErrorConstructor(eccstate_t *context);
static eccvalue_t syntaxErrorConstructor(eccstate_t *context);
static eccvalue_t typeErrorConstructor(eccstate_t *context);
static eccvalue_t uriErrorConstructor(eccstate_t *context);
static eccvalue_t evalErrorConstructor(eccstate_t *context);
static void setupBuiltinObject(eccobjscriptfunction_t **constructor, const eccnativefuncptr_t native, int parameterCount, eccobject_t **prototype, const ecctextstring_t *name);
static void nserrorfn_setup(void);
static void nserrorfn_teardown(void);
static eccobjerror_t *nserrorfn_error(ecctextstring_t text, ecccharbuffer_t *message);
static eccobjerror_t *nserrorfn_rangeError(ecctextstring_t text, ecccharbuffer_t *message);
static eccobjerror_t *nserrorfn_referenceError(ecctextstring_t text, ecccharbuffer_t *message);
static eccobjerror_t *nserrorfn_syntaxError(ecctextstring_t text, ecccharbuffer_t *message);
static eccobjerror_t *nserrorfn_typeError(ecctextstring_t text, ecccharbuffer_t *message);
static eccobjerror_t *nserrorfn_uriError(ecctextstring_t text, ecccharbuffer_t *message);
static eccobjerror_t *evalError(ecctextstring_t text, ecccharbuffer_t *message);
static void nserrorfn_destroy(eccobjerror_t *self);


eccobject_t* ECC_Prototype_Error = NULL;
eccobject_t* ECC_Prototype_ErrorRangeError = NULL;
eccobject_t* ECC_Prototype_ErrorReferenceError = NULL;
eccobject_t* ECC_Prototype_ErrorSyntaxError = NULL;
eccobject_t* ECC_Prototype_ErrorTypeError = NULL;
eccobject_t* ECC_Prototype_ErrorUriError = NULL;
eccobject_t* ECC_Prototype_ErrorEvalError = NULL;

eccobjscriptfunction_t* ECC_CtorFunc_Error = NULL;
eccobjscriptfunction_t* ECC_CtorFunc_ErrorRangeError = NULL;
eccobjscriptfunction_t* ECC_CtorFunc_ErrorReferenceError = NULL;
eccobjscriptfunction_t* ECC_CtorFunc_ErrorSyntaxError = NULL;
eccobjscriptfunction_t* ECC_CtorFunc_ErrorTypeError = NULL;
eccobjscriptfunction_t* ECC_CtorFunc_ErrorUriError = NULL;
eccobjscriptfunction_t* ECC_CtorFunc_ErrorEvalError = NULL;

const eccobjinterntype_t ECC_Type_Error = {
    .text = &ECC_ConstString_ErrorType,
};

const struct eccpseudonserror_t ECCNSError = {
    nserrorfn_setup, nserrorfn_teardown, nserrorfn_error, nserrorfn_rangeError, nserrorfn_referenceError, nserrorfn_syntaxError, nserrorfn_typeError, nserrorfn_uriError, nserrorfn_destroy,
    {}
};

static ecccharbuffer_t* messageValue(eccstate_t* context, eccvalue_t value)
{
    if(value.type == ECC_VALTYPE_UNDEFINED)
        return NULL;
    else if(value.type == ECC_VALTYPE_CHARS)
        return value.data.chars;
    else
    {
        value = ECCNSValue.toString(context, value);
        return ECCNSChars.create("%.*s", ECCNSValue.stringLength(&value), ECCNSValue.stringBytes(&value));
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

    name = ECCNSObject.getMember(context, self, ECC_ConstKey_name);
    if(name.type == ECC_VALTYPE_UNDEFINED)
        name = ECCNSValue.text(&ECC_ConstString_ErrorName);
    else
        name = ECCNSValue.toString(context, name);

    message = ECCNSObject.getMember(context, self, ECC_ConstKey_message);
    if(message.type == ECC_VALTYPE_UNDEFINED)
        message = ECCNSValue.text(&ECC_ConstString_Empty);
    else
        message = ECCNSValue.toString(context, message);

    ECCNSChars.beginAppend(&chars);
    ECCNSChars.appendValue(&chars, context, name);

    if(ECCNSValue.stringLength(&name) && ECCNSValue.stringLength(&message))
        ECCNSChars.append(&chars, ": ");

    ECCNSChars.appendValue(&chars, context, message);

    return ECCNSChars.endAppend(&chars);
}

static eccobjerror_t* create(eccobject_t* errorPrototype, ecctextstring_t text, ecccharbuffer_t* message)
{
    eccobjerror_t* self = malloc(sizeof(*self));
    ECCNSMemoryPool.addObject(&self->object);

    *self = ECCNSError.identity;

    ECCNSObject.initialize(&self->object, errorPrototype);

    self->text = text;

    if(message)
    {
        ECCNSObject.addMember(&self->object, ECC_ConstKey_message, ECCNSValue.chars(message), ECC_VALFLAG_HIDDEN);
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
    ECCNSFunction.setupBuiltinObject(constructor, native, 1, prototype, ECCNSValue.error(nserrorfn_error(*name, NULL)), &ECC_Type_Error);

    ECCNSObject.addMember(*prototype, ECC_ConstKey_name, ECCNSValue.text(name), ECC_VALFLAG_HIDDEN);
}

// MARK: - Methods

static void nserrorfn_setup(void)
{
    const eccvalflag_t h = ECC_VALFLAG_HIDDEN;

    setupBuiltinObject(&ECC_CtorFunc_Error, errorConstructor, 1, &ECC_Prototype_Error, &ECC_ConstString_ErrorName);
    setupBuiltinObject(&ECC_CtorFunc_ErrorRangeError, rangeErrorConstructor, 1, &ECC_Prototype_ErrorRangeError, &ECC_ConstString_RangeErrorName);
    setupBuiltinObject(&ECC_CtorFunc_ErrorReferenceError, referenceErrorConstructor, 1, &ECC_Prototype_ErrorReferenceError, &ECC_ConstString_ReferenceErrorName);
    setupBuiltinObject(&ECC_CtorFunc_ErrorSyntaxError, syntaxErrorConstructor, 1, &ECC_Prototype_ErrorSyntaxError, &ECC_ConstString_SyntaxErrorName);
    setupBuiltinObject(&ECC_CtorFunc_ErrorTypeError, typeErrorConstructor, 1, &ECC_Prototype_ErrorTypeError, &ECC_ConstString_TypeErrorName);
    setupBuiltinObject(&ECC_CtorFunc_ErrorUriError, uriErrorConstructor, 1, &ECC_Prototype_ErrorUriError, &ECC_ConstString_UriErrorName);
    setupBuiltinObject(&ECC_CtorFunc_ErrorEvalError, evalErrorConstructor, 1, &ECC_Prototype_ErrorEvalError, &ECC_ConstString_EvalErrorName);

    ECCNSFunction.addToObject(ECC_Prototype_Error, "toString", toString, 0, h);

    ECCNSObject.addMember(ECC_Prototype_Error, ECC_ConstKey_message, ECCNSValue.text(&ECC_ConstString_Empty), h);
}

static void nserrorfn_teardown(void)
{
    ECC_Prototype_Error = NULL;
    ECC_CtorFunc_Error = NULL;

    ECC_Prototype_ErrorRangeError = NULL;
    ECC_CtorFunc_ErrorRangeError = NULL;

    ECC_Prototype_ErrorReferenceError = NULL;
    ECC_CtorFunc_ErrorReferenceError = NULL;

    ECC_Prototype_ErrorSyntaxError = NULL;
    ECC_CtorFunc_ErrorSyntaxError = NULL;

    ECC_Prototype_ErrorTypeError = NULL;
    ECC_CtorFunc_ErrorTypeError = NULL;

    ECC_Prototype_ErrorUriError = NULL;
    ECC_CtorFunc_ErrorUriError = NULL;
}

static eccobjerror_t* nserrorfn_error(ecctextstring_t text, ecccharbuffer_t* message)
{
    return create(ECC_Prototype_Error, text, message);
}

static eccobjerror_t* nserrorfn_rangeError(ecctextstring_t text, ecccharbuffer_t* message)
{
    return create(ECC_Prototype_ErrorRangeError, text, message);
}

static eccobjerror_t* nserrorfn_referenceError(ecctextstring_t text, ecccharbuffer_t* message)
{
    return create(ECC_Prototype_ErrorReferenceError, text, message);
}

static eccobjerror_t* nserrorfn_syntaxError(ecctextstring_t text, ecccharbuffer_t* message)
{
    return create(ECC_Prototype_ErrorSyntaxError, text, message);
}

static eccobjerror_t* nserrorfn_typeError(ecctextstring_t text, ecccharbuffer_t* message)
{
    return create(ECC_Prototype_ErrorTypeError, text, message);
}

static eccobjerror_t* nserrorfn_uriError(ecctextstring_t text, ecccharbuffer_t* message)
{
    return create(ECC_Prototype_ErrorUriError, text, message);
}

static eccobjerror_t* evalError(ecctextstring_t text, ecccharbuffer_t* message)
{
    return create(ECC_Prototype_ErrorEvalError, text, message);
}

static void nserrorfn_destroy(eccobjerror_t* self)
{
    assert(self);
    if(!self)
        return;

    ECCNSObject.finalize(&self->object);

    free(self), self = NULL;
}
