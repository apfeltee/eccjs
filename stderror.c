
/*
//  error.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
*/

#include "ecc.h"

eccvalue_t objerrorfn_toString(ecccontext_t *context);
eccvalue_t objerrorfn_ctorError(ecccontext_t *context);
eccvalue_t objerrorfn_ctorRangeError(ecccontext_t *context);
eccvalue_t objerrorfn_ctorReferenceError(ecccontext_t *context);
eccvalue_t objerrorfn_ctorSyntaxError(ecccontext_t *context);
eccvalue_t objerrorfn_ctorTypeError(ecccontext_t *context);
eccvalue_t objerrorfn_ctorUriError(ecccontext_t *context);
eccvalue_t objerrorfn_ctorEvalError(ecccontext_t *context);

eccobject_t* ECC_Prototype_Error = NULL;
eccobject_t* ECC_Prototype_ErrorRangeError = NULL;
eccobject_t* ECC_Prototype_ErrorReferenceError = NULL;
eccobject_t* ECC_Prototype_ErrorSyntaxError = NULL;
eccobject_t* ECC_Prototype_ErrorTypeError = NULL;
eccobject_t* ECC_Prototype_ErrorUriError = NULL;
eccobject_t* ECC_Prototype_ErrorEvalError = NULL;

eccobjfunction_t* ECC_CtorFunc_Error = NULL;
eccobjfunction_t* ECC_CtorFunc_ErrorRangeError = NULL;
eccobjfunction_t* ECC_CtorFunc_ErrorReferenceError = NULL;
eccobjfunction_t* ECC_CtorFunc_ErrorSyntaxError = NULL;
eccobjfunction_t* ECC_CtorFunc_ErrorTypeError = NULL;
eccobjfunction_t* ECC_CtorFunc_ErrorUriError = NULL;
eccobjfunction_t* ECC_CtorFunc_ErrorEvalError = NULL;

const eccobjinterntype_t ECC_Type_Error = {
    .text = &ECC_String_ErrorType,
};

eccstrbuffer_t* ecc_error_messagevalue(ecccontext_t* context, eccvalue_t value)
{
    if(value.type == ECC_VALTYPE_UNDEFINED)
        return NULL;
    else if(value.type == ECC_VALTYPE_CHARS)
        return value.data.chars;
    else
    {
        value = ecc_value_tostring(context, value);
        return ecc_strbuf_create("%.*s", ecc_value_stringlength(&value), ecc_value_stringbytes(&value));
    }
}

eccvalue_t ecc_error_tochars(ecccontext_t* context, eccvalue_t value)
{
    eccvalue_t name, message;
    eccobject_t* self;
    eccappendbuffer_t chars;

    assert(value.type == ECC_VALTYPE_ERROR);
    assert(value.data.error);

    self = value.data.object;

    name = ecc_object_getmember(context, self, ECC_ConstKey_name);
    if(name.type == ECC_VALTYPE_UNDEFINED)
        name = ecc_value_fromtext(&ECC_String_ErrorName);
    else
        name = ecc_value_tostring(context, name);

    message = ecc_object_getmember(context, self, ECC_ConstKey_message);
    if(message.type == ECC_VALTYPE_UNDEFINED)
        message = ecc_value_fromtext(&ECC_String_Empty);
    else
        message = ecc_value_tostring(context, message);

    ecc_strbuf_beginappend(&chars);
    ecc_strbuf_appendvalue(&chars, context, name);

    if(ecc_value_stringlength(&name) && ecc_value_stringlength(&message))
        ecc_strbuf_append(&chars, ": ");

    ecc_strbuf_appendvalue(&chars, context, message);

    return ecc_strbuf_endappend(&chars);
}

eccobjerror_t* ecc_error_create(eccobject_t* errorPrototype, ecctextstring_t text, eccstrbuffer_t* message)
{
    eccobjerror_t* self = (eccobjerror_t*)malloc(sizeof(*self));
    ecc_mempool_addobject(&self->object);
    memset(self, 0, sizeof(eccobjerror_t));

    ecc_object_initialize(&self->object, errorPrototype);

    self->text = text;

    if(message)
    {
        ecc_object_addmember(&self->object, ECC_ConstKey_message, ecc_value_fromchars(message), ECC_VALFLAG_HIDDEN);
        ++message->refcount;
    }

    return self;
}

eccvalue_t objerrorfn_toString(ecccontext_t* context)
{
    ecc_context_assertthismask(context, ECC_VALMASK_OBJECT);

    return ecc_error_tochars(context, context->thisvalue);
}

eccvalue_t objerrorfn_ctorError(ecccontext_t* context)
{
    eccstrbuffer_t* message;

    message = ecc_error_messagevalue(context, ecc_context_argument(context, 0));
    ecc_context_settextindex(context, ECC_CTXINDEXTYPE_CALL);
    return ecc_value_error(ecc_error_error(ecc_context_textseek(context), message));
}

eccvalue_t objerrorfn_ctorRangeError(ecccontext_t* context)
{
    eccstrbuffer_t* message;

    message = ecc_error_messagevalue(context, ecc_context_argument(context, 0));
    ecc_context_settextindex(context, ECC_CTXINDEXTYPE_CALL);
    return ecc_value_error(ecc_error_rangeerror(ecc_context_textseek(context), message));
}

eccvalue_t objerrorfn_ctorReferenceError(ecccontext_t* context)
{
    eccstrbuffer_t* message;

    message = ecc_error_messagevalue(context, ecc_context_argument(context, 0));
    ecc_context_settextindex(context, ECC_CTXINDEXTYPE_CALL);
    return ecc_value_error(ecc_error_referenceerror(ecc_context_textseek(context), message));
}

