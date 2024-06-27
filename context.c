//
//  context.c
//  libecc
//
//  Copyright (c) 2019 Aurélien Bouilland
//  Licensed under MIT license, see LICENSE.txt file in project root
//
#include "ecc.h"

// MARK: - Private

// MARK: - Static Members

// MARK: - Methods

static void ctxfn_rangeError(eccstate_t*, ecccharbuffer_t*) __attribute__((noreturn));
static void ctxfn_referenceError(eccstate_t*, ecccharbuffer_t*) __attribute__((noreturn));
static void ctxfn_syntaxError(eccstate_t*, ecccharbuffer_t*) __attribute__((noreturn));
static void ctxfn_typeError(eccstate_t*, ecccharbuffer_t*) __attribute__((noreturn));
static void ctxfn_uriError(eccstate_t*, ecccharbuffer_t*) __attribute__((noreturn));
static void ctxfn_throw(eccstate_t*, eccvalue_t) __attribute__((noreturn));
static eccvalue_t ctxfn_callFunction(eccstate_t*, eccobjfunction_t* function, eccvalue_t thisval, int argumentCount, ...);
static int ctxfn_argumentCount(eccstate_t*);
static eccvalue_t ctxfn_argument(eccstate_t*, int argumentIndex);
static void ctxfn_replaceArgument(eccstate_t*, int argumentIndex, eccvalue_t value);
static eccvalue_t ctxfn_this(eccstate_t*);
static void ctxfn_assertThisType(eccstate_t*, eccvaltype_t);
static void ctxfn_assertThisMask(eccstate_t*, eccvalmask_t);
static void ctxfn_assertThisCoerciblePrimitive(eccstate_t*);
static void ctxfn_setText(eccstate_t*, const ecctextstring_t* text);
static void ctxfn_setTexts(eccstate_t*, const ecctextstring_t* text, const ecctextstring_t* textAlt);
static void ctxfn_setTextIndex(eccstate_t*, eccctxindextype_t index);
static void ctxfn_setTextIndexArgument(eccstate_t*, int argument);
static ecctextstring_t ctxfn_textSeek(eccstate_t*);
static void ctxfn_rewindStatement(eccstate_t*);
static void ctxfn_printBacktrace(eccstate_t* context);
static eccobject_t* ctxfn_environmentRoot(eccstate_t* context);
const struct eccpseudonscontext_t ECCNSContext = {
    ctxfn_rangeError,     ctxfn_referenceError,
    ctxfn_syntaxError,    ctxfn_typeError,
    ctxfn_uriError,       ctxfn_throw,
    ctxfn_callFunction,   ctxfn_argumentCount,
    ctxfn_argument,       ctxfn_replaceArgument,
    ctxfn_this,           ctxfn_assertThisType,
    ctxfn_assertThisMask, ctxfn_assertThisCoerciblePrimitive,
    ctxfn_setText,        ctxfn_setTexts,
    ctxfn_setTextIndex,   ctxfn_setTextIndexArgument,
    ctxfn_textSeek,       ctxfn_rewindStatement,
    ctxfn_printBacktrace, ctxfn_environmentRoot,
    {}
};

void ctxfn_rangeError(eccstate_t* self, ecccharbuffer_t* chars)
{
    ctxfn_throw(self, ECCNSValue.error(ECCNSError.rangeError(ctxfn_textSeek(self), chars)));
}

void ctxfn_referenceError(eccstate_t* self, ecccharbuffer_t* chars)
{
    ctxfn_throw(self, ECCNSValue.error(ECCNSError.referenceError(ctxfn_textSeek(self), chars)));
}

void ctxfn_syntaxError(eccstate_t* self, ecccharbuffer_t* chars)
{
    ctxfn_throw(self, ECCNSValue.error(ECCNSError.syntaxError(ctxfn_textSeek(self), chars)));
}

void ctxfn_typeError(eccstate_t* self, ecccharbuffer_t* chars)
{
    ctxfn_throw(self, ECCNSValue.error(ECCNSError.typeError(ctxfn_textSeek(self), chars)));
}

void ctxfn_uriError(eccstate_t* self, ecccharbuffer_t* chars)
{
    ctxfn_throw(self, ECCNSValue.error(ECCNSError.uriError(ctxfn_textSeek(self), chars)));
}