eccvalue_t objerrorfn_ctorSyntaxError(ecccontext_t* context)
{
    eccstrbuffer_t* message;

    message = ecc_error_messagevalue(context, ecc_context_argument(context, 0));
    ecc_context_settextindex(context, ECC_CTXINDEXTYPE_CALL);
    return ecc_value_error(ecc_error_syntaxerror(ecc_context_textseek(context), message));
}

eccvalue_t objerrorfn_ctorTypeError(ecccontext_t* context)
{
    eccstrbuffer_t* message;

    message = ecc_error_messagevalue(context, ecc_context_argument(context, 0));
    ecc_context_settextindex(context, ECC_CTXINDEXTYPE_CALL);
    return ecc_value_error(ecc_error_typeerror(ecc_context_textseek(context), message));
}

eccvalue_t objerrorfn_ctorUriError(ecccontext_t* context)
{
    eccstrbuffer_t* message;

    message = ecc_error_messagevalue(context, ecc_context_argument(context, 0));
    ecc_context_settextindex(context, ECC_CTXINDEXTYPE_CALL);
    return ecc_value_error(ecc_error_urierror(ecc_context_textseek(context), message));
}

eccvalue_t objerrorfn_ctorEvalError(ecccontext_t* context)
{
    eccstrbuffer_t* message;

    message = ecc_error_messagevalue(context, ecc_context_argument(context, 0));
    ecc_context_settextindex(context, ECC_CTXINDEXTYPE_CALL);
    return ecc_value_error(ecc_error_evalerror(ecc_context_textseek(context), message));
}

void
ecc_error_setupbo(eccobjfunction_t** pctor, const eccnativefuncptr_t native, int paramcnt, eccobject_t** prototype, const ecctextstring_t* name)
{
    (void)paramcnt;
    ecc_function_setupbuiltinobject(pctor, native, 1, prototype, ecc_value_error(ecc_error_error(*name, NULL)), &ECC_Type_Error);

    ecc_object_addmember(*prototype, ECC_ConstKey_name, ecc_value_fromtext(name), ECC_VALFLAG_HIDDEN);
}

void ecc_error_setup(void)
{
    const eccvalflag_t h = ECC_VALFLAG_HIDDEN;

    ecc_error_setupbo(&ECC_CtorFunc_Error, objerrorfn_ctorError, 1, &ECC_Prototype_Error, &ECC_String_ErrorName);
    ecc_error_setupbo(&ECC_CtorFunc_ErrorRangeError, objerrorfn_ctorRangeError, 1, &ECC_Prototype_ErrorRangeError, &ECC_String_RangeErrorName);
    ecc_error_setupbo(&ECC_CtorFunc_ErrorReferenceError, objerrorfn_ctorReferenceError, 1, &ECC_Prototype_ErrorReferenceError, &ECC_String_ReferenceErrorName);
    ecc_error_setupbo(&ECC_CtorFunc_ErrorSyntaxError, objerrorfn_ctorSyntaxError, 1, &ECC_Prototype_ErrorSyntaxError, &ECC_String_SyntaxErrorName);
    ecc_error_setupbo(&ECC_CtorFunc_ErrorTypeError, objerrorfn_ctorTypeError, 1, &ECC_Prototype_ErrorTypeError, &ECC_String_TypeErrorName);
    ecc_error_setupbo(&ECC_CtorFunc_ErrorUriError, objerrorfn_ctorUriError, 1, &ECC_Prototype_ErrorUriError, &ECC_String_UriErrorName);
    ecc_error_setupbo(&ECC_CtorFunc_ErrorEvalError, objerrorfn_ctorEvalError, 1, &ECC_Prototype_ErrorEvalError, &ECC_String_EvalErrorName);

    ecc_function_addto(ECC_Prototype_Error, "toString", objerrorfn_toString, 0, h);

    ecc_object_addmember(ECC_Prototype_Error, ECC_ConstKey_message, ecc_value_fromtext(&ECC_String_Empty), h);
}

void ecc_error_teardown(void)
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

eccobjerror_t* ecc_error_error(ecctextstring_t text, eccstrbuffer_t* message)
{
    return ecc_error_create(ECC_Prototype_Error, text, message);
}

eccobjerror_t* ecc_error_rangeerror(ecctextstring_t text, eccstrbuffer_t* message)
{
    return ecc_error_create(ECC_Prototype_ErrorRangeError, text, message);
}

eccobjerror_t* ecc_error_referenceerror(ecctextstring_t text, eccstrbuffer_t* message)
{
    return ecc_error_create(ECC_Prototype_ErrorReferenceError, text, message);
}

eccobjerror_t* ecc_error_syntaxerror(ecctextstring_t text, eccstrbuffer_t* message)
{
    return ecc_error_create(ECC_Prototype_ErrorSyntaxError, text, message);
}

eccobjerror_t* ecc_error_typeerror(ecctextstring_t text, eccstrbuffer_t* message)
{
    return ecc_error_create(ECC_Prototype_ErrorTypeError, text, message);
}

eccobjerror_t* ecc_error_urierror(ecctextstring_t text, eccstrbuffer_t* message)
{
    return ecc_error_create(ECC_Prototype_ErrorUriError, text, message);
}

eccobjerror_t* ecc_error_evalerror(ecctextstring_t text, eccstrbuffer_t* message)
{
    return ecc_error_create(ECC_Prototype_ErrorEvalError, text, message);
}

void ecc_error_destroy(eccobjerror_t* self)
{
    assert(self);
    if(!self)
        return;

    ecc_object_finalize(&self->object);

    free(self), self = NULL;
}