void ctxfn_throw(eccstate_t * self, eccvalue_t value)
{
    if(value.type == ECC_VALTYPE_ERROR)
        self->ecc->text = value.data.error->text;

    if(self->ecc->printLastThrow && self->ecc->envCount == 1)
    {
        eccvalue_t name, message;
        name = ECCValConstUndefined;

        if(value.type == ECC_VALTYPE_ERROR)
        {
            name = ECCNSValue.toString(self, ECCNSObject.getMember(self, value.data.object, ECC_ConstKey_name));
            message = ECCNSValue.toString(self, ECCNSObject.getMember(self, value.data.object, ECC_ConstKey_message));
        }
        else
            message = ECCNSValue.toString(self, value);

        if(name.type == ECC_VALTYPE_UNDEFINED)
            name = ECCNSValue.text(&ECC_ConstString_ErrorName);

        ECCNSEnv.newline();
        ECCNSEnv.printError(ECCNSValue.stringLength(&name), ECCNSValue.stringBytes(&name), "%.*s", ECCNSValue.stringLength(&message), ECCNSValue.stringBytes(&message));
        ctxfn_printBacktrace(self);
        ECCNSScript.printTextInput(self->ecc, self->ecc->text, 1);
    }

    ECCNSScript.jmpEnv(self->ecc, value);
}

eccvalue_t ctxfn_callFunction(eccstate_t* self, eccobjfunction_t* function, eccvalue_t thisval, int argumentCount, ...)
{
    eccvalue_t result;
    va_list ap;
    int offset = 0;

    if(argumentCount & ECC_CTXSPECIALTYPE_ASACCESSOR)
    {
        offset = ECC_CTXOFFSET_ACCESSOR;
    }

    va_start(ap, argumentCount);
    result = ECCNSOperand.callFunctionVA(self, offset, function, thisval, argumentCount & ECC_CTXSPECIALTYPE_COUNTMASK, ap);
    va_end(ap);

    return result;
}

int ctxfn_argumentCount(eccstate_t* self)
{
    if(self->environment->hashmap[2].value.type == ECC_VALTYPE_OBJECT)
        return self->environment->hashmap[2].value.data.object->elementCount;
    else
        return self->environment->hashmapCount - 3;
}

eccvalue_t ctxfn_argument(eccstate_t* self, int argumentIndex)
{
    self->textIndex = argumentIndex + 4;

    if(self->environment->hashmap[2].value.type == ECC_VALTYPE_OBJECT)
    {
        if(argumentIndex < (int)self->environment->hashmap[2].value.data.object->elementCount)
            return self->environment->hashmap[2].value.data.object->element[argumentIndex].value;
    }
    else if(argumentIndex < (self->environment->hashmapCount - 3))
        return self->environment->hashmap[argumentIndex + 3].value;

    return ECCValConstNone;
}

void ctxfn_replaceArgument(eccstate_t* self, int argumentIndex, eccvalue_t value)
{
    if(self->environment->hashmap[2].value.type == ECC_VALTYPE_OBJECT)
    {
        if(argumentIndex < (int)self->environment->hashmap[2].value.data.object->elementCount)
            self->environment->hashmap[2].value.data.object->element[argumentIndex].value = value;
    }
    else if(argumentIndex < self->environment->hashmapCount - 3)
        self->environment->hashmap[argumentIndex + 3].value = value;
}

eccvalue_t ctxfn_this(eccstate_t* self)
{
    self->textIndex = ECC_CTXINDEXTYPE_THIS;
    return self->thisvalue;
}

void ctxfn_assertThisType(eccstate_t* self, eccvaltype_t type)
{
    if(self->thisvalue.type != type)
    {
        ctxfn_setTextIndex(self, ECC_CTXINDEXTYPE_THIS);
        ctxfn_typeError(self, ECCNSChars.create("'this' is not a %s", ECCNSValue.typeName(type)));
    }
}

void ctxfn_assertThisMask(eccstate_t* self, eccvalmask_t mask)
{
    if(!(self->thisvalue.type & mask))
    {
        ctxfn_setTextIndex(self, ECC_CTXINDEXTYPE_THIS);
        ctxfn_typeError(self, ECCNSChars.create("'this' is not a %s", ECCNSValue.maskName(mask)));
    }
}

void ctxfn_assertThisCoerciblePrimitive(eccstate_t* self)
{
    if(self->thisvalue.type == ECC_VALTYPE_UNDEFINED || self->thisvalue.type == ECC_VALTYPE_NULL)
    {
        ctxfn_setTextIndex(self, ECC_CTXINDEXTYPE_THIS);
        ctxfn_typeError(self, ECCNSChars.create("'this' cannot be null or undefined"));
    }
}

void ctxfn_setText(eccstate_t* self, const ecctextstring_t* text)
{
    self->textIndex = ECC_CTXINDEXTYPE_SAVED;
    self->text = text;
}

void ctxfn_setTexts(eccstate_t* self, const ecctextstring_t* text, const ecctextstring_t* textAlt)
{
    self->textIndex = ECC_CTXINDEXTYPE_SAVED;
    self->text = text;
    self->textAlt = textAlt;
}

void ctxfn_setTextIndex(eccstate_t* self, eccctxindextype_t index)
{
    self->textIndex = index;
}

void ctxfn_setTextIndexArgument(eccstate_t* self, int argument)
{
    self->textIndex = argument + 4;
}

ecctextstring_t ctxfn_textSeek(eccstate_t* self)
{
    const char* bytes;
    eccstate_t seek = *self;
    uint32_t breakArray = 0, argumentCount = 0;
    ecctextstring_t callText;
    eccctxindextype_t index;
    int isAccessor = 0;

    assert(self);
    assert(self->ecc);

    index = self->textIndex;

    if(index == ECC_CTXINDEXTYPE_SAVED)
        return *self->text;

    if(index == ECC_CTXINDECTYPE_SAVEDINDEXALT)
        return *self->textAlt;

    while(seek.ops->text.bytes == ECC_ConstString_NativeCode.bytes)
    {
        if(!seek.parent)
            return seek.ops->text;

        isAccessor = seek.argumentOffset == ECC_CTXOFFSET_ACCESSOR;

        if(seek.argumentOffset > 0 && index >= ECC_CTXINDEXTYPE_THIS)
        {
            ++index;
            ++argumentCount;
            breakArray <<= 1;

            if(seek.argumentOffset == ECC_CTXOFFSET_APPLY)
                breakArray |= 2;
        }
        seek = *seek.parent;
    }

    if(seek.ops->native == ECCNSOperand.noop)
        --seek.ops;

    if(isAccessor)
    {
        if(index > ECC_CTXINDEXTYPE_THIS)
            ECCNSContext.rewindStatement(&seek);
    }
    else if(index > ECC_CTXINDEXTYPE_NO)
    {
        while(seek.ops->text.bytes != seek.textCall->bytes || seek.ops->text.length != seek.textCall->length)
            --seek.ops;

        argumentCount += seek.ops->value.data.integer;
        callText = seek.ops->text;

        // func
        if(index-- > ECC_CTXINDEXTYPE_CALL)
            ++seek.ops;

        // this
        if(index-- > ECC_CTXINDEXTYPE_CALL && (seek.ops + 1)->text.bytes <= seek.ops->text.bytes)
            ++seek.ops;

        // arguments
        while(index-- > ECC_CTXINDEXTYPE_CALL)
        {
            if(!argumentCount--)
                return ECCNSText.make(callText.bytes + callText.length - 1, 0);

            bytes = seek.ops->text.bytes + seek.ops->text.length;
            while(bytes > seek.ops->text.bytes && seek.ops->text.bytes)
                ++seek.ops;

            if(breakArray & 0x1 && seek.ops->native == ECCNSOperand.array)
                ++seek.ops;

            breakArray >>= 1;
        }
    }

    return seek.ops->text;
}

void ctxfn_rewindStatement(eccstate_t* context)
{
    while(!(context->ops->text.flags & ECC_TEXTFLAG_BREAKFLAG))
        --context->ops;
}

void ctxfn_printBacktrace(eccstate_t* context)
{
    int depth = context->depth, count, skip;
    eccstate_t frame;

    if(depth > 12)
    {
        ECCNSEnv.printColor(0, ECC_ENVATTR_BOLD, "...");
        fprintf(stderr, " (%d more)\n", depth - 12);
        depth = 12;
    }

    while(depth > 0)
    {
        count = depth--;
        frame = *context;
        skip = 0;

        while(count--)
        {
            --skip;

            if(frame.argumentOffset == ECC_CTXOFFSET_CALL || frame.argumentOffset == ECC_CTXOFFSET_APPLY)
                skip = 2;
            else if(frame.textIndex > ECC_CTXINDEXTYPE_NO && frame.ops->text.bytes == ECC_ConstString_NativeCode.bytes)
                skip = 1;

            frame = *frame.parent;
        }

        if(skip <= 0 && frame.ops->text.bytes != ECC_ConstString_NativeCode.bytes)
        {
            ECCNSContext.rewindStatement(&frame);
            if(frame.ops->text.length)
                ECCNSScript.printTextInput(frame.ecc, frame.ops->text, 0);
        }
    }
}

eccobject_t* ctxfn_environmentRoot(eccstate_t* context)
{
    eccobject_t* environment = context->strictMode ? context->environment : &context->ecc->global->environment;

    if(context->strictMode)
        while(environment->prototype && environment->prototype != &context->ecc->global->environment)
            environment = environment->prototype;

    return environment;
}
